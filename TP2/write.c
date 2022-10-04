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

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256


// SET buffer values
#define FLAG 0x7E
#define A    0x03
#define C    0x03
#define BCC  (A^C)

volatile int STOP = FALSE;

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 30; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 1 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Create string to send
    unsigned char buf[BUF_SIZE] = {0};
    //unsigned char test[] = "Hello world.";
    unsigned char buffer[] = {FLAG, A, BCC, BCC, FLAG};
    /*
    for (int i = 0; i < BUF_SIZE; i++)
    {
        buf[i] = 'a' + i % 26;
    }
    */

    int bytes = write(fd, buffer, sizeof(buffer));
    printf("%d bytes written\n", bytes);
    
    sleep(1);
    
    // Read back from the receiver
    memset(buf, 0, BUF_SIZE);
    unsigned char cmp[] = {FLAG, A, C, BCC, FLAG}; // Comparison buffer
    int i = 0;
    
    while (STOP == FALSE)
    {         
        // Returns after 1 chars have been input
        int bytes = read(fd, &buf[i], 1);
        if(bytes == 0) {
            write(fd, buffer, sizeof(buffer));
            sleep(1);    
        }
        
        if(buf[i] == cmp[i])
            i++;
        else {
            i = 0;
            memset(buf, 0, sizeof(buf));
        }
        if(i >= 5)
            STOP = TRUE;
        
        buf[i+1] = '\0'; // Set end of string to '\0', so we can printf

        printf(":%2x:%d\n", buf[i-1], bytes);
        /*
        if (buf[i] == '.')
            STOP = TRUE;
        else
            i++;
        */
            
    }
    printf("Readback successful\n");
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
    
    printf("Closing\n");
    close(fd);

    return 0;
}
