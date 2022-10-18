// Link layer protocol implementation
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FLAG    0x7E        // Flag
#define A_TR    0x03        // Address field in commands sent by the Transmitter and replies sent by the Receiver
#define A_RC    0x01        // Address field in commands sent by the Receiver and replies sent by the Transmitter
#define C_SET   0x03        // Control field for set up (SET)
#define C_UA    0x07        // Control field for unnumbered acknowledgment (UA)
#define C_DISC  0x0E        // Control field for disconection (DISC)
typedef enum {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK} readState_t;

volatile int STOP = FALSE;
static readState_t read_state = START;
static LinkLayer protocol;
static int fd;
static struct termios oldtio;
static struct termios newtio;
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    protocol = connectionParameters;
    
    fd = open(protocol.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(protocol.serialPort);
        return -1;
    }
    
    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1) {
        perror("tcgetattr");
        return -1;
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = protocol.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    if(protocol.role == LlTx) {
        newtio.c_cc[VTIME] = protocol.timeout; // Inter-character timer used
        newtio.c_cc[VMIN] = 0;  // Blocking read until 1 chars received
    } else {
        newtio.c_cc[VTIME] = 0; // Inter-character timer unused
        newtio.c_cc[VMIN] = 1;  // Blocking read until 1 chars received
    }

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return -1;
    }
    
    switch(protocol.role) {
        case(LlTx): ;
        char SET[] = {FLAG, A_TR, C_SET, A_TR^C_SET, FLAG};
        
        // Write the frame to the serial port
        int bytes = write(fd, SET, sizeof(SET));
    
        // Wait until all bytes have been written to the serial port
        sleep(0.1);
        
        // Read back from the receiver
        unsigned char buf[MAX_PAYLOAD_SIZE] = {0};
        read_state = START;
        int retransmissions = protocol.nRetransmissions;
        do {
            int bytes = read(fd, buf, 1);
                    
            if (bytes != 0) {
                switch(read_state) {
                    case START:
                        if (buf[0] == FLAG) {
                            read_state = FLAG_RCV;
                            printf("Flag byte 1 received.\n");
                        }
                        break;
                    case FLAG_RCV:
                        if (buf[0] == A_TR) {
                            read_state = A_RCV;
                            printf("A byte received.\n");
                        }
                        else if(buf[0] != FLAG)
                            read_state = START;
                        break;
                    case A_RCV:
                        if(buf[0] == C_UA) {
                            read_state = C_RCV;
                            printf("C byte received.\n");
                        }
                        else if (buf[0] != FLAG)
                            read_state = START;
                        else
                            read_state = FLAG_RCV;
                        break;
                    case C_RCV:
                        if (buf[0] == (A_TR^C_UA)) {
                            read_state = BCC_OK;
                            printf("BCC byte received.\n");
                        }
                        else if (buf[0] != FLAG)
                            read_state = START;
                        else
                            read_state = FLAG_RCV;
                        break;
                    case BCC_OK:
                        if (buf[0] == FLAG) {
                            STOP = TRUE;
                            printf("Flag byte 2 received.\n");
                        }
                        else
                            read_state = START;
                        break;
                    default:
                        break;
                }
                continue;
            }
                
            retransmissions--;
            printf("Time out. Attempts left: %d\n", retransmissions);
            read_state = START;
            write(fd, SET, sizeof(SET));
        } while (STOP == FALSE && retransmissions > 0);
        break;
        case(LlRx): ;
        read_state = START;
        do {
            int bytes = read(fd, buf, 1);
            switch(read_state) {
                case START:
                    if (buf[0] == FLAG) {
                        read_state = FLAG_RCV;
                        printf("Flag byte 1 received.\n");
                    }
                    break;
                case FLAG_RCV:
                    if (buf[0] == A_TR) {
                        read_state = A_RCV;
                        printf("A byte received.\n");
                    }
                    else if(buf[0] != FLAG)
                        read_state = START;
                    break;
                case A_RCV:
                    if(buf[0] == C_UA) {
                        read_state = C_RCV;
                        printf("C byte received.\n");
                    }
                    else if (buf[0] != FLAG)
                        read_state = START;
                    else
                        read_state = FLAG_RCV;
                    break;
                case C_RCV:
                    if (buf[0] == (A_TR^C_SET)) {
                        read_state = BCC_OK;
                        printf("BCC byte received.\n");
                    }
                    else if (buf[0] != FLAG)
                        read_state = START;
                    else
                        read_state = FLAG_RCV;
                    break;
                case BCC_OK:
                    if (buf[0] == FLAG) {
                        STOP = TRUE;
                        printf("Flag byte 2 received.\n");
                    }
                    else
                        read_state = START;
                    break;
                default:
                    break;
            }
        } while (STOP == FALSE);
            
        char UA[] = {FLAG, A_TR, C_UA, A_TR^C_UA, FLAG};
        write(fd, UA, sizeof(UA));
        break;
    }
    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO
    printf("Size = %d \n", bufSize);
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO
    packet[0] = 2;
    packet[1] = 1;
    packet[2] = 7;
    strcpy((packet + 3), "penguin");
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{

    switch(protocol.role) {
        case(LlTx): ;
            const unsigned char DISC_TX[] = {FLAG, A_TR, C_DISC, (A_TR^C_DISC), FLAG};
            write(fd, DISC_TX, sizeof(DISC_TX));
             
            // Read back from the receiver
            unsigned char buf[MAX_PAYLOAD_SIZE] = {0};
            read_state = START;
            STOP = FALSE;
            int retransmissions = protocol.nRetransmissions;
            do {
                int bytes = read(fd, buf, 1);    
                if (bytes != 0) {
                    switch(read_state) {
                        case START:
                            if (buf[0] == FLAG) {
                                read_state = FLAG_RCV;
                                printf("Flag byte 1 received.\n");
                            }
                            break;
                        case FLAG_RCV:
                            if (buf[0] == A_TR) {
                                read_state = A_RCV;
                                printf("A byte received.\n");
                            }
                            else if(buf[0] != FLAG)
                                read_state = START;
                            break;
                        case A_RCV:
                            if(buf[0] == C_DISC) {
                                read_state = C_RCV;
                                printf("C byte received.\n");
                            }
                            else if (buf[0] != FLAG)
                                read_state = START;
                            else
                                read_state = FLAG_RCV;
                            break;
                        case C_RCV:
                            if (buf[0] == (A_TR^C_DISC)) {
                                read_state = BCC_OK;
                                printf("BCC byte received.\n");
                            }
                            else if (buf[0] != FLAG)
                                read_state = START;
                            else
                                read_state = FLAG_RCV;
                            break;
                        case BCC_OK:
                            if (buf[0] == FLAG) {
                                STOP = TRUE;
                                printf("Flag byte 2 received.\n");
                            }
                            else
                                read_state = START;
                            break;
                        default:
                            break;
                    }
                    continue;
                }
                    
                retransmissions--;
                printf("Time out. Attempts left: %d\n", retransmissions);
                read_state = START;
                write(fd, DISC_TX, sizeof(DISC_TX));
            } while (STOP == FALSE && retransmissions > 0);
               
            const unsigned char UA[] = {FLAG, A_RCV, C_UA, A_RCV^C_UA, FLAG};
            write(fd, UA, sizeof(UA));
            
            break;
        case(LlRx): ;
            read_state = START;
            do {
                int bytes = read(fd, buf, 1);
                switch(read_state) {
                    case START:
                        if (buf[0] == FLAG) {
                            read_state = FLAG_RCV;
                            printf("Flag byte 1 received.\n");
                        }
                        break;
                    case FLAG_RCV:
                        if (buf[0] == A_TR) {
                            read_state = A_RCV;
                            printf("A byte received.\n");
                        }
                        else if(buf[0] != FLAG)
                            read_state = START;
                        break;
                    case A_RCV:
                        if(buf[0] == C_DISC) {
                            read_state = C_RCV;
                            printf("C byte received.\n");
                        }
                        else if (buf[0] != FLAG)
                            read_state = START;
                        else
                            read_state = FLAG_RCV;
                        break;
                    case C_RCV:
                        if (buf[0] == (A_TR^C_DISC)) {
                            read_state = BCC_OK;
                            printf("BCC byte received.\n");
                        }
                        else if (buf[0] != FLAG)
                            read_state = START;
                        else
                            read_state = FLAG_RCV;
                        break;
                    case BCC_OK:
                        if (buf[0] == FLAG) {
                            STOP = TRUE;
                            printf("Flag byte 2 received.\n");
                        }
                        else
                            read_state = START;
                        break;
                    default:
                        break;
                }
            } while (STOP == FALSE);
                
            char DISC_RX[] = {FLAG, A_RCV, C_DISC, A_RCV^C_DISC, FLAG};
            write(fd, DISC_RX, sizeof(DISC_RX));
            break;       
    }
    
    // Restore the old port settings
    //if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    //{
    //    perror("tcsetattr");
    //    exit(-1);
    //}
    
    //printf("Closing.\n");
    //close(fd);
    return 1;
}
