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
uint16_t pseudoChksum = 0b0000000000000000;
uint16_t dataFlag = 0b0101010101010101;   // (21,845) - base 10
uint16_t closeFlag = 0b1111111111111111;

// Buffer for the datagram to be sent
u_char sndDatagram[BUFFER_SIZE] = {0};  

// Prints the error message passed. 
void error(const char *msg)
{
  perror(msg);
  exit(0);
}

uint16_t calcChecksum(unsigned char *buf, unsigned nbytes, uint32_t sum)
{
  uint i;

  // Checksum all the pairs of bytes first...
  for (i = 0; i < (nbytes & ~1U); i += 2) {
    sum += (uint16_t)ntohs(*((uint16_t *)(buf + i)));
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

void printDGram(u_char *dGram, int dGramLen)
{
  // Prints the header
  for (int i=0; i < dGramLen + 8; i++) {
    if (i < 8) {
      printf("dGram[%d]: %u\n", i, (unsigned int)dGram[i]);      
    } else {
      printf("dGram[%d]: %c\n", i, (char)dGram[i]);      
    }
  }
}

void addData(u_char *sndDatagram, char *buffer)
{
	memcpy(&sndDatagram[8], buffer, BUFFER_SIZE - 8);
}

// Adds the computed checksum after calculating with the pseudo header
void addNewChksum(u_char *sndDatagram, uint16_t calcdChk)
{
	sndDatagram[4] = calcdChk >> 8;
  sndDatagram[5] = calcdChk;
}

// Makes the header for regular data grams
void makeHeader()
{

  uint32_t sum, seqSend;
  uint16_t calcdChk, chkSend, dataSend;

  sndDatagram[0] = sequenceNumber >> 24;
  sndDatagram[1] = sequenceNumber >> 16;
  sndDatagram[2] = sequenceNumber >> 8;
  sndDatagram[3] = sequenceNumber;
  sndDatagram[4] = pseudoChksum >> 8;
  sndDatagram[5] = pseudoChksum;
  sndDatagram[6] = dataFlag >> 8;
  sndDatagram[7] = dataFlag;

  calcdChk = calcChecksum(sndDatagram, BUFFER_SIZE, sum);

  addNewChksum(sndDatagram, calcdChk);  

  // For testing purposes
  seqSend = (sndDatagram[0] <<  24) | (sndDatagram[1] << 16) | (sndDatagram[2] << 8) | sndDatagram[3];
  chkSend = (sndDatagram[4] << 8) | sndDatagram[5];
  dataSend = (sndDatagram[6] << 8) | sndDatagram[7];

  printf("Seq: %u, Chk: %u, Flag: %u\n", seqSend, chkSend, dataSend);
  printf("Calc'd Chk: %u\n", calcdChk);

}

void sendDatagram(int *sockfd, struct sockaddr_in *server_addr)
{

  makeHeader();

  printDGram(sndDatagram, 15);

  int sendsize = sendto(*sockfd, sndDatagram, sizeof(sndDatagram), 0, (struct sockaddr*) server_addr, sizeof(*server_addr));
  
  if(sendsize < 0) {
    error("Error sending the packet:");
    exit(EXIT_FAILURE);
  } else {
    printf("sendsize: %d\n\n", sendsize);
  }
  memset(sndDatagram, 0, BUFFER_SIZE);

  sequenceNumber++;
}

// Closes the connection to the server
void closeConnection(int *sockfd, struct sockaddr_in *server_addr)
{
	
  sndDatagram[0] = sequenceNumber >> 24;
  sndDatagram[1] = sequenceNumber >> 16;
  sndDatagram[2] = sequenceNumber >> 8;
  sndDatagram[3] = sequenceNumber;
  sndDatagram[4] = pseudoChksum >> 8;
  sndDatagram[5] = pseudoChksum;
  sndDatagram[6] = closeFlag >> 8;
  sndDatagram[7] = closeFlag;

  sendDatagram(sockfd, server_addr);
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
  memset((char *) &server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET;					// Internet Address Family 
  server_addr.sin_port = htons(portno);		  // Port Number in Network Byte Order 

  // Retrieves the host information based on the address
  // passed from the users commandline. 
  server = gethostbyname(argv[1]);
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }

  // Copies the server info into the the appropriate socket struct. 
  bcopy((char *) server->h_addr, (char *) &server_addr.sin_addr.s_addr, server->h_length);

  memset(sndDatagram, 0, BUFFER_SIZE);

  memcpy(&sndDatagram[8], "Dog", 3);  // Add data
  //makeHeader(sndDatagram);    
  sendDatagram(&sockfd, &server_addr);  

  memcpy(&sndDatagram[8], "hello, world!", 13);   // Add data
  //makeHeader(sndDatagram);    
  sendDatagram(&sockfd, &server_addr);


  memcpy(&sndDatagram[8], "Dog", 3);  // Add data
  //makeHeader(sndDatagram);  
  sendDatagram(&sockfd, &server_addr);

  memcpy(&sndDatagram[8], "CLOSE", 5);  // Add data
  //makeHeader(sndDatagram);  
  sendDatagram(&sockfd, &server_addr);

  //closeConnection(&sockfd, &server_addr);
  close(sockfd);

  return 0;
}
