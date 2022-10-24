// Application layer protocol implementation
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "link_layer.h"
#include "application_layer.h"

#define C(n)    (0x00 + (n)) // Control field for application packet: 1 - data, 2 - start, 3 - end
#define MAX_BYTES_READ  MAX_PAYLOAD_SIZE - 4 // Max no. of bytes read allowed from a fread() call, 4 is the size of the data package header
typedef enum {SIZE, NAME} vtype_t; // T field for application packet

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParameters;
    strcpy(connectionParameters.serialPort, serialPort);
    if(strcmp(role, "tx") == 0) connectionParameters.role = LlTx;
    else connectionParameters.role = LlRx;
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout * 10;
    
    if(llopen(connectionParameters) == -1)
        return;
    
    switch(connectionParameters.role) {
        case LlTx: ;
            unsigned char ctrl_pkg[3] = {C(2), NAME, sizeof(filename)};
            strcat(ctrl_pkg, filename);
            for(int i = 0; i < sizeof(ctrl_pkg) + sizeof(filename); i++)
                printf("%02x ", ctrl_pkg[i]);
            printf("\n");
            llwrite(ctrl_pkg, sizeof(ctrl_pkg) + sizeof(filename));

            FILE* fptr = fopen(filename, "r");
            if(fptr == NULL) {
                perror("Filename doesn't exit.\n");
                return;
            }
            
            char data[MAX_PAYLOAD_SIZE - 4] = {0};
            int seq_n = 0;
            
            int bytes = fread(data, 1, MAX_BYTES_READ, fptr);
            while(bytes > 0) {

                unsigned int l1 = bytes % 256;
                unsigned int l2 = bytes / 256;
                char data_pkg_header[4] = {C(1), seq_n % 255, l2, l1};
                int data_pkg_size = sizeof(data_pkg_header) + bytes;
                
                unsigned char* data_pkg = malloc(data_pkg_size);
                memcpy(data_pkg, data_pkg_header, sizeof(data_pkg_header));
                memcpy(data_pkg + 4, data, bytes);
                
                /* 
                for(int i = 0; i < sizeof(data_pkg_header) + bytes; i++)
                    printf("%02x i = %d \n", data_pkg[i], i);
   
                printf("Bytes read = %d \n"
                       "L1 = %d ; L2 = %d \n"
                       "Sequence no. = %d \n", bytes, l1, l2, seq_n);
                */      
                llwrite(data_pkg, data_pkg_size);
                seq_n++;
                free(data_pkg);
                bytes = fread(data, 1, MAX_BYTES_READ, fptr);
            }
 
            fclose(fptr);
            
            ctrl_pkg[0] = C(3);
            
            //llwrite(ctrl_pkg, sizeof(ctrl_pkg) + sizeof(filename));
            break;
        case LlRx: ;
            unsigned char rcv_pkg[MAX_PAYLOAD_SIZE] = {0};
            int STOP = FALSE;
            FILE* fptrrx;
            while(STOP == FALSE) {
                memset(rcv_pkg, 0, MAX_PAYLOAD_SIZE);
                int ret = llread(rcv_pkg);
                if(ret == -1)
                    continue;
                switch(rcv_pkg[0]) {
                    case C(1): ;
                        //for(int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
                            //printf("%02x ", rcv_pkg[i]);
                        //}
                        int pkg_size = 256 * rcv_pkg[2] + rcv_pkg[3];
                        fwrite(&rcv_pkg[4], 1, pkg_size, fptrrx);
                        break;
                    case C(2): ;
                        int i = 1;
                        vtype_t param_type;
                        char file_name[50] = {0};
                        do {
                        param_type = rcv_pkg[i];
                        size_t param_length = rcv_pkg[i+1];
                        printf("param_type = %d \n, param_length = %d \n", param_type, param_length);
                        if(param_type == NAME)
                            strncpy(file_name, (rcv_pkg + (i+1) + 1), param_length);
                            
                        i += param_length + 1;
                        } while(param_type < 1);
                        
                        printf("filename = %s \n", file_name);
                        fptrrx = fopen(file_name, "w");
                        break;
                    case C(3):
                        printf("File closed \n");
                        STOP = TRUE;
                        fclose(fptrrx);
                        break;
                }
            }
            break;
    }
        
    llclose(0);
}
