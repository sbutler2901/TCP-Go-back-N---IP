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
#include <limits.h>

#define BUFFER_SIZE 256

uint32_t sequenceNumber = 0;
const uint16_t pseudoChksum = 0b0000000000000000;
const uint16_t ackFlag = 0b1010101010101010;
const uint16_t dataFlag = 0b0101010101010101;   // (21,845) - base 10
const uint16_t closeFlag = 0b1111111111111111;

// Buffer for the datagram to be sent
u_char sndDatagram[BUFFER_SIZE] = {0};  

// Prints the error message passed. 
void error(const char *msg)
{
  perror(msg);
  exit(0);
}

/**
 * printDGram - print the datagram to the console
 * @dGram: The datagram to be printed
 * @dGramLen: The length of the datagram's data component
 *
 **/
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

/**
 * calcChecksum - calculate the checksum of the datagram to be sent
 * @nbytes: The number of bytes in the buffer
 * @sum: The variable to hold the checksum during computing
 *
 * Return: uint16_t - The checksum calculated
 **/
uint16_t calcChecksum(unsigned nbytes, uint32_t sum)
{
  uint i;

  // Checksum all the pairs of bytes first...
  for (i = 0; i < (nbytes & ~1U); i += 2) {
    sum += (uint16_t)ntohs(*((uint16_t *)(sndDatagram + i)));
    if (sum > 0xFFFF)
      sum -= 0xFFFF;
  }

  /*
   * If there's a single byte left over, checksum it, too.
   * Network byte order is big-endian, so the remaining byte is
   * the high byte.
   */
  if (i < nbytes) {
    sum += sndDatagram[i] << 8;
    if (sum > 0xFFFF)
      sum -= 0xFFFF;
  }

  return (sum);
}

/**
 * addData - adds the data from the buffer to the datagram
 * @buffer: The buffer the data is coming from
 * @buffLen: The length of the data in the buffer
 **/
void addData(char *buffer, int buffLen)
{
  int numBytes;

  // This will need improvement, but for now this is adequeate
  if (buffLen > BUFFER_SIZE - 8) {
    numBytes = BUFFER_SIZE - 8;
  } else {
    numBytes = buffLen;
  }
	memcpy(&sndDatagram[8], buffer, numBytes);
}

/**
 * addNewChksum - adds the computed checksum to the datagram
 * @calcdChk: The computed checksum
 **/
void addNewChksum(uint16_t calcdChk)
{
	sndDatagram[4] = calcdChk >> 8;
  sndDatagram[5] = calcdChk;
}

// Makes the header for regular data grams
/**
 * makeHeader - makes the header for the datagram to be sent
 *
 * Note: The checksum is computed on a header with the pseudo-checksum
 * in the header component for the checksum   
 **/
void makeHeader()
{

  uint32_t sum=0, seqSend=0;
  uint16_t calcdChk=0, chkSend=0, dataSend=0;

  sndDatagram[0] = sequenceNumber >> 24;
  sndDatagram[1] = sequenceNumber >> 16;
  sndDatagram[2] = sequenceNumber >> 8;
  sndDatagram[3] = sequenceNumber;
  sndDatagram[4] = pseudoChksum >> 8;
  sndDatagram[5] = pseudoChksum;
  sndDatagram[6] = dataFlag >> 8;
  sndDatagram[7] = dataFlag;

  //printDGram(sndDatagram, 15);

  calcdChk = calcChecksum(BUFFER_SIZE, sum);

  addNewChksum(calcdChk);  

  // For testing purposes
  seqSend = (sndDatagram[0] <<  24) | (sndDatagram[1] << 16) | (sndDatagram[2] << 8) | sndDatagram[3];
  chkSend = (sndDatagram[4] << 8) | sndDatagram[5];
  dataSend = (sndDatagram[6] << 8) | sndDatagram[7];

  printf("Seq: %u, Chk: %u, Flag: %u\n", seqSend, chkSend, dataSend);
}

/**
 * sendDatagram - sends the datagram to the server
 * @sockfd: The file descriptor for the socket
 * @server_addr: Contains the info for the server
 **/
void sendDatagram(int *sockfd, struct sockaddr_in *server_addr)
{

  int sendsize = sendto(*sockfd, sndDatagram, sizeof(sndDatagram), 0, (struct sockaddr*) server_addr, sizeof(*server_addr));
  
  if(sendsize < 0) {
    error("Error sending the packet:");
    exit(EXIT_FAILURE);
  } else {
    printf("sendsize: %d\n", sendsize);
  }
  memset(sndDatagram, 0, BUFFER_SIZE);

  sequenceNumber++;
  if (sequenceNumber == USHRT_MAX) sequenceNumber = 0;    // refer to getAck()
}
/**
 * verifyAck - verifys the ACK'd seq # received from the server
 * @ackdSeqNum: The sequence # received in the ACK
 *
 * Note: If the sequence number is USHRT_MAX then the datagram received was not
 * an ACK.
 **/
void verifyAck(uint32_t ackdSeqNum)
{
  if (ackdSeqNum == USHRT_MAX) printf("The received datagram was not an ACK\n");
  else printf("Seq # %u has been acknowledged\n\n", ackdSeqNum);
}

/**
 * getAck - receives the ACK from the server for the datagram sent.
 * @sockfd: The file descriptor for the socket
 * @server_addr: Contains the info for the server
 * @clientLen: TBD
 *
 * Note: If the ack received does not have to appropriate flag in the header USHRT_MAX will be returned
 *
 * Return: uint32_t - the sequence # received in the ACK
 **/
uint32_t getAck(int *sockfd, struct sockaddr_in *server_addr, socklen_t *clientLen)
{
  int recsize;
  u_char recvdDatagram[BUFFER_SIZE] = {0};    // Buffer for receiving datagram

  recsize = recvfrom(*sockfd, (void*)recvdDatagram, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, clientLen);
  if (recsize < 0) {
    error("ERROR on recvfrom");
    exit(1);
  }
  printf("receivesize: %d\n", recsize);

  uint32_t seqRecvd = (recvdDatagram[0] <<  24) | (recvdDatagram[1] << 16) | (recvdDatagram[2] << 8) | recvdDatagram[3];
  uint16_t chkRecvd = (recvdDatagram[4] << 8) | recvdDatagram[5];
  uint16_t flagRecvd = (recvdDatagram[6] << 8) | recvdDatagram[7];
  printf("Ack's Seq: %u, Chk: %u, Flag: %u\n", seqRecvd, chkRecvd, flagRecvd); 

  if (flagRecvd == ackFlag) {
    printf("The ACK for seq # %d was received\n", seqRecvd);
    return seqRecvd;
  }
  return USHRT_MAX;
}

/**
 * closeConnection - closes the connection to the server using the predefined close flag
 * @sockfd: The file descriptor for the socket
 * @server_addr: Contains the info for the server
 **/
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
  socklen_t clientLen;                        // Stores the size of the clients sockaddr_in 

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

  clientLen = sizeof(server_addr);

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

  //memcpy(&sndDatagram[8], "Dog", 3);  // Add data
  addData("Dog", 3);
  makeHeader();    
  sendDatagram(&sockfd, &server_addr);  
  verifyAck( getAck(&sockfd, &server_addr, &clientLen) );  

  addData("hello, world!", 13);
  makeHeader();    
  sendDatagram(&sockfd, &server_addr);
  verifyAck( getAck(&sockfd, &server_addr, &clientLen) );  

  addData("Dog", 3);  // Add data
  makeHeader();  
  sendDatagram(&sockfd, &server_addr);
  verifyAck( getAck(&sockfd, &server_addr, &clientLen) );  

  addData("CLOSE", 5);  // Add data
  makeHeader();  
  sendDatagram(&sockfd, &server_addr);
  verifyAck( getAck(&sockfd, &server_addr, &clientLen) );  

  closeConnection(&sockfd, &server_addr);
  close(sockfd);

  return 0;
}
