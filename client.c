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

uint32_t sequenceNumber = 0;
uint16_t checksum = 0b0000000000000000;
uint16_t dataFlag = 0b0101010101010101;   // (21,845) - base 10


// Prints the error message passed. 
void error(const char *msg)
{
  perror(msg);
  exit(0);
}

uint32_t calcChecksum(unsigned char *buf, unsigned nbytes, u_int32_t sum)
{
  uint i;

  /* Checksum all the pairs of bytes first... */
  for (i = 0; i < (nbytes & ~1U); i += 2) {
    sum += (u_int16_t)ntohs(*((u_int16_t *)(buf + i)));
    if (sum > 0xFFFF)
      sum -= 0xFFFF;
  }

  /*
   * If there's a single byte left over, checksum it, too.
   * Network byte order is big-endian, so the remaining byte is
   * the high byte.
   */
  if (i < nbytes) {
    sum += buf[i] << 8;
    if (sum > 0xFFFF)
      sum -= 0xFFFF;
  }

  return (sum);
}

void makeHeader(u_char *datagram) {
  datagram[0] = sequenceNumber >> 24;
  datagram[1] = sequenceNumber >> 16;
  datagram[2] = sequenceNumber >> 8;
  datagram[3] = sequenceNumber;
  datagram[4] = checksum >> 8;
  datagram[5] = checksum;
  datagram[6] = dataFlag >> 8;
  datagram[7] = dataFlag;


  uint32_t seqRetrieve = (datagram[0] <<  24) | (datagram[1] << 16) | (datagram[2] << 8) | datagram[3];
  printf("seqRetrieve: %u\n", seqRetrieve);

  uint32_t chkRetrieve = (datagram[4] << 8) | datagram[5];
  printf("chkRetrieve: %u\n", chkRetrieve);

  uint32_t dataRetrieve = (datagram[6] << 8) | datagram[7];
  printf("dataRetrieve: %u\n", dataRetrieve);  
}

void sendDatagram(int *sockfd, char *buffer, struct sockaddr_in *server_addr) {
  // void *datagram = NULL;
  // datagram = malloc(BUFFER_SIZE);

  u_char datagram[BUFFER_SIZE];

  //printf("datagram length: %lu\n", strlen(datagram));
  //printf("flag: %c, %c, %c, %c\n", datagram[0], datagram[1], datagram[2], datagram[3]);

  //snprintf(datagram, BUFFER_SIZE, "%u%d%d%s", sequenceNumber, checksum, dataFlag, buffer);
  //snprintf(datagram, BUFFER_SIZE, "%lu%d%d", sequenceNumber, checksum, dataFlag);

  //snprintf(datagram, BUFFER_SIZE, "%u", sequenceNumber);

  //printf("datagram length: %lu\n", strlen(datagram));

  //memcpy(datagram, sequenceNumber, 4);

  makeHeader(datagram);

  memcpy(&datagram[8], "Hell", 4);
  //strcpy(&datagram[8], "Hello");

  uint32_t tmp0;
  uint32_t tmp1 = calcChecksum(datagram, BUFFER_SIZE, tmp0);
  printf("tmp0: %u, tmp1: %u\n", tmp0, tmp1);

  int sendsize = sendto(*sockfd, datagram, sizeof(datagram), 0, (struct sockaddr*) server_addr, sizeof(*server_addr));
  if(sendsize < 0) {
    error("Error sending the packet:");
    exit(EXIT_FAILURE);
  } else {
    printf("sendsize: %d\n", sendsize);
  }
  sequenceNumber++;
}


int main(int argc, char *argv[])
{
  // The socket file descriptor, port number,
  // and the number of chars read/written
  int sockfd, portno, rwChars, winSize, maxSegSize, sendsize;

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

  strcpy(buffer, "hello, world!");

  sendDatagram(&sockfd, buffer, &server_addr);

/*
  recsize = recvfrom(sockfd, (void*)buffer, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &clientLen);
  if (recsize < 0) {
    error("ERROR on recvfrom");
    exit(1);
  } else if ( strstr(buffer, "CLOSE") ) {
    printf("CLOSE was sent: %s\n", buffer);
    break;
  }
  printf("recsize: %d\n", (int)recsize);
  printf("datagram: %.*s\n", (int)recsize, buffer);

  memset(buffer, 0, BUFFER_SIZE);*/

  strcpy(buffer, "Second");

  sendDatagram(&sockfd, buffer, &server_addr);

  strcpy(buffer, "CLOSE");

  sendDatagram(&sockfd, buffer, &server_addr);

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