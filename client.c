// File: client.c 
// Name: Seth Butler 
// Project: 2 
// Class: Internet Protocols 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define BUFFER_SIZE 256

// Prints the error message passed. 
void error(const char *msg)
{
  perror(msg);
  exit(0);
}

int main(int argc, char *argv[])
{
  // The socket file descriptor, port number,
  // and the number of chars read/written
  int sockfd, portno, rwChars, winSize, maxSegSize, bytes_sent;

  // Sockadder_in struct that stores the IP address, 
  // port, and etc of the server.
  struct sockaddr_in server_addr;

  // Hostent struct that keeps relevant host info. 
  // Such as official name and address family.
  struct hostent *server;

  // Read/Write stream buffer, the server's host name, and the file's name to read from. 
  char buffer[BUFFER_SIZE], *host_name, *file_name;

  if (argc < 6) {
    fprintf(stderr,"usage: %s hostname port file-name N MSS\n", argv[0]);
    exit(0);
  }

  host_name = argv[1];
  portno = atoi(argv[2]);
  file_name = argv[3];
  winSize = atoi(argv[4]);
  maxSegSize = atoi(argv[5]);

  printf("%s, %d, %s, %d, %d\n", host_name, portno, file_name, winSize, maxSegSize);

  // AF_INET is for the IPv4 protocol. SOCK_STREAM represents a 
  // Stream Socket. 0 uses system default for transportation
  // protocol. In this case will be UDP 
  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0) error("ERROR opening socket");

  // Sets all variables in the server_addr struct to 0 to prevent "junk" 
  // in the variables. "Always pass structures by reference w/ the 
  // size of the structure." 
  //bzero((char *) &server_addr, sizeof(server_addr));
  memset((char *) &server_addr, 0, sizeof(server_addr));

  // Internet Address Family 
  server_addr.sin_family = AF_INET;

  // Port Number in Network Byte Order 
  server_addr.sin_port = htons(portno);

  // Retrieves the host information based on the address
  // passed from the users commandline. 
  server = gethostbyname(argv[1]);
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }

  // Copies the server info into the the appropriate socket struct. 
  bcopy((char *) server->h_addr, (char *) &server_addr.sin_addr.s_addr, server->h_length);
  //memcpy((char *) &server_addr.sin_addr.s_addr, (char *) server->h_addr, server->h_length);

  strcpy(buffer, "hello, world!");


  bytes_sent = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if(bytes_sent < 0) {
    error("Error sending the packet:");
    exit(EXIT_FAILURE);
  }

  /*
  // Establishes the connection between the server and the client. 
  //if (connect(sockfd,( struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) error("ERROR connecting");

  do {
    printf("Please enter a state (case sensitive): ");

    //bzero(buffer, BUFFER_SIZE);
    memset(buffer, 0, BUFFER_SIZE);

    // Gets the state.
    fgets(buffer, rwMax,stdin);
    
    // Removes the newline character from the user's message.  
    buffer[strlen(buffer)-1] = '\0';

    // Writes the client to the server. 
    rwChars = write(sockfd,buffer,strlen(buffer));
    if (rwChars < 0) error("ERROR writing to socket");

    //bzero(buffer, BUFFER_SIZE);
    memset(buffer, 0, BUFFER_SIZE);


    // Reads what the server sent to the client. 
    rwChars = read(sockfd, buffer, rwMax);
    if (rwChars < 0) error("ERROR reading from socket");

    printf("%s\n", buffer);

    printf("Would you like to continue? [y]: ");

    fgets(confirmation, 5, stdin);

  } while (confirmation[0] == 'y');

  rwChars = write(sockfd, "END", strlen("END"));
  // Closes the process's connection to it's sockets. 

  */

  close(sockfd);

  return 0;
}
