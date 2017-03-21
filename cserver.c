/* File: cserver.c */
/* Name: Seth Butler */
/* Assignment: 3 */
/* Class: Operating Systems 2 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_NUM_CONNECTIONS 1
#define BUFFER_SIZE 256

FILE *capitalInfo;

/* Stores a state w/ it's state. */
typedef struct {
  char *state;
  char *capital;
} stateCap;

/* Stores the 50 stateCap structures. */
static stateCap dict[50];

/* Prints the error message passed. */
void error(const char *msg)
{
  perror(msg);
  exit(1);
}

/* Reads the capitalinfo file storing the states
    and their capitals. */
void readFile() {
  char line [BUFFER_SIZE];
  int numberRead = 0;

  capitalInfo = fopen("capitalinfo", "r" );

  if(capitalInfo != NULL) {    
    while(fgets(line, BUFFER_SIZE, capitalInfo) != NULL) {

      /* Gets the state & capital from the line. */
      char *tempState = strtok(line, "-");
      char *tempCapital = strtok(NULL, "\n");

      /* Removes trailing space from the state string. */
      tempState[strlen(tempState)-1] = 0;

      /* Allocates the memory for the string and
          Copies the retrieved string into the struct.  */
      dict[numberRead].state = (char *) malloc(16);
      strcpy(dict[numberRead].state, tempState);
  
      /* Removes preceding space. */
      tempCapital++;

      /* Removes trailing newline of capital except for last entry. */
      if(numberRead < 49) {
        tempCapital[strlen(tempCapital)-1] = 0;
      }

      /* Allocates the memory for the string and
          Copies the retrieved string into the struct. */
      dict[numberRead].capital = (char *) malloc(16);
      strcpy(dict[numberRead].capital, tempCapital);

      numberRead++;
    }

    /*for(int index = 0; index <= 50; index++) {
      printf("state: [%s], capital: [%s]\n", dict[index].state, dict[index].capital);
    }*/

    fclose(capitalInfo);
  } else {
    error("ERROR opening the file");
    exit(1);
  }
}

/* Returns the name of the state's capital. */
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

/* The function that performs the server's function
    of returning the capital of a state passed from
    it's client. */
void serverProcess(int newsockfd) {

  /* Read/Write stream buffer. */
  char buffer[BUFFER_SIZE], confirmation[1];

  char *state, *capital, *message;

  /* Max amount of chars able to be read/written to the stream
      and the number read after each read/write. */
  int rwMax, rwChars;

  rwMax = BUFFER_SIZE - 1;

  while(1){

    /* Sets all contents of the buffer to 0. */
    memset(buffer, 0, BUFFER_SIZE);

    /* Reads what the client has sent to the server. */
    rwChars = read(newsockfd,buffer, rwMax);

    if (rwChars < 0) error("ERROR reading from socket");

    /* Terminates when END is received from client. */
    if(strcmp(buffer, "END") == 0) {
      printf("Terminating client connection\n");
      break;
    }

    state = buffer;

    capital = getCapital(state);

    /* Prepares the message to be sent to the client. */
    sprintf(message, "%s's capital: %s", state, capital);

    /* Writes to the client */
    rwChars = write(newsockfd, message, rwMax);

    if (rwChars < 0) error("ERROR writing to socket");
  }
}


int main(int argc, char *argv[])
{
  /* The socket file descriptors, port number,
      and the */
  int sockfd, newsockfd, portno;

  /* Stores the size of the clients sockaddr_in */
  socklen_t clientLen;

  /* Sockadder_in structs that store IP address, port, and
      etc for the server and its client. */
  struct sockaddr_in server_addr, client_addr;  

  pid_t childPid;

  if (argc < 2) {
    fprintf(stderr,"ERROR, no port provided\n");
    exit(1);
  }

  readFile();

  /*AF_INET is for the IPv4 protocol. SOCK_STREAM represents a 
      Stream Socket. 0 uses system default for transportation
      protocol. In this case will be TCP (continuous & reliable). */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) error("ERROR opening socket");

  /* Sets all variables in the serv_addr struct to 0 to prevent "junk" 
      in the variables. "Always pass structures by reference w/ the 
      size of the structure." */
  //bzero((char *) &server_addr, sizeof(server_addr));
  memset((char*) &server_addr, 0, sizeof(server_addr));

  portno = atoi(argv[1]);

  /* Internet Address Family */
  server_addr.sin_family = AF_INET;

  /* Port Number in Network Byte Order */
  server_addr.sin_port = htons(portno);

  /* IP Address in Network Byte Order. In this case it is always 
      the address on which the server is running. INADDR_ANY gets 
      this address. */
  server_addr.sin_addr.s_addr = INADDR_ANY;

  /* Binds the servers local protocol to the socket. Keeps 
      the socket reserved and open for this specific
      process. */
  if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) error("ERROR on binding");

  /* Has the process now listen for incoming client connections and
      specifies the maximum number of connections allowed. */
  listen(sockfd, MAX_NUM_CONNECTIONS);

  clientLen = sizeof(client_addr);

  while(1) {
    /* Accepts a connection from the client and creates a new socket descriptor
        to handle communication between the server and the client. */
    newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &clientLen);
    if (newsockfd < 0) {
      error("ERROR on accept");
      exit(1);
    }
  
    /* Creates the child processes for each client connected. */
    childPid = fork();

    if (childPid < 0) {
      perror("Error on fork");
      exit(1);
    }

    if (childPid == 0) {
      /*  Client Process */
      close(sockfd);
      serverProcess(newsockfd);
      exit(0);
    } else {
      close(newsockfd);
    }

  }

  return 0; 
}
