/*
**CAB403 Semester 02; 2018
**Assignment: Process Management and Distributed Computing
**Assignment 1
**
**Server Side
**Author: Luke Goeree 20/09/2018
*/

//Included Libraries
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
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
#define DEFAULT_SERVER_PORT 12345
#define NUM_THREADS 10
#define REQUEST_BACKLOG 10
#define STANDARD_ERROR -1
#define SOCKET_ERROR -1
#define COMM_ERROR 1
#define DATA_ONE 1
#define DATA_TWO 2
#define COMM_TERMINATE COMM_ERROR
#define COMM_NORMAL 0
#define RANDOM_NUMBER_SEED 42
#define TERMINATE_VALUE USHRT_MAX
#define PLYR_DETS_LEN 128
#define GAME_WON 1
#define GAME_LOST 2
#define NUM_TILES_X 9
#define NUM_TILES_Y 9
#define NUM_MINES 10

/**Borrowed Struct to enable boolean types
https://stackoverflow.com/questions/1921539/using-boolean-values-in-c**/
typedef enum{
  false,true
} bool;

//Custom Structs
/**Custome Struct to store state of the tile**/
typedef struct Tile{
  int adjacent_mines;
  bool revealed;
  bool is_mine;
}Tile;

/**Custome Struct to describe the games' state**/
typedef struct GameState{
  // Additional fields
  int placed_flags;
  bool game_over;
  Tile tiles[NUM_TILES_X][NUM_TILES_Y];
} GameState;

/**Custome Struct to describe all authenticated players**/
typedef struct authedPlyr{
  char name[PLYR_DETS_LEN];
  char password[PLYR_DETS_LEN];
} authedPlyr_t;

/**Custome Struct to describe entry to leaderboard**/
typedef struct leader{
  int authedPlyrID;
  int gamesWon;
  int gamesPlayed;
  int gameTime;
  struct leader* prev;
  struct leader* next;
} leader_t;

/**Custome Struct to describe client connection requests**/
typedef struct request{
  int fd;
  struct request* next;
} request_t;

//Global Variables
authedPlyr_t* authedPlyrs;
int numPlyrs = -1; //skip past the headers
leader_t* ldrHead = NULL;
leader_t* ldrTail = NULL;
int numLdrs = 0;
request_t* waitHead = NULL;
request_t* waitTail = NULL;
volatile char procServer = 1;


//Process Synchronisation: semaphore for multiple thread lock/unlock
pthread_mutex_t waitMut = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_cond_t gotReq = PTHREAD_COND_INITIALIZER;
sem_t writeMut;
pthread_mutex_t rdCntMut = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
int rdCnt = 0;

//Methods
int sndStrTrig(int fd, char* string);
int rcvStrTrig(int fd, char* string);
int sndIntTrigs(int fd, int* data, int size);
int sndIntTrig(int fd, int data);
int rcvIntTrig(int fd, int* data);
void importPlayers();
void updateLeaderBoard(int authedPlyrID, int status);
int sendClientLeaderBoard(int fd);
int gameRound(int fd, int* status);
void clientOp(int thread_id, int fd);
void* cleanupThreads(void* data);
void* clientRequest(void* data);
void globalCleanup();
void sigint_handler(int signal);

/*
**Main Function
*/
int main(int argc, char* argv[]){
  int socketFd, clientFd, rc, i;
  int threadIDs[NUM_THREADS];
  pthread_t threads[NUM_THREADS];
  struct sockaddr_in serverAddr;
  struct sockaddr_in clientAddr;
  socklen_t sin_size = sizeof(struct sockaddr *);
  int srvrPort = DEFAULT_SERVER_PORT;
  request_t* request;
  struct timespec sleep_spec;
  srand(RANDOM_NUMBER_SEED);
  signal(SIGINT, sigint_handler);
  sleep_spec.tv_sec = 0;
  sleep_spec.tv_nsec = 10000;
  rc = sem_init(&writeMut, 0, 1);

  if(rc!=0){
    perror("sem_init");
    exit(1);
  }
  importPlayers();
  //Ensure Server program has some form of client port
  if(argc!=2){
    srvrPort = DEFAULT_SERVER_PORT;
  } else{
    srvrPort = atoi(argv[1]);
  }

  /**Socket Operations**/
  /** Socket Structures as per 
    www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html**/
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(srvrPort);
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  socketFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if(socketFd==SOCKET_ERROR){
    perror("socket");
    exit(1);
  }
  //Bind Socket to endpoint
  if(bind(socketFd,(struct sockaddr*)&serverAddr,sizeof(struct sockaddr))==-1){
    perror("bind");
    close(socketFd);
    exit(1);
  }
  //Start Listening to socket
  if(listen(socketFd,REQUEST_BACKLOG)==SOCKET_ERROR){
    perror("listen");
    close(socketFd);
    exit(1);
  }
  //Create Threadpool
  for(i=0;i<NUM_THREADS;++i){
    threadIDs[i] = i;
    pthread_create(&threads[i],NULL, clientRequest,(void*)&threadIDs[i]);
  }
  // handle incoming client requests
  while(procServer){
    clientFd = accept(socketFd,(struct sockaddr*)&clientAddr,&sin_size);
    if(clientFd==SOCKET_ERROR){
      if(errno==EAGAIN OR errno==EWOULDBLOCK){
        nanosleep(&sleep_spec,NULL);
      } else{
        perror("accept");
      }
      continue;
    }
    request = malloc(sizeof(*request));
    request->fd = clientFd;
    request->next = NULL;
    pthread_mutex_lock(&waitMut);
    if(waitHead==NULL){
      waitHead = request;
      waitTail = request;
    } else{
      waitTail->next = request;
      waitTail = request;
    }
    pthread_cond_signal(&gotReq);
    pthread_mutex_unlock(&waitMut);
  }
  //Cancel Threadpool
  for(i=0;i<NUM_THREADS;++i){
    pthread_cancel(threads[i]);
    pthread_join(threads[i],NULL);
  }
  globalCleanup();
  close(socketFd);
  return 0;
}


int sndStrTrig(int fd, char* string){
  int i, rc;
  char str_ended = 0;
  uint16_t num;
  for (i = 0; i < PLYR_DETS_LEN; ++i){
    if (str_ended){
      num = htons(0);
    } else{
      num = htons(string[i]);
      if (string[i] == '\0') str_ended = 1;
    }
    rc = send(fd, &num, sizeof(uint16_t), 0);
    if (rc == SOCKET_ERROR){
      return COMM_ERROR;
    }
  }
  return COMM_NORMAL;
}

/*
**String sent for socket logging
**http://pubs.opengroup.org/onlinepubs/000095399/functions/recv.html
*/
int rcvStrTrig(int fd, char* string){
  int rc;
  uint16_t num;
  for (int i=0;i<PLYR_DETS_LEN;++i){
    rc = recv(fd,&num,sizeof(uint16_t),0);
    if (rc==SOCKET_ERROR){
      return COMM_ERROR;
    }
    if (ntohs(num)==TERMINATE_VALUE){
      return COMM_TERMINATE;
    }
    string[i] = (char) ntohs(num);
  }
  return COMM_NORMAL;
}

/*
**sndIntTrigs:Utility method to process int arrays to client
*/
int sndIntTrigs(int fd, int* data, int size){
  int rc;
  uint16_t num = htons(size);
  rc = send(fd,&num,sizeof(uint16_t),0);
  if(rc==SOCKET_ERROR){
    return COMM_ERROR;
  }
  for (int i=0;i<size;++i){
    num = htons(data[i]);
    rc = send(fd,&num,sizeof(uint16_t),0);
    if(rc==SOCKET_ERROR){
      return COMM_ERROR;
    }
  }
  return COMM_NORMAL;
}

/*
**sndIntTrig:Utility method to process int codes to client
*/
int sndIntTrig(int fd, int data){
  int rc;
  uint16_t num = htons(data);
  rc = send(fd,&num,sizeof(uint16_t),0);
  if(rc==SOCKET_ERROR){
    return COMM_ERROR;
  }
  return COMM_NORMAL;
}

/*
**rcvIntTrig:Utility method to process int codes from client
*/
int rcvIntTrig(int fd, int* data){
  int rc;
  uint16_t num;
  rc = recv(fd,&num,sizeof(uint16_t),0);
  if(rc==SOCKET_ERROR){
    return COMM_ERROR;
  }
  if(ntohs(num)==TERMINATE_VALUE){
    return COMM_TERMINATE;
  }
  *data = (int) ntohs(num);
  return COMM_NORMAL;
}

/*
**Function to import users from Authentication.txt into authedPlayer struct types
*/
void importPlayers(){
  FILE* fp;
  char* line = NULL;
  size_t len = 0;
  ssize_t read;
  char after_white;

  fp = fopen("Authentication.txt", "r");
  if (fp == NULL){
    perror("fopen");
    exit(1);
  }
  while ((read = getline(&line,&len,fp))!=SOCKET_ERROR){
    numPlyrs++;
  }
  authedPlyrs = malloc(numPlyrs * sizeof(*authedPlyrs));
  rewind(fp);

  for (int i = -1; (read = getline(&line,&len,fp))!=SOCKET_ERROR;++i){
    if (i==SOCKET_ERROR){
      continue; // skip first header line
    }
    after_white = 0;
    int j = 0;

    while (1){
      if (line[j]=='\n' OR line[j]=='\r' OR line[j]=='\0'){
        authedPlyrs[i].password[j-after_white] = '\0';
        break; 
      } else if(line[j]!=' ' AND line[j]!='\t'){
        if ((j>0 AND line[j-1]==' ') OR line[j-1]=='\t'){
          after_white = j;
        }
        if(after_white){
          authedPlyrs[i].password[j-after_white] = line[j];
        } else{
          authedPlyrs[i].name[j] = line[j];
        }
        if(line[j+1]==' ' OR line[j+1]=='\t'){
          authedPlyrs[i].name[j+1] = '\0';
        }
      }
      j++;
    }
  }
  fclose(fp);
}

/*
**updateLeaderBoard: formats and orders the leaderboard
*/
void updateLeaderBoard(int authedPlyrID, int status){
  leader_t* entry;
  leader_t* l;
  char create_new = 1;
  char insert = 0;

  if(ldrHead!=NULL){
    for(entry=ldrHead;entry!=NULL;entry = entry->next){
      if(entry->authedPlyrID==authedPlyrID){
        create_new = 0;
        break;
      }
    }
  }
  if(create_new){
    entry = malloc(sizeof(*entry));
    entry->authedPlyrID = authedPlyrID;
    entry->gamesWon = status==GAME_WON ? 1:0;
    entry->gamesPlayed = 1;
    entry->prev = NULL;
    entry->next = NULL;
    insert = 1;
    numLdrs++;
  } else{
    entry->gamesPlayed++;
    if(status==GAME_WON){
      entry->gamesWon++;
      insert = 1;
      if(entry->next!=NULL){
        if(entry->prev==NULL){
          entry->next->prev = NULL;
          ldrHead = entry->next;
        } else{
          entry->next->prev = entry->prev;
        }
      }
      if(entry->prev!=NULL){
        if(entry->next==NULL){
          entry->prev->next = NULL;
          ldrTail = entry->prev;
        } else{
          entry->prev->next = entry->next;
        }
      }
      if(ldrHead==entry AND ldrTail==entry){
        ldrHead = NULL;
        ldrTail = NULL;
      }
    }
  }
  if(insert){
    if(ldrHead==NULL){
      ldrHead = entry;
      ldrTail = entry;
    } else{
      //Ascending order based on games won
      for(l=ldrHead;l!=NULL;l=l->next){
        if(l->gamesWon<entry->gamesWon){
          if(l->next==NULL){ //insertion at tail
            entry->prev = l;
            l->next = entry;
            ldrTail = entry;
            break;
          } else if(l->next->gamesWon>entry->gamesWon){ //insertion after l
            entry->prev = l;
            entry->next = l->next;
            l->next->prev = entry;
            l->next = entry;
            break;
          }
        } else if(l->prev==NULL){ //insertion at head
          entry->next = l;
          l->prev = entry;
          ldrHead = entry;
          break;
        } else if(l->prev->gamesWon<entry->gamesWon){ //insertion before l
          entry->prev = l->prev;
          entry->next = l;
          l->prev->next = entry;
          l->prev = entry;
          break;
        }
      }
    }
  }
}

/*
**sendClientLeaderBoard: transfers leaderbaord to client screen
*/
int sendClientLeaderBoard(int fd){
  leader_t* l;
  if(sndIntTrig(fd,numLdrs)==COMM_ERROR){
    return COMM_ERROR;
  } 
  for(l=ldrHead;l!=NULL;l=l->next){
    if(sndStrTrig(fd,authedPlyrs[l->authedPlyrID].name)==COMM_ERROR){
      return COMM_ERROR;
    } 
    if(sndIntTrig(fd,l->gamesWon)==COMM_ERROR){
      return COMM_ERROR;
    } 
    if(sndIntTrig(fd,l->gamesPlayed)==COMM_ERROR){
      return COMM_ERROR;
    } 
  }
  return 0;
}

/*
**TO DO://
**gameRound: server side game logic loop
*/
int gameRound(int fd, int* status){
  printf("Play Game loop: Server\n\n\n");
}

/*
**clientOP: Method to process incoming program request from clients
*/
void clientOp(int thread_id, int fd){
  char authedPlyrname[PLYR_DETS_LEN];
  char password[PLYR_DETS_LEN];
  int authedPlyrID = STANDARD_ERROR;
  int rc, code, status;

  if(rcvStrTrig(fd,authedPlyrname)==COMM_ERROR){
    printf("recv error (authedPlyrname): closing client #%i\n",fd);
    fflush(stdout);
    return;
  }

  if(rcvStrTrig(fd,password)==COMM_ERROR){
    printf("recv error (password): closing client #%i\n",fd);
    fflush(stdout);
    return;
  }

  for(int i=0;i<numPlyrs;++i){
    if(strcmp(authedPlyrname,authedPlyrs[i].name)==0){
      authedPlyrID = i;
      break;
    }
  }

  if(authedPlyrID==STANDARD_ERROR OR strcmp(password,authedPlyrs[authedPlyrID].password)!=0){
    sndIntTrig(fd, 0); // login rejection
    return;
  }

  // login success
  if(sndIntTrig(fd,DATA_ONE)==COMM_ERROR){
    printf("send error (login success): closing client #%i\n", fd);
    fflush(stdout);
    return;
  }

  while(1){
    if(rcvIntTrig(fd,&code)==COMM_ERROR){
      printf("recv error (action code): closing client #%i\n",fd);
      fflush(stdout);
      return;
    }
    if(code==DATA_ONE){
      rc = gameRound(fd,&status);
      if(rc==COMM_ERROR){
        printf("comm error (gameRound): closing client #%i\n",fd);
        fflush(stdout);
        return;
      }
      sem_wait(&writeMut);
      updateLeaderBoard(authedPlyrID,status);
      sem_post(&writeMut);
    } else if(code==DATA_TWO){
      pthread_mutex_lock(&rdCntMut);
      rdCnt++;
      if(rdCnt==DATA_ONE){
        sem_wait(&writeMut);
      }
      pthread_mutex_unlock(&rdCntMut);
      rc = sendClientLeaderBoard(fd);
      pthread_mutex_lock(&rdCntMut);
      rdCnt--;
      if(rdCnt==0){
        sem_post(&writeMut);
      }
      pthread_mutex_unlock(&rdCntMut);
      if(rc==COMM_ERROR){
        printf("comm error (sendClientLeaderBoard): closing client #%i\n",fd);
        fflush(stdout);
        return;
      }
    } else{
      return;
    }
  }
}

/*
**cleanupThreads: Method terminate threads upon process kill
*/
void* cleanupThreads(void* data){
  request_t* req = *((request_t**) data);

  if(req!=NULL){
    sndIntTrig(req->fd,TERMINATE_VALUE);
    shutdown(req->fd,SHUT_RDWR);
    close(req->fd);
    free(req);
  }
  pthread_mutex_unlock(&waitMut);
}

/*
**clientRequest: Method responsible for handling the connections from clients
*/
void* clientRequest(void* data){
  int thread_id = *((int*) data);
  request_t* req = NULL;
  pthread_cleanup_push(cleanupThreads,(void*)&req);
  pthread_mutex_lock(&waitMut);
  while(1){
    pthread_testcancel();
    if(waitHead!=NULL){
      req = waitHead;
      if(waitHead->next!=NULL){
        waitHead = waitHead->next;
      } else{
        waitHead = NULL;
        waitTail = NULL;
      }
      pthread_mutex_unlock(&waitMut);
      printf("client connected (#%i)\n",req->fd);
      fflush(stdout);
      clientOp(thread_id,req->fd);
      shutdown(req->fd,SHUT_RDWR);
      close(req->fd);
      printf("client disconnected (#%i)\n",req->fd);
      fflush(stdout);
      free(req);
      req = NULL;
      pthread_mutex_lock(&waitMut);
    } else{
      pthread_cond_wait(&gotReq,&waitMut);
    }
  }
  pthread_cleanup_pop(cleanupThreads);
}

void globalCleanup(){
  request_t* reqClean;
  request_t* tmpReqClean;
  leader_t* ldrClean;
  leader_t* tmpLdrClean;
  reqClean = waitHead;
  //Clear Any Remaining Requests
  while(reqClean!=NULL){
    sndIntTrig(reqClean->fd,TERMINATE_VALUE);
    shutdown(reqClean->fd,SHUT_RDWR);
    close(reqClean->fd);
    tmpReqClean = reqClean->next;
    free(reqClean);
    reqClean = tmpReqClean;
  }
  ldrClean = ldrHead;
  while(ldrClean!=NULL){
    tmpLdrClean = ldrClean->next;
    free(ldrClean);
    ldrClean = tmpLdrClean;
  }
  free(authedPlyrs);
}

/*
**Signal Interrupt
**Interrupts the process cycle via user input (i.e. CTRL+C)
*/
void sigint_handler(int signal){
  if(signal==SIGINT){
    procServer = 0;
  }
}