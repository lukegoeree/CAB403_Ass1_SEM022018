/*
**CAB403 Semester 02; 2018
**Assignment: Process Management and Distributed Computing
**Assignment 1
**
**Server Side
**Author: Luke Goeree 20/09/2018
*/

//Included Libraries
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>

//Static Definitions
#define AND &&
#define OR ||
#define SOCKET_ERROR -1
#define COMM_ERROR 1
#define COMM_TERM COMM_ERROR
#define COMM_NORM 0
#define TERM_VALUE USHRT_MAX
#define USER_DETAILS_BLOCK 30
#define DEFAULT_SERVER_PORT 12345
#define REQUEST_BACKLOG 10
#define AUTH_FILE "Authentication.txt"

//Custom Structs
/**Custome Struct to describe all authenticated players**/
struct authedPlayer{
	char name[USER_DETAILS_BLOCK];
	char password[USER_DETAILS_BLOCK];
};
typedef struct authedPlayer authedPlayer_t;

struct request{
	int serverFd;
	struct request* next;
};
typedef struct request request_t;

//Global Variables
int globalDebugMode = 1;
authedPlayer_t* authedPlayers;
int globalNumUsers = -1;	//skip the file headers
request_t* waitHead = NULL;
request_t* waitTail = NULL;
volatile int globalSrvrProc = 1;

//Process Synchronisation: semaphore for multiple thread lock/unlock
pthread_mutex_t waitMut = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_cond_t gotReq = PTHREAD_COND_INITIALIZER;
pthread_mutex_t readCntMut = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
sem_t writeMut;
int readCnt = 0;

/*
**String sent for socket logging
**http://pubs.opengroup.org/onlinepubs/000095399/functions/recv.html
*/
int recvString(int fd, char* string){
	int remCon;
	uint16_t num;

	for(int i=0;i<USER_DETAILS_BLOCK;i++){
		remCon = recv(fd,&num,sizeof(uint16_t),0);
		if(remCon==SOCKET_ERROR){
			return COMM_ERROR;
		}
		if(ntohs(num)==TERM_VALUE){
			return COMM_TERM;
		}
		string[i] = (char) ntohs(num);
	}
	return COMM_NORM;
}




//handle_client
void clientOp(int threadID, int fd){
	char plyrUsrnm[USER_DETAILS_BLOCK];
	char plyrPwd[USER_DETAILS_BLOCK];

	if(recvString(fd, plyrUsrnm)==COMM_ERROR){
		printf("Error 'USERNAME': Terminating Connection #%i\n", fd);
		fflush(stdout);
		return;
	}
	if(recvString(fd, plyrPwd)==COMM_ERROR){
		printf("Error 'PASSWORD': Terminating Connection #%i\n", fd);
		fflush(stdout);
		return;
	}

}





/*
**Function to import users from Authentication.txt into authedPlayer struct types
*/
void importUsers(){
	FILE *fp;
	char *line = NULL;
	size_t len = 0; //allows only positive return values, i.e. 0 - 65535 unsigned
	ssize_t read; //allows a return value of negative numbers, i.e. -1 signed
	char whiteSpace;

	fp = fopen(AUTH_FILE, "r");
	

	if(fp==NULL){
		perror("File Open NULL\n");
		exit(1);
	}

	/**Debugging Section to ensure correct file details are being imported**/
	if(globalDebugMode==1){
		printf("File is open\n");//debugging line
		char linetwo[256];//debugging line
		printf("Printf of Auth.txt:\n");//debugging line
		while(fgets(linetwo, sizeof(linetwo), fp)){//debugging line
			globalNumUsers++;//debugging line
			printf("%s", linetwo);//debugging line
		}
		globalNumUsers = -1;
		rewind(fp);
	}//end debug
	
	/**count players listed in auth file**/
	while((read=getline(&line,&len,fp))!=-1){
		globalNumUsers++;
	}

	/**Allocate memory for list of authenticated players**/
	authedPlayers = malloc(globalNumUsers * sizeof(*authedPlayers));
	if(authedPlayers==NULL){
		printf("authedPlayers is NULL\n");
		perror("authedPlayers List is NULL");
		exit(1);
	}
	rewind(fp);	//set cursor to beginning of file

	for(int i = -1; (read=getline(&line,&len,fp))!=-1; ++i){
		if(globalDebugMode==1){printf("Import Users: First For Loop; iloop #: %d\n", i);}
		if(i==-1) continue;
		whiteSpace = 0;
		int j = 0;
		while(1){
			if(line[j]=='\n' OR line[j]=='\r' OR line[j]=='\0'){
				authedPlayers[i].password[j - whiteSpace] = '\0';
				break;
			}
			else if(line[j]!=' ' AND line[j]!='\t'){
				if((j>0 && line[j-1]==' ') OR line[j-1]=='\t'){
					whiteSpace = j;
				}
				if(whiteSpace){
					authedPlayers[i].password[j-whiteSpace] = line[j];
				} else {
					authedPlayers[i].name[j] = line[j];
				}
				if(line[j+1]==' ' OR line[j+1]=='\t'){
					authedPlayers[i].name[j+1] = '\0';
				}
			}
			j++;
		}
		if(globalDebugMode==1){
			printf("Username: ");
			for(int k=0;k<=j;k++){
				printf("%c", line[k]);
				if(line[k]=='\t'){
					k = k+1;
					printf("\n");
					printf("Password: ");
				}
			}
		}
		printf("\n");
	}
	fclose(fp);
}

/*
**Signal Interrupt
**Interrupts the process cycle via user input (i.e. CTRL+C)
*/
void sigint_handler(int signal){
	if(signal==SIGINT){
		globalSrvrProc = 0;
		fputs("Thank you for playing!\n", stdout);
	}
}

/*
**Rotating waiting cursor while server waits
*/
void waitingCursur(){
	char chars[] = {'-','\\','|','/'};
	for(int i=0;globalSrvrProc!=0;++i){
		printf("%c\r",chars[i % sizeof(chars)]);
		fflush(stdout);
		usleep(200000);
	}
}

/*
**Main Function
*/
int main(int argc, char* argv[]){

	int socketFd;
	int clientFd;
	int serverPort = DEFAULT_SERVER_PORT;
	struct sockaddr_in serverAddress;

	signal(SIGINT, sigint_handler);
	importUsers();

	if(argc>1){
		serverPort = atoi(argv[1]);
	}

	
	/**	Socket Structures as per 
		www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html**/
	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(serverPort);
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

	/**Socket Operations**/
	if(globalDebugMode==1){printf("Creating Socket\n");}
	socketFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if(socketFd==SOCKET_ERROR){
		perror("Socket Creation\n");
		exit(1);
	}

	if(bind(socketFd,(struct sockaddr*)&serverAddress,sizeof(struct sockaddr))==SOCKET_ERROR){
		perror("Socket Binding\n");
		close(socketFd);
		exit(1);
	}
	if(listen(socketFd,REQUEST_BACKLOG)==SOCKET_ERROR){
		perror("Socket Listening\n");
		close(socketFd);
		exit(1);
	}

	/**Debugging Section to ensure correct details are being parsed**/
	if(globalDebugMode==1){
		printf("Socket Being Used: %d\n", socketFd);
		printf("Port Being Used: %d\n", serverPort);
	}
	
	while(globalSrvrProc){
		//TO DO: Main server processing
		waitingCursur();
	}
	close(socketFd);
	return 0;
}