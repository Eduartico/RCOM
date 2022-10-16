// Application layer protocol implementation
#include <string.h>

#include "link_layer.h"
#include "application_layer.h"

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
        
    llclose(0);
}
