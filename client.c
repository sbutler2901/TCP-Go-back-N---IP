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
#include <sys/time.h>

#define BUFFER_SIZE 256
#define TIMEOUT 3.0

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
void addData(u_char *sndDatagram, char *fileBuffer, size_t maxSegSize)
{
	memcpy(&sndDatagram[8], fileBuffer, maxSegSize);
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
void makeHeader(u_char *sndDatagram, int dGramLen)
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

  calcdChk = calcChecksum(sndDatagram, dGramLen+8, sum);

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
int sendDatagram(int *sockfd, struct sockaddr_in *server_addr, u_char *sndDatagram, int datagramLen)
{

  int sendSize = sendto(*sockfd, sndDatagram, datagramLen, 0, (struct sockaddr*) server_addr, sizeof(*server_addr));
  if(sendSize < 0) error("Error sending the packet:");
  
  printf("sendSize: %d\n", sendSize);

  sequenceNumber++;
  if (sequenceNumber == USHRT_MAX) sequenceNumber = 0;    // refer to getAck()
  return sendSize;
}

/**
 * verifyACK - verifys the ACK'd seq # received from the server
 * @ackdSeqNum: The sequence # received in the ACK
 *
 * Note: If the sequence number is USHRT_MAX then the datagram received was not
 * an ACK.
 **/
int verifyACK(uint32_t lastSeqACKd, uint32_t ackdSeqNum)
{
  if (ackdSeqNum == USHRT_MAX) { 
    printf("The received datagram was not an ACK\n\n");
    return 0;
  }
  if(lastSeqACKd > ackdSeqNum) {
    printf("lastACKd= %d, recentACK= %d\n", lastSeqACKd, ackdSeqNum);
    return 0;
  }
  printf("Seq # %u has been acknowledged\n\n", ackdSeqNum);
  return 1;
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
  if (recsize < 0) error("ERROR on recvfrom");
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
void clearBuffers(u_char *sndDatagram, char *fileBuffer, size_t maxSegSize)
{
  memset(sndDatagram, 0, maxSegSize+8);
  memset(fileBuffer, 0, maxSegSize);  
}

/**
 * closeConnection - closes the connection to the server using the predefined close flag
 * @sockfd: The file descriptor for the socket
 * @server_addr: Contains the info for the server
 * @sndDatagram: The datagram being sent to close the connection
 **/
void closeConnection(int *sockfd, struct sockaddr_in *server_addr, u_char *sndDatagram)
{
  printf("Client: closing connection\n");
	
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
// char * readFile(char *fileBuffer) {
//   return fgets(fileBuffer, BUFFER_SIZE, fileToTransfer);
// }

size_t readFile(char *fileBuffer, size_t numToRead) {
  return fread((void*)fileBuffer, sizeof(char), numToRead, fileToTransfer);
}

void startTimer(struct timeval *timer) {
  if(gettimeofday(timer, NULL) != 0) error("ERROR: gettimeofday failed");
}

int hasTimerExpired(struct timeval *timer)
{
  struct timeval currentTime;

  if(timer->tv_sec == -1) return 0;
  if(gettimeofday(&currentTime, NULL) != 0) error("ERROR: gettimeofday failed");
  if(difftime(currentTime.tv_sec, timer->tv_sec) > TIMEOUT) return 1;
  return 0;
}

int areThereACKs(int maxfd, fd_set *allset, fd_set *rset, struct timeval *timeout)
{
	int nready;

  *rset = *allset;    // needs to be reset each time
  nready = select( maxfd, rset, NULL, NULL,  timeout);
  if(nready < 0) error("ERROR: select failed");
  return nready;
}

void savePacket(u_char *sndDatagram, u_char **goBackDgrams, int goBackDgramPtr, size_t maxSegSize)
{
 for(size_t i = 0; i<maxSegSize+8; i++) {
    goBackDgrams[goBackDgramPtr][i] = sndDatagram[i];
  }
}

void resendDgrams(u_char **goBackDgrams, int *sockfd, struct sockaddr_in *server_addr, size_t maxSegSize, int goBackDgramPtr, int sndDataSize, int winSize)
{
	int i, dGramLen = -1, numResent = 0;
	size_t j;
  u_char *sndDatagram = (u_char*) calloc(sndDataSize, sizeof(u_char));

  if (sndDatagram == NULL) error("Datagram memory allocation failure\n");

  for(i = goBackDgramPtr; numResent < winSize; i++) {
    if(i >= winSize) i = 0;
    for(j = 0; j<maxSegSize+8; j++) {
      sndDatagram[j] = goBackDgrams[i][j];
      if(dGramLen == -1 && j>=8 && sndDatagram[j] == 0) dGramLen = j;
    }
    if(dGramLen == -1) dGramLen = maxSegSize+8;

    sendDatagram(sockfd, server_addr, sndDatagram, dGramLen);  
    memset(sndDatagram, 0, maxSegSize+8);
    numResent++;
    dGramLen = -1;
  }
  free(sndDatagram);
}

int main(int argc, char *argv[])
{
	// The socket file descriptor, port number, and the number of chars read/written
  int sockfd, portno, winSize, currentWin, sndDataSize, fileBufferSize, goBackDgramPtr = 0;    
  size_t maxSegSize, sendSize, numRead = 0;
  u_char *sndDatagram;      // The buffer storing each datagram before it is sent
  char *fileBuffer;         // Buffer storing the file data for each datagram
  struct sockaddr_in server_addr;             // Sockadder_in struct that stores the IP address, port, and etc of the server.
  socklen_t clientLen;                        // Stores the size of the clients sockaddr_in 
  struct hostent *server;                     // Hostent struct that keeps relevant host info. Such as official name and address family.
  char *host_name, *file_name;                // The host name and file name retrieve from command line
  u_char **goBackDgrams;
  uint32_t lastSeqACKd = 0, acksSeq;

  // START select() - Used by select() to poll if there are ACKs to be read
  fd_set rset;              // File descriptors that might be ready to read
  fd_set allset;            // Set of all file descriptors
  int maxfd;                // Max file descriptor of sets (will need to be incremented by 1)
  struct timeval timeout;   // Specifies how long select should wait. 0 for both elements == no block
  timeout.tv_sec = 0;       // Represents the number of whole seconds of elapsed time
  timeout.tv_usec = 0;      // The rest of the elapsed time (a fraction of a second), represented as the number of microseconds.
  struct timeval timer;     // Tracks timeout for retransmission
  timer.tv_sec = -1;
  timer.tv_usec = -1;
  // END

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

  currentWin = winSize;

  if(maxSegSize > BUFFER_SIZE) maxSegSize = BUFFER_SIZE;  // Server is unaware of maxSegSize so this is a temp fix

  // It appears u_char & char are of size 1B
  sndDataSize = (sizeof(u_char)*maxSegSize) + 8;
  fileBufferSize = sizeof(char)*maxSegSize;

  //printf("snd: %d, file: %d\n", sndDataSize, fileBufferSize);

  goBackDgrams = (u_char**) malloc(winSize * sizeof(*goBackDgrams));
  if (goBackDgrams == NULL) error("Go back step 1 memory allocation failure\n");

  for(int i=0; i<winSize; i++) {
    goBackDgrams[i] = (u_char*) malloc(sndDataSize);
    if (goBackDgrams[i] == NULL) error("Go back step 2 memory allocation failure\n");
  }

  sndDatagram = (u_char*) malloc(sndDataSize);
  fileBuffer = (char*) malloc(fileBufferSize);
  if (sndDatagram == NULL) error("Datagram memory allocation failure\n");
  if (fileBuffer == NULL) error("Filebuffer memory allocation failure\n");

  // AF_INET is for the IPv4 protocol. SOCK_STREAM represents a 
  // Stream Socket. 0 uses system default for transportation
  // protocol. In this case will be UDP 
  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0) error("ERROR opening socket");

  maxfd = sockfd+1;  
  FD_ZERO(&allset);         // Initialiazes
  FD_SET(sockfd, &allset);  // Adds socket
  rset = allset;            // initializes read set

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
  if (server == NULL) error("ERROR, no such host");

  // Copies the server info into the the appropriate socket struct. 
  bcopy((char *) server->h_addr, (char *) &server_addr.sin_addr.s_addr, server->h_length);

  fileToTransfer = fopen(argv[3], "r");
  if(fileToTransfer == NULL) error("Error opening the file to tranfer");

  //*** Init - End ***

  //*** The client processes are ready to begin ***

  while(1) {
    if(hasTimerExpired(&timer)) {
      printf("Timer expired\n");
      resendDgrams(goBackDgrams, &sockfd, &server_addr, maxSegSize, goBackDgramPtr, sndDataSize, winSize);
      startTimer(&timer);
    }
    while(areThereACKs(maxfd, &allset, &rset, &timeout)) {
      acksSeq = getAck(&sockfd, &server_addr, &clientLen);

      if(verifyACK(lastSeqACKd, acksSeq)) {
        currentWin++;
        lastSeqACKd = acksSeq;
        startTimer(&timer);
      } 
    }
    if(currentWin > 0) {
      // START - Send packet
      numRead = readFile(fileBuffer, maxSegSize);
      if(numRead <= 0) {
        printf("There is no more data to send\n");
        break;
      }

      addData(sndDatagram, fileBuffer, maxSegSize);
      makeHeader(sndDatagram, maxSegSize);

      // START - Save packet
      savePacket(sndDatagram, goBackDgrams, goBackDgramPtr, maxSegSize);
      goBackDgramPtr++;
      if(goBackDgramPtr == winSize) goBackDgramPtr = 0;
      // END - save packet

      sendSize = sendDatagram(&sockfd, &server_addr, sndDatagram, numRead+8);  
      clearBuffers(sndDatagram, fileBuffer, maxSegSize);
      // END - send packet

      currentWin--;
      if(timer.tv_sec == -1) startTimer(&timer);  // First loop: timer has never been started
    }
  }
  //** End file sending **/

  closeConnection(&sockfd, &server_addr, sndDatagram);  
  close(sockfd);
  for(int i=0; i<winSize; i++) {
    free(goBackDgrams[i]);
  }
  free(goBackDgrams);
  free(sndDatagram);
  free(fileBuffer);
  fclose(fileToTransfer);  
  exit(0);
}
