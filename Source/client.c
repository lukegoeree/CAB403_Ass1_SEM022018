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
#include <ctype.h>

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
#define GAME_WON 1
#define NUM_TILES_X 9
#define NUM_TILES_Y 9
#define NUM_MINES 10
#define ASCII_BUFF 65

/**Borrowed Struct to enable boolean types
https://stackoverflow.com/questions/1921539/using-boolean-values-in-c**/
typedef enum{
  false,true
} bool;

//Custom Structs
/**Custome Struct to store state of the tile**/
typedef struct Tile{
  int adjMine;
  bool revealed;
  bool isMine;
}Tile;

/**Custome Struct to describe the games' state**/
typedef struct GameState{
  // Additional fields
  int flagPlaced;
  bool gameOver;
  Tile tiles[NUM_TILES_X][NUM_TILES_Y];
} GameState;

//Global Variables
volatile int fd;
char gameBoardServer[NUM_TILES_X][NUM_TILES_Y];
char gameBoardClient[NUM_TILES_X][NUM_TILES_Y];
bool tileRev[NUM_TILES_X][NUM_TILES_Y];
int mineLocs[NUM_MINES][2];
int minesLeft = NUM_MINES;
char usrOption;
bool glblUsrInputVal;
int xCoord, yCoord;
char *mine = "*";
char *mineRevealed = "x";
char *flag = "+";
char *tileDormant = "-";
int glblGameInProc = 0;
bool glblIsPosVal = false;
int glblDebug = 1;
int glblUsrCoord;

//Methods
bool isValidTile(int x, int y);
bool tileHasMine(int x, int y);
int adjacentMineCount(int x, int y);
void initGameBoards();
void drawBoardVisual();
void revealTile(int x, int y);
void openSafeTiles(int x, int y);
void unleashMines();
void initMineLocs();
int sendstr(int fd, char* string);
int recvstr(int fd, char* string);
int sendint(int fd, int data);
int recvint(int fd, int* data);
void exit_client(int code);
void login(char* username);
int main_menu();
void play_game(char* name);
void dispLeaderBoard();
void sigint_handler(int signal);

/*
**Main Function
*/
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
      initGameBoards();
      initMineLocs();
      glblGameInProc = 1;
      while(glblGameInProc==1){
        play_game(username);
      }
    } else if(rc==2){
      dispLeaderBoard();
    } else{
      printf("Thank you for playing!\n");
      exit_client(0);
    }
  }
  return 0;
}

bool isValidTile(int x, int y){
  return (x>=0) AND (x<NUM_TILES_X) AND (y>=0) AND (y<NUM_TILES_Y);
}

bool tileHasMine(int x, int y){
  if(gameBoardServer[x][y]==*mine){
    return true;
  } else{
    return false;
  }
}

int adjacentMineCount(int x, int y){
  int count = 0;
  if(tileHasMine(x,y)){
    count = -1;
  } else{
    if(isValidTile(x-1,y)){
      if(tileHasMine(x-1,y)){
        count++;
      }
    }
    if(isValidTile(x+1,y)){
      if(tileHasMine(x+1,y)){
        count++;
      }
    }
    if(isValidTile(x,y+1)){
      if(tileHasMine(x,y+1)){
        count++;
      }
    }
    if(isValidTile(x,y-1)){
      if(tileHasMine(x,y-1)){
        count++;
      }
    }
    if(isValidTile(x-1,y-1)){
      if(tileHasMine(x-1,y-1)){
        count++;
      }
    }
    if(isValidTile(x-1,y+1)){
      if(tileHasMine(x-1,y+1)){
        count++;
      }
    }
    if(isValidTile(x+1,y+1)){
      if(tileHasMine(x+1,y+1)){
        count++;
      }
    }
    if(isValidTile(x+1,y-1)){
      if(tileHasMine(x+1,y-1)){
         count++;
      }
    }
  }
  return count;
}

void initGameBoards(){
  for(int i=0;i<NUM_TILES_X;i++){
    for(int j=0;j<NUM_TILES_Y;j++){
      gameBoardClient[j][i] = *tileDormant;
      gameBoardServer[j][i] = *tileDormant;
    }
  }
}

/*
**drawBoardVisual: formats and diplays the gameBoard for client connection
*/
void drawBoardVisual(){
  //The first section is to allow extendable gameboard
  printf("\n    ");
  for(int t=1;t<=NUM_TILES_X;t++){
    printf("%d ",t);
  }     
  printf("\n");
  int boardLen = 3 + (NUM_TILES_X*2);
  for(int u=1;u<=boardLen;u++){
    printf("-");
  }
  printf("\n");
  for(int i=0;i<NUM_TILES_X;i++){
    for(int j=0;j<NUM_TILES_Y;j++){
      if(j==0){
        //printf("%c | ",i+ASCII_BUFF);
        printf("%d | ",i+1);
      }
      printf("%c",gameBoardClient[j][i]);
      printf(" ");
    } 
    printf("\n");
  }
  printf("\n");
}

void revealTile(int x, int y){
  int tileNoMine = adjacentMineCount(x,y);
  char tileNoMine_c = '0'+tileNoMine;
  if(isValidTile(x,y)){
    gameBoardClient[x][y] = tileNoMine_c;
  }
}

void openSafeTiles(int x, int y){
  if((adjacentMineCount(x,y))==0){
    revealTile(x-1,y);
    if(gameBoardClient[x-1][y]=='0' AND tileRev[x-1][y]==false AND isValidTile(x-1,y)){
      tileRev[x-1][y] = true;
      openSafeTiles(x-1,y); 
    }
    revealTile(x+1,y);
    if(gameBoardClient[x-1][y]=='0' AND tileRev[x-1][y]==false AND isValidTile(x-1,y)){
      tileRev[x-1][y] = true;
      openSafeTiles(x-1,y);
    }
    revealTile(x,y+1);
    if(gameBoardClient[x][y+1]=='0' AND tileRev[x][y+1]==false AND isValidTile(x,y+1)){
      tileRev[x][y+1] = true;
      openSafeTiles(x,y+1);
    }
    revealTile(x,y-1);
    if(gameBoardClient[x][y-1]=='0' AND tileRev[x][y-1]==false AND isValidTile(x,y-1)){
      tileRev[x][y-1] = true;
      openSafeTiles(x,y-1);
    }
    revealTile(x-1,y+1);
    if(gameBoardClient[x-1][y+1]=='0' AND tileRev[x-1][y+1]==false AND isValidTile(x-1,y+1)){
      tileRev[x-1][y+1] = true;
      openSafeTiles(x-1,y+1);
    }
    revealTile(x-1,y-1);
    if(gameBoardClient[x-1][y-1]=='0' AND tileRev[x-1][y-1]==false AND isValidTile(x-1,y-1)){
      tileRev[x-1][y-1] = true;
      openSafeTiles(x-1,y-1);
    }
    revealTile(x+1,y+1);
    if(gameBoardClient[x+1][y+1]=='0' AND tileRev[x+1][y+1]==false AND isValidTile(x+1,y+1)){
      tileRev[x+1][y+1] = true;
      openSafeTiles(x+1,y+1);
    }
    revealTile(x+1,y-1);
    if(gameBoardClient[x+1][y-1]=='0' AND tileRev[x+1][y-1]==false AND isValidTile(x+1,y-1)){
      tileRev[x+1][y-1] = true;
      openSafeTiles(x+1,y-1);
    }
  }
}

void unleashMines() {
  for(int i=0;i<NUM_MINES;i++){
    gameBoardClient[mineLocs[i][0]][mineLocs[i][1]] = *mine;
  }
}

void initMineLocs(){
  for(int i=0;i<NUM_MINES;i++){
    int x, y;
    do {
      x = rand() % NUM_TILES_X;
      y = rand() % NUM_TILES_Y;
    } while (tileHasMine(x,y));
    mineLocs[i][0] = x;
    mineLocs[i][1] = y;
    gameBoardServer[x][y] = *mine;
  }
}

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
  glblUsrInputVal = true;
  int coordSet;
  bool isValPos = false;
  while(glblUsrInputVal==true){
    printf("\n\n\n%d mines left\n\n", minesLeft);
    drawBoardVisual();
    printf("Choose an option(case sensitive): \n");
    printf("<r> Reveal tile\n");
    printf("<p> Place flag\n");
    printf("<q> Quit\n\n");
    printf("Option (r,p,q): ");
    scanf("%c", &usrOption);
    if((usrOption=='r') || (usrOption=='p')){
      while(!isValPos){
        printf("Enter tile coordinates (X,Y): ");
        scanf("%d",&coordSet);
        if (coordSet>10){
          isValPos = true;
        } else{
          printf("Please type numbers\n");
        }
      }
      xCoord = coordSet / 10 - 1;
      yCoord = coordSet % 10 - 1;
      if(usrOption=='r'){
        int tileNoMine = adjacentMineCount(xCoord,yCoord);
        if(tileNoMine==-1){
          unleashMines();
          gameBoardClient[xCoord][yCoord] = *mineRevealed;
          drawBoardVisual();
          printf("\nYou have unleashed a mine. Game over\n\n");
          glblGameInProc = false;
        }
        revealTile(xCoord,yCoord);
        openSafeTiles(xCoord,yCoord);
      } else{
        if(gameBoardServer[xCoord][yCoord]==*mine){
          gameBoardServer[xCoord][yCoord] = *flag;
          gameBoardClient[xCoord][yCoord] = *flag; 
          minesLeft--;           
        } else if(gameBoardClient[xCoord][yCoord]==*flag){
          printf("\nYou have already flagged this tile\n");
        } else{
          printf("\nNo mine there try again\n");
        }
        if(minesLeft==0){
          printf("You won\n");
          glblGameInProc = false;
        }
      }
      glblUsrInputVal = false;
    } else if((usrOption=='q')){
      glblGameInProc = false;
    } else{
      glblUsrInputVal = false;
    }
  }
  
}

void dispLeaderBoard(){
  char name[BUFF_LEN];
  int numLdrs, gamesWon, gamesPlyd;
  
  if(recvint(fd,&numLdrs)==COMM_ERROR){
    perror("recvint");
    exit_client(1);
  }
  if(numLdrs>0){
    printf("==============================================================================\n\n");
    for(int i=0;i<numLdrs;++i){
      if(recvstr(fd,name)==COMM_ERROR){
        perror("recvstr");
        exit_client(1);
      }
      if(recvint(fd,&gamesWon)==COMM_ERROR){
        perror("recvint");
        exit_client(1);
      }
      if(recvint(fd,&gamesPlyd)==COMM_ERROR){
        perror("recvint");
        exit_client(1);
      }
      printf("%s\t\t999 seconds\t\t%i games won, %i games played\n",name,gamesWon,gamesPlyd);
    }
    printf("\n==============================================================================\n\n");
  } else{
    printf("==============================================================================\n\n");
    printf("There is no information currently in the leaderboard. Try again later.\n\n");
    printf("==============================================================================\n\n");
  }
}

void sigint_handler(int signal){
  if(signal==SIGINT){
    sendint(fd,TERMINATE_VALUE);
    printf("\nThank you for playing!\n");
    exit_client(0);
  }
}