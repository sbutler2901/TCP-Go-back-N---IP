// File: client.c 
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

#define MAX_NUM_CONNECTIONS 1
#define BUFFER_SIZE 256


/*

/**
 * cpupri_find - find the best (lowest-pri) CPU in the system
 * @cp: The cpupri context
 * @p: The task
 * @lowest_mask: A mask to fill in with selected CPUs (or NULL)
 *
 * Note: This function returns the recommended CPUs as calculated during the
 * current invocation.  By the time the call returns, the CPUs may have in
 * fact changed priorities any number of times.  While not ideal, it is not
 * an issue of correctness since the normal rebalancer logic will correct
 * any discrepancies created by racing against the uncertainty of the current
 * priority configuration.
 *
 * Return: (int)bool - CPUs were found
int cpupri_find(struct cpupri *cp, struct task_struct *p,
    struct cpumask *lowest_mask)
{
  */

//unsigned long sequenceNumber = 0;
uint16_t pseudoChksum = 0b0000000000000000;
uint16_t ackFlag = 0b1010101010101010;
uint16_t dataFlag = 0b0101010101010101;   // (21,845) - base 10
uint16_t closeFlag = 0b1111111111111111;

// Prints the error message passed. 
void error(const char *msg)
{
  perror(msg);
  exit(1);
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

// void sendAck(int *sockfd, char *buffer, struct sockaddr_in *server_addr, unsigned long sequenceNumber)
//{
//   char datagram[BUFFER_SIZE];

//   snprintf(datagram, BUFFER_SIZE, "%lu%d%d", sequenceNumber, zeroChksum, ackFlag);

//   int sendsize = sendto(*sockfd, datagram, strlen(datagram), 0, (struct sockaddr*) server_addr, sizeof(*server_addr));
//   if(sendsize < 0) {
//     error("Error sending the packet:");
//     exit(EXIT_FAILURE);
//   } else {
//     printf("sendsize: %d\n", sendsize);
//   }
// //  sequenceNumber++;
// }

void printBuffer(u_char *buffer)
{
  for (int i=0; i<20; i++) {
    printf("pre chck: %u\n", (unsigned int)buffer[i]);
  }
}

int main(int argc, char *argv[])
{
  int sockfd, portno;                         // The socket file descriptor & port number
  double drop_prob;                           // Probablity a packet is dropped

  
  char *file_name;                            // The file to write to and the buffer to store the datagram
  //u_char buffer[BUFFER_SIZE] = {0};           // Buffer for ACKs
  u_char recvdDatagram[BUFFER_SIZE] = {0};    // Buffer for receiving datagram

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

  while (1)
  {
    // Accepts a connection from the client and creates a new socket descriptor
    // to handle communication between the server and the client. 
    recsize = recvfrom(sockfd, (void*)recvdDatagram, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &clientLen);
    if (recsize < 0) {
      error("ERROR on recvfrom");
      exit(1);
    }
    printf("receivesize: %d\n", recsize);

    for (int i=0; i<20; i++) {
      printf("pre chck: %u\n", (unsigned int)recvdDatagram[i]);
    }
    printf("letter: %c\n", (char)recvdDatagram[8]);

    //Retrieve header
    uint32_t seqRecvd = (recvdDatagram[0] <<  24) | (recvdDatagram[1] << 16) | (recvdDatagram[2] << 8) | recvdDatagram[3];
    uint16_t chkRecvd = (recvdDatagram[4] << 8) | recvdDatagram[5];
    uint16_t flagRecvd = (recvdDatagram[6] << 8) | recvdDatagram[7];
    printf("Seq: %u, Chk: %u, Flag: %u\n", seqRecvd, chkRecvd, flagRecvd);

    if (flagRecvd == closeFlag) {
      break;
    }


//    printf("pre chck: %u\n", (unsigned int)recvdDatagram[5]);
    // Make pseudo header for checksum calculation
    recvdDatagram[4] = pseudoChksum >> 8;
    recvdDatagram[5] = pseudoChksum;
    printf("post chck: %u\n", (unsigned int)recvdDatagram[5]);

    uint32_t sum;
    uint16_t calcdChk = calcChecksum(recvdDatagram, BUFFER_SIZE, sum);
    printf("Calc'd Chk: %u\n", (unsigned int)calcdChk);

    memset(recvdDatagram, 0, BUFFER_SIZE);

    /*if ( verifyChksum() && verifySequence() ) {
      sendAck();
      writeFile();
    }*/
  }

  printf("The client has closed the connection\n");
  close(sockfd);
  return 0; 
}
