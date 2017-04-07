// File: server.c 
// Name: Seth Butler 
// Project: 2 
// Class: Internet Protocols 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>

#define MAX_NUM_CONNECTIONS 1
#define BUFFER_SIZE 256
#define ACK_DGRAM_SIZE 8

uint32_t sequenceNumberExpected = 0;    // the USHRT_MAX for this variable is used to signify a failed ACK on the client side
const uint16_t pseudoChksum = 0b0000000000000000;
const uint16_t ackFlag = 0b1010101010101010;
const uint16_t dataFlag = 0b0101010101010101;   // (21,845) - base 10
const uint16_t closeFlag = 0b1111111111111111;

FILE *fileToWrite;

// Prints the error message passed. 
void error(const char *msg)
{
  perror(msg);
  exit(1);
}

/**
 * printDGram - print the datagram to the console
 * @dGram: The datagram to be printed
 * @dGramLen: The length of the datagram's data component
 * @withIndicies: If true, outputs each byte in the datagram with its corresponding index
 *
 **/
void printDGram(u_char *dGram, int dGramLen, uint8_t withIndicies)
{
  // Prints the header
  for (int i=0; i < dGramLen + 8; i++) {
    if (i < 8) {
      printf("dGram[%d]: %u\n", i, (unsigned int)dGram[i]);      
    } else {
    	if(withIndicies > 0) printf("dGram[%d]: %c\n", i, (char)dGram[i]);
    	else printf("%c", (char)dGram[i]);
    }
  }
}

void clearBuffers(u_char *recvdDatagram, u_char *ackDatagram) 
{
  memset(recvdDatagram, 0, BUFFER_SIZE);    
  memset(ackDatagram, 0, ACK_DGRAM_SIZE);    
}

/**
 * calcChecksum - calculate the checksum of the datagram to be sent
 * @nbytes: The number of bytes in the buffer
 * @sum: The variable to hold the checksum during computing
 *
 * Return: uint16_t - The checksum calculated
 **/
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

/**
 * makeHeader - makes the header for the ACK datagram to be sent to the client
 * @ackDatagram - the datagram to be sent to the client
 * @seqNum - the sequence number being ACK'd
 **/
void makeHeader(u_char *ackDatagram, uint32_t seqNum)
{

  uint32_t seqSend=0;
  uint16_t chkSend=0, flagSend=0;

  ackDatagram[0] = seqNum >> 24;
  ackDatagram[1] = seqNum >> 16;
  ackDatagram[2] = seqNum >> 8;
  ackDatagram[3] = seqNum;
  ackDatagram[4] = pseudoChksum >> 8;
  ackDatagram[5] = pseudoChksum;
  ackDatagram[6] = ackFlag >> 8;
  ackDatagram[7] = ackFlag;

  // For testing purposes
  seqSend = (ackDatagram[0] <<  24) | (ackDatagram[1] << 16) | (ackDatagram[2] << 8) | ackDatagram[3];
  chkSend = (ackDatagram[4] << 8) | ackDatagram[5];
  flagSend = (ackDatagram[6] << 8) | ackDatagram[7];

  printf("ACK Seq: %u, Chk: %u, Flag: %u\n", seqSend, chkSend, flagSend);

}

/**
 * sendAck - sends an ACK to the client for a datagram received
 * @sockfd: The file descriptor for the socket
 * @server_addr: Contains the info for the server
 * @ackDatagram: The datagram to be sent to the client
 * @seqNum: The sequence number being ACK'd
 **/
void sendAck(int *sockfd, struct sockaddr_in *server_addr, u_char *ackDatagram, uint32_t seqNum)
{
  printf("Sending Ack for sequence # %d: ", seqNum);

  makeHeader(ackDatagram, seqNum);

  int sendSize = sendto(*sockfd, ackDatagram, ACK_DGRAM_SIZE, 0, (struct sockaddr*) server_addr, sizeof(*server_addr));
  
  if(sendSize < 0) {
    error("Error sending the packet:");
  } else {
    printf("sendSize: %d\n\n", sendSize);
  }
  //memset(ackDatagram, 0, BUFFER_SIZE);
}

/**
 * verifySequence - verifies the sequence number of the datagram received from the client was what it should be
 * @seqRecvd: The sequence number being verified
 **/
int verifySequence(uint32_t seqRecvd)
{
  if ( seqRecvd == sequenceNumberExpected) {
    printf("The sequence # was as expected\n");
    sequenceNumberExpected++; 
    if (sequenceNumberExpected == USHRT_MAX) sequenceNumberExpected = 0;    // refer to global variable declaration

    return 1;
  } else {
    printf("The sequence # was not as expected\n");
    return 0;    
  }
}

/**
 * verifyChksum - verifies the checksum of the datagram receieved from the client
 * @datagram: The datagram the checksum is being computed on
 * @chkRecvd: The checksum received in the datagram
 *
 * Return: (int)bool - if the checksums matched
 **/
int verifyChksum(u_char *recvdDatagram, uint16_t chkRecvd)
{
  uint32_t sum = 0;
  uint16_t calcdChk = 0;

  // Make pseudo header for checksum calculation
  recvdDatagram[4] = pseudoChksum >> 8;
  recvdDatagram[5] = pseudoChksum;

  calcdChk = calcChecksum(recvdDatagram, BUFFER_SIZE, sum);
  printf("Calc'd Chk: %u\n", (unsigned int)calcdChk);

  if (calcdChk == chkRecvd) {
    printf("The checksums matched\n");
    return 1;
  } else {
    printf("The checksums did not matched\n");    
    return 0;
  }
}    

int main(int argc, char *argv[])
{
  int sockfd, portno;                         // The socket file descriptor & port number
  double drop_prob;                           // Probablity a packet is dropped
  
  char *file_name;                            // The file to write to and the buffer to store the datagram
  //u_char buffer[BUFFER_SIZE] = {0};           // Buffer for ACKs
  u_char recvdDatagram[BUFFER_SIZE] = {0};    // Buffer for receiving datagram
  u_char ackDatagram[ACK_DGRAM_SIZE] = {0};

  socklen_t clientLen;                        // Stores the size of the clients sockaddr_in 
  int recsize;                            // Stores size of received datagram

  struct sockaddr_in server_addr;             // Sockadder_in structs that store IP address, port, and etc for the server and its client. 

  if (argc < 4) {
    fprintf(stderr,"usage: %s port# file-name probablity\n", argv[0]);
    exit(1);
  }

  portno = atoi(argv[1]);
  file_name = argv[2];
  drop_prob = atof(argv[3]);

  // Sets all variables in the serv_addr struct to 0 to prevent "junk" 
  // in the variables. "Always pass structures by reference w/ the 
  // size of the structure." 
  //bzero((char *) &server_addr, sizeof(server_addr));
  memset((char*) &server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET;            // Internet Address Family 
  server_addr.sin_port = htons(portno);        // Port Number in Network Byte Order 

  // IP Address in Network Byte Order. In this case it is always 
  //   the address on which the server is running. INADDR_ANY gets 
  //   this address. 
  server_addr.sin_addr.s_addr = INADDR_ANY;

  clientLen = sizeof(server_addr);

  // AF_INET is for the IPv4 protocol. SOCK_DGRAM represents a 
  // Datagram. 0 uses system default for transportation
  // protocol. In this case will be UDP. 
  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0) error("ERROR opening socket");

  // Binds the servers local protocol to the socket. Keeps 
  // the socket reserved and open for this specific
  // process. 
  if ( bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0 ) {
    close(sockfd);
    error("ERROR on binding");
  } 

  fileToWrite = fopen(argv[2], "w");

  if(fileToWrite == NULL) {
    error("Error opening the file\n");
  }

  while (1)
  {
    // Accepts a connection from the client and creates a new socket descriptor
    // to handle communication between the server and the client. 
    recsize = recvfrom(sockfd, (void*)recvdDatagram, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &clientLen);
    if (recsize < 0) {
      error("ERROR on recvfrom");
    }
    printf("receivesize: %d\n", recsize);

    //Retrieve header
    uint32_t seqRecvd = (recvdDatagram[0] <<  24) | (recvdDatagram[1] << 16) | (recvdDatagram[2] << 8) | recvdDatagram[3];
    uint16_t chkRecvd = (recvdDatagram[4] << 8) | recvdDatagram[5];
    uint16_t flagRecvd = (recvdDatagram[6] << 8) | recvdDatagram[7];
    printf("Seq: %u, Chk: %u, Flag: %u\n", seqRecvd, chkRecvd, flagRecvd);

    if (flagRecvd == closeFlag) {
      printf("The client has closed the connection\n");      
      break;
    }

    if ( verifyChksum(recvdDatagram, chkRecvd) && verifySequence(seqRecvd) ) {
      sendAck(&sockfd, &server_addr, ackDatagram, seqRecvd);
      fwrite (&recvdDatagram[8] , sizeof(char), recsize-8, fileToWrite);
      // printf("Start Datagram data: \n");
      //printDGram(recvdDatagram, 100, 0);
      // printf("End Datagram data\n");

    } else { 
      //printDGram(&recvdDatagram[8], recsize-8);
      break;
    }

    clearBuffers(recvdDatagram, ackDatagram);
  }

  close(sockfd);
  fclose(fileToWrite);  
  return 0; 
}
