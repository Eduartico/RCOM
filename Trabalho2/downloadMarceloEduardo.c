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

/* Read from data socket */
long int read_data_socket(int data_sockfd, char* filepath, long int filesize) {
	FILE* fileptr;
	char filename[128] = {0};
    
    if(strchr(filepath, '/') == NULL) 
        strcpy(filename, filepath);
    else {
        char* tok = strtok(filepath, "/");
	    while((tok = strtok(NULL, "/")) != NULL)
		    strcpy(filename, tok);
    }
    printf("filename %s\n", filename);
	fileptr = fopen(filename, "w");
	
	/*read from socket and write to file*/
	char buffer[MAX_BUF_SIZE] = {0};

	int bytes;
	long int totalbytes = 0;
	while(filesize > 0) {
		memset(buffer, 0, MAX_BUF_SIZE);
		bytes = read(data_sockfd, buffer, MAX_BUF_SIZE);
		//printf("Bytes received: %d\n", bytes);

		fwrite(buffer, 1, bytes, fileptr);
		filesize -= bytes;
		totalbytes += bytes;

	}

	fclose(fileptr);
	//printf("Bytes left: %ld\n", current_filesize);
	return totalbytes;
}

/* Read from socket */
void read_socket(int sockfd) {	
	char buffer[MAX_BUF_SIZE] = {0};
	int bytes;
	
	sleep(1);	// Delay execution to give server time to send data
	while((bytes = read(sockfd, buffer, MAX_BUF_SIZE)) > 0) {
    	//printf("Bytes read: %d\n", bytes); 
    	printf("%s", buffer);
    }
}

/* Write to socket */
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
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
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
	
// --- CTRL CONNECTION SETUP
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
// ---
    
    /*read welcome message*/
    char buffer[MAX_BUF_SIZE] = {0};
    int bytes;
      
	read_socket(sockfd);
	
    /*login to server*/
    char command[] = "USER ";
    strcat(command, username);
    write_socket(sockfd, command);
    
    read_socket(sockfd);
    
    memset(command, 0, sizeof(command));
    strcpy(command, "PASS ");
    strcat(command, password);    
    write_socket(sockfd, command);
    
    read_socket(sockfd);
    
    /*enter passive mode*/
    memset(command, 0, sizeof(command));
    strcpy(command, "PASV");
    write_socket(sockfd, command);

// --- GET DATA PORT   
    char data_address[16] = {0};   
	char delim2[] = "(),";
	char dot = '.';
	
	/*read response from server after entering pass mode*/
	memset(buffer, 0, sizeof(buffer));
	sleep(1);	// Delay execution to give server time to send data
	while((bytes = read(sockfd, buffer, MAX_BUF_SIZE)) > 0) {
    	//printf("Bytes read: %d\n", bytes); 
    	printf("%s", buffer);
    }
    
    /* parse response for ip address and port*/
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
// ---

// --- CONNECT DATA PORT    
	/*build data socket*/
	int data_sockfd;
    struct sockaddr_in data_server_addr;
    
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
// ---

	/*get size of file to download*/
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
    
    /*parse response from server to retrive file size */
    strtok(buffer, " ");
    long int filesize = strtol(strtok(NULL, "\n"), NULL, 10);
    
    /*send command to download file*/
	memset(command, 0, sizeof(command));
    strcpy(command, "RETR ");
    strcat(command, urlpath); 
    write_socket(sockfd, command);
       
    read_socket(sockfd);
    
    /*read from data socket to retrieve the file*/
    long int size_read = read_data_socket(data_sockfd, urlpath, filesize);
    //printf("Filesize: %ld\nBytes received: %ld\n", filesize, size_read);
    
    read_socket(sockfd);

    /*send command to close connection*/
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
