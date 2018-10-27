/**
*CAB403 Semester 02; 2018
*Assignment: Process Management and Distributed Computing
*Assignment 1
*
*Client Side
*Author: Luke Goeree 20/09/2018
**/

//Included Libraries
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <limits.h>

//Static Definitions
#define OR ||
#define AND &&
#define BUFF_LEN 128
#define SOCKET_ERROR -1
#define COMM_ERROR 1
#define COMM_TERMINATE COMM_ERROR
#define COMM_NORMAL 0
#define TERMINATE_VALUE USHRT_MAX
#define MSG_LEN 128

//Global Variables
volatile int fd;

int sendstr(int fd, char* string){
  int rc;
  char str_ended = 0;
  uint16_t num;

  for(int i=0;i<MSG_LEN;++i){
    if(str_ended){
      num = htons(0);
    } else{
      num = htons(string[i]);
      if(string[i]=='\0'){
        str_ended = 1;
      }
    }
    rc = send(fd,&num,sizeof(uint16_t),0);
    if(rc==SOCKET_ERROR){
      return COMM_ERROR;
    }
  }
  return COMM_NORMAL;
}

int recvstr(int fd, char* string){
  int rc;
  uint16_t num;

  for (int i=0;i<MSG_LEN;++i){
    rc = recv(fd,&num,sizeof(uint16_t),0);
    if(rc==SOCKET_ERROR){
      return COMM_ERROR;
    }
    if(ntohs(num)==TERMINATE_VALUE){
      return COMM_TERMINATE;
    }
    string[i] = (char) ntohs(num);
  }
  return COMM_NORMAL;
}

int sendint(int fd, int data){
  int rc;
  uint16_t num = htons(data);
  rc = send(fd,&num,sizeof(uint16_t),0);
  if(rc==SOCKET_ERROR){
    return COMM_ERROR;
  }
  return COMM_NORMAL;
}

int recvint(int fd, int* data){
  int rc;
  uint16_t num;
  rc = recv(fd, &num,sizeof(uint16_t),0);
  if(rc==SOCKET_ERROR){
    return COMM_ERROR;
  }
  if(ntohs(num)==TERMINATE_VALUE){
    return COMM_TERMINATE;
  }
  *data = (int) ntohs(num);
  return COMM_NORMAL;
}

void exit_client(int code){
  fflush(stdout);
  shutdown(fd,SHUT_RDWR);
  close(fd);
  exit(code);
}

void login(char* username){
  int status;
  char password[MSG_LEN];

  printf("===============================================\n");
  printf("WELCOME TO THE ONLINE MINESWEEPER GAMING SYSTEM\n");
  printf("===============================================\n\n");
  printf("You are required to logon with your registered username and password\n\n");
  printf("Username: ");
  scanf("%s", username);
  printf("Password: ");
  scanf("%s", password);
  if(sendstr(fd, username)==COMM_ERROR){
    perror("send");
    exit_client(1);
  }
  if(sendstr(fd,password)==COMM_ERROR){
    perror("send");
    exit_client(1);
  }
  if(recvint(fd,&status)==COMM_ERROR){
    perror("recv");
    exit_client(1);
  } else if(status==0){
    printf("You entered either an incorrect username or password. Disconnecting.\n");
    fflush(stdout);
    exit_client(0);
  }
}

int main_menu(){
  int scanned;
  int option = -1;
  char input[BUFF_LEN];

  fputs("Please enter an option\n", stdout);
  fputs("<1>\tPlay Minesweeper\n", stdout);
  fputs("<2>\tShow Leaderboard\n", stdout);
  fputs("<3>\tQuit\n\n", stdout);
  do{
    fputs("Selection (1-3): ",stdout);
    fgets(input, BUFF_LEN, stdin);
    while(input[0]=='\n'){
      fgets(input, BUFF_LEN, stdin);
    }
    scanned = sscanf(input,"%i",&option);
    if(scanned!=1 OR option<1 OR option>3){
      printf("Option must be a number between 1 and 3.\n");
      option = -1;
    }
  } while(option==-1);
  return option;
}

void play_game(char* name){
  printf("Play Game loop: Client\n\n\n");
}

void show_leaderboard(){
  char name[BUFF_LEN];
  int num_leaders, games_won, games_played;
  
  if(recvint(fd,&num_leaders)==COMM_ERROR){
    perror("recvint");
    exit_client(1);
  }
  if(num_leaders>0){
    for(int i=0;i<num_leaders;++i){
      if(recvstr(fd,name)==COMM_ERROR){
        perror("recvstr");
        exit_client(1);
      }
      if(recvint(fd,&games_won)==COMM_ERROR){
        perror("recvint");
        exit_client(1);
      }
      if(recvint(fd,&games_played)==COMM_ERROR){
        perror("recvint");
        exit_client(1);
      }
      printf("================================\n\n");
      printf("Player :  %s\n",name);
      printf("Number of games won:    %i\n",games_won);
      printf("Number of games played: %i\n\n",games_played);
    }
    printf("================================\n\n");
  } else{
    printf("================================\n\n");
    printf("There is no information currently in the leaderboard. Try again later.\n\n");
    printf("================================\n\n");
  }
}

void sigint_handler(int signal){
  if(signal==SIGINT){
    sendint(fd,TERMINATE_VALUE);
    printf("\nThank you for playing!\n");
    exit_client(0);
  }
}

int main(int argc, char* argv[]){
  int port, rc;
  struct hostent* host;
  struct sockaddr_in server_addr;
  char username[MSG_LEN];

  if(argc<3){
    printf("Client requires a hostname and port number to connect to\n");
    exit(1);
  }
  signal(SIGINT,sigint_handler);
  port = atoi(argv[2]);
  host = gethostbyname(argv[1]);
  if(host==NULL){
    herror("gethostbyname");
    exit(1);
  }
  fd = socket(AF_INET,SOCK_STREAM,0);
  if(fd==SOCKET_ERROR){
    perror("socket");
    exit_client(1);
  }
  memset(&server_addr,0,sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr = *((struct in_addr*) host->h_addr);
  if(connect(fd,(struct sockaddr*)&server_addr,sizeof(struct sockaddr))==SOCKET_ERROR){
    perror("connect");
    exit_client(1);
  }
  login(username);
  while(1){
    rc = main_menu();
    sendint(fd, rc);
    if(rc==1){
      play_game(username);
    } else if(rc==2){
      show_leaderboard();
    } else{
      printf("Thank you for playing!\n");
      exit_client(0);
    }
  }
  return 0;
}