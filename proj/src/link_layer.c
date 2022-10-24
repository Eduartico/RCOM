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
#define FLAG     0x7e        // Flag
#define A_TR     0x03        // Address field in commands sent by the Transmitter and replies sent by the Receiver
#define A_RC     0x01        // Address field in commands sent by the Receiver and replies sent by the Transmitter
#define C_SET    0x03        // Control field for set up (SET)
#define C_UA     0x07        // Control field for unnumbered acknowledgment (UA)
#define C_DISC   0x0b        // Control field for disconection (DISC)
#define C_RR(n)  (((0x00 + (n)) << 7) + 0x05) // Control field for frame I positive acknowledgement (RR)
#define C_REJ(n) (((0x00 + (n)) << 7) + 0x01) // Control field for frame I rejection (REJ)
#define C_SEQ(n) ((0x00 + (n)) << 6) // Control field for data frame, n = sequence number
#define ESC_OCT  0x7d        // Escape character
#define BYTE_ST  0x20        // Byte to perform the XOR comparison with the stuffed data byte

typedef enum {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, DATA, STUFF, BCC2_OK} readState_t;

volatile int STOP = FALSE;
static readState_t read_state = START;
static LinkLayer protocol;
static int fd;
static int n_seq;
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
        if(retransmissions == 0)
            return -1;
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
                    if(buf[0] == C_SET) {
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
    unsigned char frame[2 * MAX_PAYLOAD_SIZE + 7] = {0}; // +7 to account for the other fields besides the data field, payload size doubled to account for stuffing
    frame[0] = FLAG;
    frame[1] = A_TR;
    frame[2] = C_SEQ(n_seq);
    frame[3] = frame[1]^frame[2];
    
    int i; // frame index
    int offset = 0; // offset index (in case of byte stuffing)
    int bcc2 = 0;
    for(i = 0; i < bufSize; i++) {
        if(buf[i] == FLAG) {
            frame[i+4] = ESC_OCT;
            offset++;
            frame[i+4+offset] = buf[i]^BYTE_ST;
        } else if (buf[i] == ESC_OCT) {
            frame[i+4] = ESC_OCT;
            offset++;
            frame[i+4+offset] = buf[i]^BYTE_ST;
        } else
            frame[i+4+offset] = buf[i]; // add 4 to i as data field starts at index 4
        bcc2 = bcc2^buf[i];
    }
    
    i = i + 4 + offset; // Advance i to current frame index
    if(bcc2 == FLAG) {
        frame[i] = ESC_OCT;
        frame[i+1] = bcc2^BYTE_ST;
    } else if (bcc2 == ESC_OCT) {
        frame[i] = ESC_OCT;
        frame[i+1] = bcc2^BYTE_ST;
    }
    else frame[i] = bcc2;
    frame[i+1] = FLAG; 
    
    for(int j = 0; j <= i+1; j++) {
        printf("%02x ", frame[j]);
    }
    
    write(fd, frame, i+1);
    /*
    switch(read_state) {
        case START:
            if (byte == FLAG) {
                read_state = FLAG_RCV;
            }
            break;
        case FLAG_RCV:
            if (byte == A_TR) {
                read_state = A_RCV;
            }
            else if(byte != FLAG)
                read_state = START;
            break;
        case A_RCV:
            if(byte == C_SET) {
                read_state = C_RCV;
            }
            else if (byte != FLAG)
                read_state = START;
            else
                read_state = FLAG_RCV;
            break;
        case C_RCV:
            if (byte == A_TR^C_SET) {
                read_state = BCC_OK;
            }
            else if (byte != FLAG)
                read_state = START;
            else
                read_state = FLAG_RCV;
            break;
        case BCC_OK:
            if (byte == FLAG) {
                STOP = TRUE;
            }
            else
                read_state = START;
            break;
        default:
            break;
        }
        */
    n_seq = n_seq^1;
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    read_state = START;
    STOP = FALSE;
   
    int i = 0;  // current index
    int bcc2 = 0;
    do {
        int bytes = read(fd, &packet[i], 1);
        switch(read_state) {
            case START:
                if (packet[i] == FLAG) {
                    read_state = FLAG_RCV;
                    printf("Flag I received \n");
                }
                break;
            case FLAG_RCV:
                if (packet[i] == A_TR) {
                    read_state = A_RCV;
                    printf("A I received \n");
                }
                else if(packet[i] != FLAG) {
                    read_state = START;
                    i = 0;
                    memset(packet, 0, MAX_PAYLOAD_SIZE);
                }
                break;
            case A_RCV:
                if(packet[i] == C_SEQ(n_seq)) {
                    read_state = C_RCV;
                    printf("C I received \n");
                }
                else if (packet[i] == C_SEQ(n_seq^1)) { // received duplicate frame
                    tcflush(fd, TCIOFLUSH);
                    n_seq = n_seq^1;
                    unsigned char RR[] = {FLAG, A_TR, C_RR(n_seq), A_TR^C_RR(n_seq), FLAG};
                    write(fd, RR, sizeof(RR));
                    return -1;
                }
                else if (packet[i] != FLAG) {
                    read_state = START;
                }
                else {
                    read_state = FLAG_RCV;
                }
                break;
            case C_RCV:
                if (packet[i] == A_TR^C_SEQ(n_seq)) {
                    read_state = DATA;
                    printf("BCC byte received \n");
                }
                else if (packet[i] != FLAG) {
                    read_state = START;
                }
                else {
                    read_state = FLAG_RCV;
                }
                break;
            case DATA:
                if(packet[i] == FLAG) {
                    if(bcc2 == 0)
                        STOP = TRUE;
                    else {
                        printf("BCC2 error, rejected. \n");
                        tcflush(fd, TCIOFLUSH);
                        char REJ[] = {FLAG, A_TR, C_REJ(n_seq), A_TR^C_REJ(n_seq), FLAG};
                        write(fd, REJ, sizeof(REJ));
                        sleep(1);
                        return -1;
                    }
                }
                else if(packet[i] == ESC_OCT) {
                    read_state = STUFF;
                    continue;
                } 
                else {
                    bcc2 = bcc2^packet[i];
                    i++;
                }
                break;
            case STUFF:
                    if(packet[i] == 0x5e){
                        packet[i] = FLAG;
                        if(bcc2 == 0) {
                            read_state = BCC2_OK;
                        }
                        
                    }
                    else if (packet[i] == 0x5d) {
                        packet[i] = ESC_OCT;
                        if(bcc2 == 0) {
                            read_state = BCC2_OK;
                        }
                    }
                    else {
                        tcflush(fd, TCIOFLUSH);
                        char REJ[] = {FLAG, A_TR, C_REJ(n_seq), A_TR^C_REJ(n_seq), FLAG};
                        write(fd, REJ, sizeof(REJ));
                    }
                    bcc2 = bcc2^packet[i];
                    i++;
                    read_state = DATA;  // back to normal
                break;
            default:
                break;
            }
    } while (STOP == FALSE);
    
    n_seq = n_seq^1;
    unsigned char RR[] = {FLAG, A_TR, C_RR(n_seq), A_TR^C_RR(n_seq), FLAG};
    write(fd, RR, sizeof(RR));
    // Reset flag and bcc2 bytes
    packet[i] = 0;
    packet[i-1] = 0;
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
               
            const unsigned char UA[] = {FLAG, A_RC, C_UA, A_RC^C_UA, FLAG};
            write(fd, UA, sizeof(UA));
            
            break;
        case(LlRx): ;
            read_state = START;
            STOP = FALSE;
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
            
            char DISC_RX[] = {FLAG, A_RC, C_DISC, A_RC^C_DISC, FLAG};
            write(fd, DISC_RX, sizeof(DISC_RX));
            read_state = START;
            STOP = FALSE;
            retransmissions = protocol.nRetransmissions;
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
                        if (buf[0] == A_RC) {
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
                        if (buf[0] == (A_RC^C_UA)) {
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
            write(fd, DISC_RX, sizeof(DISC_RX));
        } while (STOP == FALSE && retransmissions > 0);
            break;       
    }
    
    //Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
    
    printf("Closing.\n");
    close(fd);
    return 1;
}
