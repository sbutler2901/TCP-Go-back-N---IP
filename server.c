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
#include <time.h>

#undef DEBUG

#define BUFFER_SIZE 1032
#define ACK_DGRAM_SIZE 8
#define MAX_TIMES_FAIL 128

uint32_t sequenceNumberExpected = 0;    // the USHRT_MAX for this variable is used to signify a failed ACK on the client side
const uint16_t pseudoChksum = 0b0000000000000000;
const uint16_t ackFlag = 0b1010101010101010;
const uint16_t dataFlag = 0b0101010101010101;   // (21,845) - base 10
const uint16_t closeFlag = 0b1111111111111111;

FILE *fileToWrite;

/**
 * error - prints the value of errno & exit
 * @msg: The specific message to preceed the error
 **/
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
  for (int i=0; i < dGramLen + 8; i++) {
    if (i < 8) {
			// Header
      printf("dGram[%d]: %u\n", i, (unsigned int)dGram[i]);      
    } else {
    	if(withIndicies > 0) printf("dGram[%d]: %c\n", i, (char)dGram[i]);
    	else printf("%c", (char)dGram[i]);
    }
  }
}

/**
 * clearBuffers - Clears the datagram buffers
 * recvdDatagram: The buffer for receiving datagrams
 * ackDatagram: The buffer for sending ACK datagrams
 **/
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

  ackDatagram[0] = seqNum >> 24;
  ackDatagram[1] = seqNum >> 16;
  ackDatagram[2] = seqNum >> 8;
  ackDatagram[3] = seqNum;
  ackDatagram[4] = pseudoChksum >> 8;
  ackDatagram[5] = pseudoChksum;
  ackDatagram[6] = ackFlag >> 8;
  ackDatagram[7] = ackFlag;


#ifdef DEBUG
  uint32_t seqSend = (ackDatagram[0] <<  24) | (ackDatagram[1] << 16) | (ackDatagram[2] << 8) | ackDatagram[3];
  uint16_t chkSend = (ackDatagram[4] << 8) | ackDatagram[5];
  uint16_t flagSend = (ackDatagram[6] << 8) | ackDatagram[7];  
  printf("ACK Seq: %u, Chk: %u, Flag: %u\n", seqSend, chkSend, flagSend);
#endif
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
  int sendSize;

#ifdef DEBUG
  printf("Sending Ack for sequence # %d: ", seqNum);
#endif

  makeHeader(ackDatagram, seqNum);

  sendSize = sendto(*sockfd, ackDatagram, ACK_DGRAM_SIZE, 0, (struct sockaddr*) server_addr, sizeof(*server_addr));
  
  if(sendSize < 0) error("Error sending the packet:"); 
  //printf("sendSize: %d\n\n", sendSize);
}

/**
 * verifySequence - verifies the sequence number of the datagram received from the client was what it should be
 * @seqRecvd: The sequence number being verified
 **/
int verifySequence(uint32_t seqRecvd)
{
  if ( seqRecvd == sequenceNumberExpected) {

#ifdef DEBUG
    printf("The sequence # was as expected: %d\n", seqRecvd);
#endif

    sequenceNumberExpected++; 
    if (sequenceNumberExpected == USHRT_MAX) sequenceNumberExpected = 0;    // refer to global variable declaration

    return 1;
  } else {
#ifdef DEBUG
    printf("The sequence # was not as expected: recvd=%d, expect=%d\n", seqRecvd, sequenceNumberExpected);
#endif
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
int verifyChksum(u_char *recvdDatagram, uint16_t chkRecvd, int dGramSize)
{
  uint32_t sum = 0;
  uint16_t calcdChk = 0;

  // Make pseudo header for checksum calculation
  recvdDatagram[4] = pseudoChksum >> 8;
  recvdDatagram[5] = pseudoChksum;

  calcdChk = calcChecksum(recvdDatagram, dGramSize, sum);

#ifdef DEBUG
  printf("Received Chk: %u, Calc'd Chk: %u\n", (unsigned int) chkRecvd, (unsigned int)calcdChk);
#endif

  if (calcdChk == chkRecvd) {
    //printf("The checksums matched\n");
    return 1;
  } else {
    printf("Received Chk: %u, Calc'd Chk: %u\n", (unsigned int) chkRecvd, (unsigned int)calcdChk);
    printf("The checksums did not matched\n");    
    return 0;
  }
}   

/**
 * randZeroToOne - returns a random number between 0 - 1
 *
 * Note: rand() must be seeded prior to calls using srand()
 *
 * Return: double - the random number generated
 **/
double randZeroToOne() {
  return rand() / (RAND_MAX + 1.);
}

/**
 * wasDropped - gets a random number and if it is <= the drop prob it indicates a drop 
 * 	by returning true
 * @drop_prob: the artificial probablity that a packet is dropped
 *
 * Return: int - 1 if the packet should be dropped, 0 otherwise
 **/
int wasDropped(double drop_prob) {
  double randGend = randZeroToOne();

#ifdef DEBUG
  printf("randGend: %f, drop_prob: %f\n", randGend, drop_prob);
#endif

  if(randGend <= drop_prob) return 1;
  return 0;	
} 

int main(int argc, char *argv[])
{
  int sockfd, portno;                // The socket file descriptor, port number, and size of rcvdDatagram
  ssize_t recsize;
  double drop_prob;                           // Probablity a packet is dropped
  char *file_name;                            // The file to write to and the buffer to store the datagram
  u_char recvdDatagram[BUFFER_SIZE] = {0};    // Buffer for receiving datagram
  u_char ackDatagram[ACK_DGRAM_SIZE] = {0};		// Buffer for ACKs
  socklen_t clientLen;                        // Stores the size of the clients sockaddr_in 
  struct sockaddr_in server_addr;             // Sockadder_in structs that store IP address, port, and etc for the server and its client. 
  uint32_t seqRecvd, chkRecvd, flagRecvd;			// Stores the sequence #, checksum, and flag from the received datagram
  uint32_t lastACKseq;

  if (argc < 4) {
    fprintf(stderr,"usage: %s port# file-name probablity\n", argv[0]);
    exit(1);
  }

	//*** Init - Begin ***  

  portno = atoi(argv[1]);
  file_name = argv[2];
  drop_prob = atof(argv[3]);

	srand(time(NULL));		// Sends the RNG

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
    error("ERROR on binding the socket");
  } 

  fileToWrite = fopen(argv[2], "w");
  if(fileToWrite == NULL) error("Error opening the file\n");

  //*** Init - End ***

  //*** The client processes are ready to begin ***

  int numTimesFailed = 0;

  while(1)
  {
    // Accepts a connection from the client and creates a new socket descriptor
    // to handle communication between the server and the client. 
    recsize = recvfrom(sockfd, (void*)recvdDatagram, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &clientLen);
    if (recsize < 0) error("ERROR on recvfrom");

#ifdef DEBUG
    printf("receivesize: %d\n", recsize);
#endif

    //Retrieve header
    seqRecvd = (recvdDatagram[0] <<  24) | (recvdDatagram[1] << 16) | (recvdDatagram[2] << 8) | recvdDatagram[3];
    chkRecvd = (recvdDatagram[4] << 8) | recvdDatagram[5];
    flagRecvd = (recvdDatagram[6] << 8) | recvdDatagram[7];

#ifdef DEBUG
    printf("Seq: %u, Chk: %u, Flag: %u\n", seqRecvd, chkRecvd, flagRecvd);
#endif

    if (flagRecvd == closeFlag) {
      printf("The client has closed the connection\n");      
      break;
    }

    if(!wasDropped(drop_prob)) {
      if ( verifyChksum(recvdDatagram, chkRecvd, recsize) && verifySequence(seqRecvd) ) {
  	sendAck(&sockfd, &server_addr, ackDatagram, seqRecvd);      
        fwrite(&recvdDatagram[8] , sizeof(char), recsize-8, fileToWrite);
        lastACKseq = seqRecvd;
        numTimesFailed = 0;
      } else {
        numTimesFailed++;
        if (numTimesFailed >= MAX_TIMES_FAIL) {
          printf("Attempting to resend ack for %d\n", lastACKseq);
          sendAck(&sockfd, &server_addr, ackDatagram, lastACKseq);
          numTimesFailed = 0;
        }
      }
    } else {
      printf("Packet loss, sequence number = %d\n", seqRecvd);
    }

    clearBuffers(recvdDatagram, ackDatagram);
  }

  close(sockfd);
  fclose(fileToWrite);  
  exit(0); 
}
