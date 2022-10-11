// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "app.h"

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

static linkLayer_t protocol;
static state_t read_state = START;

static struct termios oldtio;
static struct termios newtio;

volatile int STOP = FALSE;

int setup_port() {
    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(protocol.port, O_RDWR | O_NOCTTY);

    if (fd < 0) {
        perror(protocol.port);
        return -1;
    }
    
    if(new_port_settings(fd) == -1)
        return -1;

    printf("New termios structure set\n");
    return fd;
}

int new_port_settings(int fd) {

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1) {
        perror("tcgetattr");
        return -1;
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 1 char received

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
    
    return 0;
}

framei_t make_framei(framei_t frame) {
    frame.;
}

int change_read_stateSU(char byte) {
    switch(read_state) {
        case START:
            if (byte == FLAG) {
                read_state = FLAG_RCV;
                printf("Flag byte 1 received.\n");
            }
            break;
        case FLAG_RCV:
            if (byte == A_TR) {
                read_state = A_RCV;
                printf("A byte received.\n");
            }
            else if(byte != FLAG)
                read_state = START;
            break;
        case A_RCV:
            if(byte == C_SET) {
                read_state = C_RCV;
                printf("C byte received.\n");
            }
            else if (byte != FLAG)
                read_state = START;
            else
                read_state = FLAG_RCV;
            break;
        case C_RCV:
            if (byte == A_TR^C_SET) {
                read_state = BCC_OK;
                printf("BCC byte received.\n");
            }
            else if (byte != FLAG)
                read_state = START;
            else
                read_state = FLAG_RCV;
            break;
        case BCC_OK:
            if (byte == FLAG) {
                STOP = TRUE;
                printf("Flag byte 2 received.\n");
            }
            else
                read_state = START;
            break;
        default:
            break;
        }
}

int change_read_stateI(char byte, int n) {
    switch(read_state) {
        case START:
            if (byte == FLAG) {
                read_state = FLAG_RCV;
                printf("Flag byte 1 received.\n");
            }
            break;
        case FLAG_RCV:
            if (byte == A_TR) {
                read_state = A_RCV;
                printf("A byte received.\n");
            }
            else if(byte != FLAG)
                read_state = START;
            break;
        case A_RCV:
            if(byte == C_I(n) {
                read_state = C_RCV;
                printf("C byte received.\n");
            }
            else if (byte != FLAG)
                read_state = START;
            else
                read_state = FLAG_RCV;
            break;
        case C_RCV:
            if (byte == A_TR^C_I(n)) {
                read_state = BCC1_OK;
                printf("BCC1 byte received.\n");
            }
            else if (byte != FLAG)
                read_state = START;
            else
                read_state = FLAG_RCV;
            break;
        case BCC1_OK:
            if (byte == BCC2) {
                STOP = TRUE;
                printf("Flag byte 2 received.\n");
            }
            else
                read_state = START;
            break;
        default:
            break;
        }
}

int fd_read(int fd, char* buffer) {
    int bytes = read(fd, buffer, 1);
    
    if(!bytes) return -1;
    
    change_read_state(buffer[0]);
    
    return bytes;
}

int fd_write(int fd, const char* buffer, int length) {
    // Copy buffer to protocol's frame
    memcpy(protocol.frame, buffer, length);
    
    // Write the frame to the serial port
    int bytes = write(fd, protocol.frame, length);
    
    // Wait until all bytes have been written to the serial port
    sleep(0.1);
    
    if(bytes < 0)
        return -1;
        
    return bytes;   
}

int llopen(char* port) {
    
    // Setting values for protocol data structure
    strcpy(protocol.port, port);
    protocol.sequenceNumber = 0;
    protocol.timeout = 30;
    protocol.numTransmissions = 3;
    
    int fd = setup_port();
    if(fd < 0)
        exit(-1);

    unsigned char buf[MAX_SIZE] = {0};
    
    printf("Waiting to receive SET frame...\n");        
    do {
        int bytes = fd_read(fd, buf);
    } while (STOP == FALSE);
    
    printf("SET frame received. Sending UA frame...\n");
    fd_write(fd, UA, sizeof(UA));

    return fd;
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    if (argc < 2) {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1 0\n",
               argv[0],
               argv[0]);
        exit(1);
    }
       
    int fd = llopen(argv[1]);
    
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
    
    printf("Closing.\n");
    close(fd);

    return 0;
}
