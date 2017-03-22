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
FILE *capitalInfo;

// Stores a state w/ it's state. 
typedef struct {
  char *state;
  char *capital;
} stateCap;

// Stores the 50 stateCap structures. 
static stateCap dict[50];

// Reads the capitalinfo file storing the states
// and their capitals. 
void readFile() {
  char line [BUFFER_SIZE];
  int numberRead = 0;

  capitalInfo = fopen("capitalinfo", "r" );

  if(capitalInfo != NULL) {    
    while(fgets(line, BUFFER_SIZE, capitalInfo) != NULL) {

      // Gets the state & capital from the line. 
      char *tempState = strtok(line, "-");
      char *tempCapital = strtok(NULL, "\n");

      // Removes trailing space from the state string. 
      tempState[strlen(tempState)-1] = 0;

      // Allocates the memory for the string and
       //    Copies the retrieved string into the struct.  
      dict[numberRead].state = (char *) malloc(16);
      strcpy(dict[numberRead].state, tempState);
  
      // Removes preceding space. 
      tempCapital++;

      // Removes trailing newline of capital except for last entry. 
      if(numberRead < 49) {
        tempCapital[strlen(tempCapital)-1] = 0;
      }

      // Allocates the memory for the string and
      //     Copies the retrieved string into the struct. 
      dict[numberRead].capital = (char *) malloc(16);
      strcpy(dict[numberRead].capital, tempCapital);

      numberRead++;
    }

    //for(int index = 0; index <= 50; index++) {
    //   printf("state: [%s], capital: [%s]\n", dict[index].state, dict[index].capital);
    // }

    fclose(capitalInfo);
  } else {
    error("ERROR opening the file");
    exit(1);
  }
}

// Returns the name of the state's capital. 
char *getCapital(char* state) {

  //printf("state = [%s]\n", state);
  for(int index = 0; index < 50; index++) {
    //printf("potentialCap = [%s]\n", dict[index].capital);
    if(strcmp(dict[index].state, state) == 0) {
      //printf("Capital= [%s]\n", dict[index].capital);
      return dict[index].capital;
    }
  }
  return "That was not a state\n";
}
*/

/*
// The function that performs the server's function
//     of returning the capital of a state passed from
//     it's client. 
void serverProcess(int newsockfd) {

  // Read/Write stream buffer. 
  char buffer[BUFFER_SIZE], confirmation[1];

  char *state, *capital, *message;

  // Max amount of chars able to be read/written to the stream
      and the number read after each read/write. 
  int rwMax, rwChars;

  rwMax = BUFFER_SIZE - 1;

  while(1){

    // Sets all contents of the buffer to 0. 
    memset(buffer, 0, BUFFER_SIZE);

    // Reads what the client has sent to the server. 
    rwChars = read(newsockfd,buffer, rwMax);

    if (rwChars < 0) error("ERROR reading from socket");

    // Terminates when END is received from client. 
    if(strcmp(buffer, "END") == 0) {
      printf("Terminating client connection\n");
      break;
    }

    state = buffer;

    capital = getCapital(state);

    // Prepares the message to be sent to the client. 
    sprintf(message, "%s's capital: %s", state, capital);

    // Writes to the client 
    rwChars = write(newsockfd, message, rwMax);

    if (rwChars < 0) error("ERROR writing to socket");
  }
}
*/

//unsigned long sequenceNumber = 0;
unsigned short zeroChksum = 0b0000000000000000;
unsigned short ackFlag = 0b1010101010101010;

// Prints the error message passed. 
void error(const char *msg)
{
  perror(msg);
  exit(1);
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

// void sendAck(int *sockfd, char *buffer, struct sockaddr_in *server_addr, unsigned long sequenceNumber) {
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

int main(int argc, char *argv[])
{
  // The socket file descriptor & port number
  int sockfd, portno;

  // Probablity a packet is dropped
  double drop_prob;

  // The file to write to and the buffer to store the datagram
  char *file_name;
  u_char buffer[BUFFER_SIZE] = {0};

  // Stores the size of the clients sockaddr_in 
  socklen_t clientLen;

  // Stores size of received datagram
  ssize_t recsize;

  // Sockadder_in structs that store IP address, port, and
  // etc for the server and its client. 
  struct sockaddr_in server_addr;  

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

  // Internet Address Family 
  server_addr.sin_family = AF_INET;

  // Port Number in Network Byte Order 
  server_addr.sin_port = htons(portno);

  // IP Address in Network Byte Order. In this case it is always 
  //     the address on which the server is running. INADDR_ANY gets 
   //    this address. 
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

  while (1) {
    // Accepts a connection from the client and creates a new socket descriptor
    // to handle communication between the server and the client. 
    recsize = recvfrom(sockfd, (void*)buffer, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &clientLen);
    if (recsize < 0) {
      error("ERROR on recvfrom");
      exit(1);
    } /*else if ( strstr(buffer, "CLOSE") ) {
      printf("CLOSE was sent: %s\n", buffer);
      break;
    }*/

    uint32_t seqRetrieve = (buffer[0] <<  24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
    printf("seqRetrieve: %u\n", seqRetrieve);

    uint32_t chkRetrieve = (buffer[4] << 8) | buffer[5];
    printf("chkRetrieve: %u\n", chkRetrieve);

    uint32_t dataRetrieve = (buffer[6] << 8) | buffer[7];
    printf("dataRetrieve: %u\n", dataRetrieve); 

    uint32_t tmp0;
    uint32_t tmp1 = calcChecksum(buffer, BUFFER_SIZE, tmp0);
    printf("tmp0: %u, tmp1: %u\n", tmp0, tmp1);

    memset(buffer, 0, BUFFER_SIZE);

    /*if ( verifyChksum() && verifySequence() ) {
      sendAck();
      writeFile();
    }*/
  }

  printf("The client has closed the connection\n");
  close(sockfd);
  return 0; 
}
