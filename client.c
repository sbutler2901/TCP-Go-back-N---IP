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

const uint16_t pseudoChksum = 0b0000000000000000;
const uint16_t ackFlag = 0b1010101010101010;
const uint16_t dataFlag = 0b0101010101010101;   // (21,845) - base 10
const uint16_t closeFlag = 0b1111111111111111;
uint32_t sequenceNumber = 0;

FILE *fileToTransfer;

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

/**
 * calcChecksum - calculate the checksum of the datagram to be sent
 * @sndDatagram: the datagram to calculate the checksum over
 * @nbytes: The number of bytes in the buffer
 * @sum: The variable to hold the checksum during computing
 *
 * Return: uint16_t - The checksum calculated
 **/
uint16_t calcChecksum(u_char *sndDatagram, unsigned nbytes, uint32_t sum)
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
 * @sndDatagram: The datagram to which the buffer data is being added
 * @buffer: The buffer the data is coming from
 * @buffDataLen: The length of the data in the buffer
 **/
void addData(u_char *sndDatagram, char *buffer, int buffDataLen)
{
  int numBytes;

  // This will need improvement, but for now this is adequate
  if (buffDataLen > BUFFER_SIZE - 8) {
    numBytes = BUFFER_SIZE - 8;
  } else {
    numBytes = buffDataLen;
  }

	memcpy(&sndDatagram[8], buffer, numBytes);
  memset(buffer, 0, BUFFER_SIZE);      
}

/**
 * addNewChksum - adds the computed checksum to the datagram
 * @sndDatagram: the datagram to which the checksum is being added
 * @calcdChk: The computed checksum
 **/
void addNewChksum(u_char *sndDatagram, uint16_t calcdChk)
{
	sndDatagram[4] = calcdChk >> 8;
  sndDatagram[5] = calcdChk;
}

// Makes the header for regular data grams
/**
 * makeHeader - makes the header for the datagram to be sent
 * @sndDatagram: the datagram buffer for the header to be placed
 *
 * Note: The checksum is computed on a header with the pseudo-checksum
 * in the header component for the checksum   
 **/
void makeHeader(u_char *sndDatagram)
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

  calcdChk = calcChecksum(sndDatagram, BUFFER_SIZE, sum);

  addNewChksum(sndDatagram, calcdChk);  

  // For testing purposes
  seqSend = (sndDatagram[0] <<  24) | (sndDatagram[1] << 16) | (sndDatagram[2] << 8) | sndDatagram[3];
  chkSend = (sndDatagram[4] << 8) | sndDatagram[5];
  dataSend = (sndDatagram[6] << 8) | sndDatagram[7];

  printf("Datagram Seq: %u, Chk: %u, Flag: %u\n", seqSend, chkSend, dataSend);
}

/**
 * sendDatagram - sends the datagram to the server
 * @sndDatagram: The datagram being sent
 * @sockfd: The file descriptor for the socket
 * @server_addr: Contains the info for the server
 **/
void sendDatagram(int *sockfd, struct sockaddr_in *server_addr, u_char *sndDatagram, int datagramLen)
{

  int sendsize = sendto(*sockfd, sndDatagram, datagramLen, 0, (struct sockaddr*) server_addr, sizeof(*server_addr));
  
  if(sendsize < 0) {
    error("Error sending the packet:");
    exit(1);
  } else {
    printf("sendsize: %d\n", sendsize);
  }

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
  if (ackdSeqNum == USHRT_MAX) printf("The received datagram was not an ACK\n\n");
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
  uint32_t seqRecvd, chkRecvd, flagRecvd;
  u_char recvdDatagram[BUFFER_SIZE] = {0};    // Buffer for receiving datagram

  recsize = recvfrom(*sockfd, (void*)recvdDatagram, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, clientLen);
  if (recsize < 0) {
    error("ERROR on recvfrom");
    exit(1);
  }
  printf("receivesize: %d\n", recsize);

  seqRecvd = (recvdDatagram[0] <<  24) | (recvdDatagram[1] << 16) | (recvdDatagram[2] << 8) | recvdDatagram[3];
  chkRecvd = (recvdDatagram[4] << 8) | recvdDatagram[5];
  flagRecvd = (recvdDatagram[6] << 8) | recvdDatagram[7];
  printf("Ack's Seq: %u, Chk: %u, Flag: %u\n", seqRecvd, chkRecvd, flagRecvd); 

  if (flagRecvd == ackFlag) {
    printf("The ACK for seq # %d was received\n", seqRecvd);
    return seqRecvd;
  }
  return USHRT_MAX;
}

/**
 * clearBuffers - clears the buffers being used for each round of datagra / ACK interations
 * @sndDatagram: The datagram buffer to be nulled
 * @fileBuffer: The buffer storing the file's lines to be nulled
 **/
void clearBuffers(u_char *sndDatagram, char *fileBuffer)
{
  memset(sndDatagram, 0, BUFFER_SIZE);
  memset(fileBuffer, 0, BUFFER_SIZE);  
}

/**
 * closeConnection - closes the connection to the server using the predefined close flag
 * @sockfd: The file descriptor for the socket
 * @server_addr: Contains the info for the server
 * @sndDatagram: The datagram being sent to close the connection
 **/
void closeConnection(int *sockfd, struct sockaddr_in *server_addr, u_char *sndDatagram)
{
	
  sndDatagram[0] = sequenceNumber >> 24;
  sndDatagram[1] = sequenceNumber >> 16;
  sndDatagram[2] = sequenceNumber >> 8;
  sndDatagram[3] = sequenceNumber;
  sndDatagram[4] = pseudoChksum >> 8;
  sndDatagram[5] = pseudoChksum;
  sndDatagram[6] = closeFlag >> 8;
  sndDatagram[7] = closeFlag;

  sendDatagram(sockfd, server_addr, sndDatagram, 8);
}

// Gets BUFFER_SIZE amount of data from file
char * readFile(char *fileBuffer) {
  return fgets(fileBuffer, BUFFER_SIZE, fileToTransfer);
}

// int u_char_array_len(u_char *array)
// {
//    /* Initialize a unsigned char pointer here  */
//   char *ptr = (char *)array;
//   while(*ptr != '\0') {
//     ptr++;
//     printf("test\n");
//   }
//   printf("Dif: %ld\n", ptr - (char*)array);
//   return ptr - (char*)array;
//    /* A loop that starts at string_start and
//     * is increment by one until it's value is zero,
//     *e.g. while(*s!=0) or just simply while(*s) */
//    /* Return the difference of the incremented pointer and the original pointer */  
// }

int main(int argc, char *argv[])
{
  int sockfd, portno, winSize, maxSegSize;    // The socket file descriptor, port number, and the number of chars read/written
  u_char sndDatagram[BUFFER_SIZE] = {0};      // The buffer storing each datagram before it is sent
  char fileBuffer[BUFFER_SIZE] = {0};         // Buffer storing the file data for each datagram
  struct sockaddr_in server_addr;             // Sockadder_in struct that stores the IP address, port, and etc of the server.
  socklen_t clientLen;                        // Stores the size of the clients sockaddr_in 
  struct hostent *server;                     // Hostent struct that keeps relevant host info. Such as official name and address family.
  char *host_name, *file_name;                // The host name and file name retrieve from command line

  if (argc < 6) {
    fprintf(stderr,"usage: %s hostname port file-name N MSS\n", argv[0]);
    exit(1);
  }

  //*** Init - Begin ***

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
    exit(1);
  }

  // Copies the server info into the the appropriate socket struct. 
  bcopy((char *) server->h_addr, (char *) &server_addr.sin_addr.s_addr, server->h_length);

  fileToTransfer = fopen(argv[3], "r");

  if(fileToTransfer == NULL) {
    error("Error opening the file");
    exit(1);
  }

  //*** Init - End ***

  //*** The client processes are ready to begin ***

  while(readFile(fileBuffer) != NULL) {
    addData(sndDatagram, fileBuffer, BUFFER_SIZE);
    makeHeader(sndDatagram);
    //datagramlen(fileBuffer);
    sendDatagram(&sockfd, &server_addr, sndDatagram, BUFFER_SIZE);  
    verifyAck( getAck(&sockfd, &server_addr, &clientLen) );
    clearBuffers(sndDatagram, fileBuffer);    
  }

  //** End file sending **/

  closeConnection(&sockfd, &server_addr, sndDatagram);  
  close(sockfd);
  fclose(fileToTransfer);  

  exit(0);
}
