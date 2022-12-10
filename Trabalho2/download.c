#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <string.h>

#define MAX_BUF_SIZE	1024
#define CTRL_PORT 		21
#define MAX_TIMEOUT		3

/*
int read_data_port(int sockfd, char* address[16]) {
	char buffer[MAX_BUF_SIZE] = {0};
	int bytes;
	char delim[] = "(),";
	char dot = '.';
	
	sleep(1);	// Delay execution to give server time to send data
	while((bytes = read(sockfd, buffer, MAX_BUF_SIZE)) > 0) {
    	printf("Bytes read: %d\n", bytes); 
    	printf("%s", buffer);
    }
    
    strtok(buffer, delim);
    //printf("%s\n", *address);
    printf("hello\n");
    strcat(*(address), strtok(NULL, delim));
    printf("%s\n", *address);
    strncat(*(address), &dot, 1);
    printf("%s\n", *address);
    strcat(*(address), strtok(NULL, delim));
    strncat(*(address), &dot, 1);
    printf("%s\n", address);
    printf("%s\n", strtok(NULL, delim));
    printf("%s\n", strtok(NULL, delim));
    
    
    int part1 = strtol(strtok(NULL, delim), NULL, 10);
    int part2 = strtol(strtok(NULL, delim), NULL, 10);
	int port = 256*part1 + part2;
	
    return port;
}   
*/

void read_data_socket(int sockfd, char* filepath, int filesize) {
	FILE* fileptr;
	
	char* tok = strtok(filepath, "/");
	char filename[128] = {0};
	while((tok = strtok(NULL, "/")) != NULL)
		strcpy(filename, tok);
		
	fileptr = fopen(filename, "w");
	
	//read from socket and write to file
	char buffer[MAX_BUF_SIZE] = {0};
	memset(buffer, 0, MAX_BUF_SIZE);
	
	int current_filesize = filesize;
	int condition;
	int bytes;
	
	while(current_filesize > 0) {
		if(current_filesize >= MAX_BUF_SIZE) {
			condition = MAX_BUF_SIZE;
			current_filesize -= MAX_BUF_SIZE;
		}
		else {
			condition = current_filesize;
			current_filesize = 0;
		}
		
		bytes = read(sockfd, buffer, MAX_BUF_SIZE);
		fwrite(buffer, 1, bytes, fileptr);
		
	}
	
	fclose(fileptr);
}

void read_socket(int sockfd) {	
	char buffer[MAX_BUF_SIZE] = {0};
	int bytes;
	
	sleep(1);	// Delay execution to give server time to send data
	while((bytes = read(sockfd, buffer, MAX_BUF_SIZE)) > 0) {
    	//printf("Bytes read: %d\n", bytes); 
    	printf("%s", buffer);
    }
}

void write_socket(int sockfd, char* command) {	
	int bytes = write(sockfd, command, strlen(command));
    write(sockfd, "\n", 1);
    if (bytes > 0) {
        //printf("Bytes written: %d\n", bytes+1);
        printf("%s\n", command);
    } else {
        perror("write()");
        exit(-1);
    }
}

int main(int argc, char *argv[]) {
    struct hostent *h;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <address to get IP address>\n", argv[0]);
        exit(-1);
    }
	
	/*parsing the argument*/
	char delim[] = "[]:/";
	strtok(argv[1], delim);
	char* username = strtok(NULL, delim);
	char* password = strtok(NULL, delim);
	char* hostname = strtok(NULL, delim);
	char* urlpath = strtok(NULL, ""); 
	printf("Username: %s\nPassword: %s\nHost: %s\nPath: %s\n", username, password, hostname, urlpath);
	
	
    if ((h = gethostbyname(hostname)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));
    printf("\n");
       
    int sockfd;
    struct sockaddr_in server_addr;
    
    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) h->h_addr)));    /*32 bit Internet address network byte ordered*/

    server_addr.sin_port = htons(CTRL_PORT);        /*server TCP port must be network byte ordered */
    
    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
      
    /*put the socket into non-blocking mode */
    int block_mode = 1;	// (1 = non-blocking, 0 = blocking)
    int rc;
    if((rc = ioctl(sockfd, FIONBIO, (char *) &block_mode)) == -1) {
    	perror("ioctl()");
    	exit(-1);
    }
    
    /*read data from server*/
    char buffer[MAX_BUF_SIZE] = {0};
    int bytes;
    //int count = 0;
    //while(timeout < MAX_TIMEOUT) {
    //	bytes = read(sockfd, buffer, MAX_BUF_SIZE);
    //	printf("%d\n",bytes);
    //	if(bytes != -1) {
    //		//printf("%d\nCount: %d\n", bytes, count);
    //		count = 0; }
    //	else {
    //		count++;
    //		sleep(0.5);
    //		}
    //}
	//int responses = 0;
	//sleep(1);	// Pause program while connection is established
	//(bytes = read(sockfd, buffer, MAX_BUF_SIZE)) > 0
    //while((bytes = read(sockfd, buffer, MAX_BUF_SIZE)) > 0) {
    //	printf("Bytes read: %d\n", bytes); 
    //	printf("%s", buffer);
    	//if(strstr(buffer, "\r\n") != NULL)
    	//	responses++;
    //}
    
	read_socket(sockfd);
	
    /*send a string to the server*/
    char command[] = "USER ";
    strcat(command, username);
    write_socket(sockfd, command);
    
    read_socket(sockfd);
    
    memset(command, 0, sizeof(command));
    strcpy(command, "PASS ");
    strcat(command, password);    
    write_socket(sockfd, command);
    
    read_socket(sockfd);
    
    memset(command, 0, sizeof(command));
    strcpy(command, "PASV");
    write_socket(sockfd, command);

// GET DATA PORT   
    char data_address[16] = {0};
    //int data_port = read_data_port(sockfd, &data_address);
    //printf("%d\n", data_port);
    
	char delim2[] = "(),";
	char dot = '.';
	
	memset(buffer, 0, sizeof(buffer));
	sleep(1);	// Delay execution to give server time to send data
	while((bytes = read(sockfd, buffer, MAX_BUF_SIZE)) > 0) {
    	//printf("Bytes read: %d\n", bytes); 
    	printf("%s", buffer);
    }
    
    strtok(buffer, delim2);
    strcat(data_address, strtok(NULL, delim2));
    strncat(data_address, &dot, 1);
    strcat(data_address, strtok(NULL, delim2));
    strncat(data_address, &dot, 1);
    strcat(data_address, strtok(NULL, delim2));
    strncat(data_address, &dot, 1);
    strcat(data_address, strtok(NULL, delim2));

    int part1 = strtol(strtok(NULL, delim2), NULL, 10);
    int part2 = strtol(strtok(NULL, delim2), NULL, 10);
	int data_port = 256*part1 + part2;
	
	//printf("Address: %s\n", data_address);
	//printf("Port: %d\n", data_port);
	
	/*build data socket*/
	int data_sockfd;
    struct sockaddr_in data_server_addr;
//---

// CONNECT DATA PORT    
    /*server address handling*/
    bzero((char *) &data_server_addr, sizeof(data_server_addr));
    data_server_addr.sin_family = AF_INET;
    data_server_addr.sin_addr.s_addr = inet_addr(data_address);    /*32 bit Internet address network byte ordered*/

    data_server_addr.sin_port = htons(data_port);        /*server TCP port must be network byte ordered */
    
    /*open a TCP socket*/
    if ((data_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    
    /*connect to the server*/
    if (connect(data_sockfd,
                (struct sockaddr *) &data_server_addr,
                sizeof(data_server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
//---

	memset(command, 0, sizeof(command));
    strcpy(command, "SIZE ");
    strcat(command, urlpath); 
    write_socket(sockfd, command);
       
    memset(buffer, 0, sizeof(buffer));
	sleep(1);	// Delay execution to give server time to send data
	while((bytes = read(sockfd, buffer, MAX_BUF_SIZE)) > 0) {
    	//printf("Bytes read: %d\n", bytes); 
    	printf("%s", buffer);
    }
    
    strtok(buffer, " ");
    int filesize = strtol(strtok(NULL, "\n"), NULL, 10);
    
	memset(command, 0, sizeof(command));
    strcpy(command, "RETR ");
    strcat(command, urlpath); 
    write_socket(sockfd, command);
       
    read_socket(sockfd);
    read_data_socket(data_sockfd, urlpath, filesize);
    
    memset(command, 0, sizeof(command));
    strcpy(command, "QUIT");
    write_socket(sockfd, command);
    
    read_socket(sockfd);
    
    /*close sockets*/
    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }
    
    if (close(data_sockfd)<0) {
        perror("close()");
        exit(-1);
    }
    
    return 0;
}
