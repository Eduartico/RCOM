#define MAX_SIZE 256

// Field frame values
#define FLAG    0x7E        // Flag
#define A_TR    0x03        // Address field in commands sent by the Transmitter and replies sent by the Receiver
#define A_RC    0x01        // Address field in commands sent by the Receiver and replies sent by the Transmitter
#define C_SET   0x03        // Control field for set up (SET)
#define C_UA    0x07        // Control field for unnumbered acknowledgment (UA)

// Frame formats
const unsigned char SET[] = {FLAG, A_TR, C_SET, A_TR^C_SET, FLAG};
const unsigned char UA[] = {FLAG, A_TR, C_UA, A_TR^C_UA, FLAG};

// State enum for state machine
typedef enum {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK} state_t;

// Format type enum
typedef enum {INFO, SUPER, UNNUM} format_t;

// Protocol data structure
typedef struct linkLayer {
    char port[20];                  // Device /dev/ttySx, x = 0,1,...
    unsigned int sequenceNumber;    // Frame sequence number
    unsigned int timeout;           // Timer value: 0.1s 
    unsigned int numTransmissions;  // Number of retries in case of failure
    char frame[MAX_SIZE];           // Frame
} linkLayer_t;

// Data link functions
int setup_port();
int new_port_settings(int fd);
int make_frame(format_t type, int cmd);
int change_read_state(char byte);
int fd_read(int fd, char* buffer);
int fd_write(int fd, const char* buffer, int length);

// Application functions
int llopen(char* port);
