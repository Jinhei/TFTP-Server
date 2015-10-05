/* Nicholas Fong
 * TFTP Server Part 4
 * April 17, 2014
 */

// SLEEP FUNCTION COMMENT CODED OUT IN LINE 260

#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

// definitions
#define MAXLENGTH 8192
#define ERROR -1
#define SUCCESS 0

// structures
struct clientState {
  int valid;
  int clientfd;
  char* clientip;
  struct sockaddr_in cliaddr;
  int blockNumber;
  FILE* fp;
  int eof;
};

// function prototypes
int parseRequest(char buf[], char opcode[], char filename[], char mode[]);
int sendData(char filename[], struct clientState* cs);
int sendError(struct clientState* cs);

// main function
int main(int argc, char** argv){
  int sockfd;
  struct sockaddr_in servaddr, recvaddr;
  socklen_t len;
  char opcode[MAXLENGTH];
  int opNumber;
  char buf[MAXLENGTH];
  char filename[MAXLENGTH];
  char mode[MAXLENGTH];
  struct clientState cs[100];
  int i;
  int numClients = 0;
  int maxfd;
  fd_set fdset;
  int newPort = atoi(argv[1]) + 1;

  struct timeval timeout;
  
  // check for port number
  if(argc != 2){
    perror("Proper usage is: ./server <port number>");}

  /*
   * SET UP CONNECTION
   */

  // socket
  if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    perror("Socket error.");}

  // set up settings
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons((unsigned short) atoi(argv[1]));
  if((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) < 0){
    perror("Bind error.");}

  maxfd = sockfd; 

  // receive packet
  while(1){
    if(numClients > 100){
      perror("Too many clients. Please restart the server.");}

    // reset timeout
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    // reset fdset
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);

    for(i = 0; i < numClients; i++){
      FD_SET(cs[i].clientfd, &fdset);
      if(maxfd < cs[i].clientfd){
          maxfd = cs[i].clientfd;
      }
    }

    // select
    if(select(maxfd + 1, &fdset, 0, 0, &timeout) < 0){
      perror("Select error.");}

    // new connection
    if(FD_ISSET(sockfd, &fdset)){
      // set up current clientStatus struct
      cs[numClients].valid = 1;
      cs[numClients].blockNumber = 1;
      cs[numClients].eof = 0;

      // open new socket
      if((cs[numClients].clientfd = socket(AF_INET,SOCK_DGRAM, 0)) < 0){
        perror("New socket error.");
      }

      len = sizeof(cs[numClients].cliaddr);
      recvfrom(sockfd, buf, MAXLENGTH, 0, (struct sockaddr*) &cs[numClients].cliaddr, &len);

      // parse request
      if((opNumber = parseRequest(buf, opcode, filename, mode)) < 0){
        perror("Error parsing request.");}

      // follow request
      switch(opNumber){
        case 1:
          // print request
          printf("%s %s %s from 127.0.0.1:%u\n", opcode, filename, mode, cs[numClients].cliaddr.sin_port);
          //open file
          if((cs[numClients].fp = fopen(filename, "r")) < 0){
            sendError(&cs[numClients]);
            return ERROR;}
          if(sendData(filename, &cs[numClients]) < 0){
            perror("Error sending data.");}
          break;
        case 2:
          // print request
          printf("%s %s %s from 127.0.0.1:%u\n", opcode, filename, mode, cs[numClients].cliaddr.sin_port);
          sendError(&cs[numClients]);
        default:
          sendError(&cs[numClients]);
          break;
      }
    numClients++;
    }

    // loop through all clients
    for(i = 0; i < numClients; i++){
      // check if cs[i] is ready
      if(FD_ISSET(cs[i].clientfd, &fdset)){
        // receive packet
        len = sizeof(cs[i].cliaddr);
        recvfrom(cs[i].clientfd, buf, MAXLENGTH, 0, (struct sockaddr*) &cs[i].cliaddr, &len);


        // parse request
        if((opNumber = parseRequest(buf, opcode, filename, mode)) < 0){
          perror("Error parsing request.");}

        // follow request
        switch(opNumber){
          case 1:
            sendError(&cs[i]);
          case 2:
            sendError(&cs[i]);            
          case 4:
            if(sendData(filename, &cs[i]) < 0){
            perror("Error sending data.");}
            break;
          default:
            sendError(&cs[i]);
            break;}
        }
      }
    
  }

  return SUCCESS;
}

int parseRequest(char buf[], char opcode[], char filename[], char mode[]){
  int k = 2;
  int j = 0;
  int opnumber;

  // copy opcode
  switch(buf[1]){
    case 1:
      strcpy(opcode,"RRQ");
      opnumber = 1;
      break;
    case 2: 
      strcpy(opcode, "WRQ");
      opnumber = 2;
      break;
    case 3: 
      strcpy(opcode, "DATA");
      opnumber = 3;
      break;
    case 4: 
      strcpy(opcode, "ACK");
      opnumber = 4;
      break;
    case 5:
      strcpy(opcode, "ERROR");
      opnumber = 5;
      break;
    default:
      printf("Opcode invalid. Opcode: %d \n", buf[1]);
      return ERROR;
      break;}

  if(opnumber == 1 || opnumber == 2){
    // copy filename
    while((char) buf[k] != (char) 0){
      filename[j] = buf[k];
      j++;
      k++;}
  
    k++;
    j=0;
  
    // copy mode
    while((char) buf[k] != (char) 0){
      mode[j] = buf[k];
      j++;
      k++;}
    }

  else if(opnumber == 4){
    filename[j++] = buf[k++];
    filename[j] = buf[k];
  }
  
  return opnumber;
}

// create and send data packet
int sendData(char filename[], struct clientState* cs){
  char sendbuf[MAXLENGTH];
  int i = 4;
  int bytesRead = 0;

  bzero(sendbuf, sizeof(sendbuf));
  
  // opcode for sendbuf
  sendbuf[0] = 0;
  sendbuf[1] = 3;
  sendbuf[2] = cs->blockNumber/256;
  sendbuf[3] = cs->blockNumber%256;
  
  // read file into sendbuf
  for(i = 4; i < 516; i++){
    // break if end of file
    if (feof(cs->fp)){
      cs->eof = 1;
      break;}
    sendbuf[i] = fgetc(cs->fp);
  }

  sendto(cs->clientfd, sendbuf, i, 0, (struct sockaddr*) &cs->cliaddr, sizeof(cs->cliaddr));
  // sleep(2);

  // Increment block count and reset buffer count
  cs->blockNumber++;
    
  return SUCCESS;
}

// create and send error packet
int sendError(struct clientState* cs){
  int errbuf[MAXLENGTH];

  // error packet
  errbuf[0] = 0;
  errbuf[1] = 5;
  errbuf[2] = 0;
  errbuf[3] = 1;
  strcpy(&errbuf[4], "File not found.");
  errbuf[4+strlen("File not found.")+1] = 0;

  // send error
  sendto(cs->clientfd, errbuf, sizeof(errbuf), 0, (struct sockaddr *)&cs->cliaddr, sizeof(cs->cliaddr));

  return SUCCESS;
}