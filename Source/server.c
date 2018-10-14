/**
*CAB403 Semester 02; 2018
*Assignment: Process Management and Distributed Computing
*Assignment 1
*
*Server Side
*Author: Luke Goeree 20/09/2018
**/

//Included Libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

//Static Definitions
#define SOCKET_ERROR -1
#define USER_DETAILS_BLOCK 20
#define DEFAULT_SERVER_PORT 12345
#define REQUEST_BACKLOG 10
#define AUTH_FILE "authentication.txt"


//Custom Structs

/*
** Custome Struct to describe all authenticated players
*/
struct authedPlayer{
	char name[USER_DETAILS_BLOCK];
	char password[USER_DETAILS_BLOCK];
};
typedef struct authedPlayer authedPlayer_t;


//Global Variables
int debug_mode = 1;
authedPlayer_t* authPlayers;
int numUsers = -1;
volatile int serverProcessing = 1;







void importUsers(){
	FILE *fp;
	char *line = NULL;
	size_t len = 0; //allows only positive return values, i.e. 0 - 65535 unsigned
	ssize_t read; //allows a return value of negative numbers, i.e. -1 signed
	char after_white;

	fp = fopen(AUTH_FILE, "r");
	

	if(fp==NULL){
		perror("File Open NULL\n");
		exit(1);
	}

	//Debugging Section to ensure correct file details are being imported
	if(debug_mode==1){
		printf("File is open\n");//debugging line
		char linetwo[256];//debugging line
		printf("Printf of Auth.txt:\n");//debugging line
		while(fgets(linetwo, sizeof(linetwo), fp)){//debugging line
			numUsers++;//debugging line
			printf("%s", linetwo);//debugging line
		}
		printf("\n");
		printf("printf numUsers: \t%d\n",numUsers);//debugging line
		numUsers = -1;
		printf("reset numUsers: \t%d\n",numUsers);//debugging line
	}
	
	while((read=getline(&line,&len,fp))!=-1){
		numUsers++;
		printf("getline numUsers: \t%d\n",numUsers);//debugging line
	}

	authPlayers = malloc(numUsers * sizeof(*authPlayers));
	rewind(fp);
	if(debug_mode==1){printf("Rewind Completed\n");}

	for(int i = -1; (read=getline(&line,&len,fp))!=-1; ++i){
		if(debug_mode==1){printf("Import Users: First For Loop; iloop #: %d\n", i);}
		if(i==-1) continue;
		after_white = 0;
		int j = 0;
		while(1){
			if(debug_mode==1){printf("Import Users: Entering While Loop\n", i);}
			if(line[j]=='\n'||line[j]=='\r'||line[j]=='\0'){
				authPlayers[i].password[j - after_white] = '\0';
				break;
			}
			else if(line[j]!=' ' && line[j]!='\t'){
				if((j>0 && line[j-1]==' ')||line[j-1]=='\t'){
					after_white = j;
				}
				if(after_white){
					authPlayers[i].password[j-after_white] = line[j];
				} else {
					authPlayers[i].name[j] = line[j];
				}
				if(line[j+1]==' '||line[j+1]=='\t'){
					authPlayers[i].name[j+1] = '\0';
				}
			}
			j++;
		}
	}

	fclose(fp);
}



/*Signal Interrupt
Interrupts the process cycle via user input (i.e. CTRL+C)*/
void sigint_handler(int signal){
	if(signal==SIGINT){
		serverProcessing = 0;
		fputs("Thank you for playing!\n", stdout);
	}
}




//Main Function
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
	if(debug_mode==1){printf("Server Side Running!\n");}

	/*	
	**	Socket Structures as per 
	**	www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
	*/
	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(serverPort);
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

	//Socket Operations
	if(debug_mode==1){printf("Creating Socket\n");}
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

	//Debugging Section to ensure correct details are being parsed
	if(debug_mode==1){
		printf("Socket Being Used: %d\n", socketFd);
		printf("Port Being Used: %d\n", serverPort);
	}

	
	
	while(serverProcessing){
		//TO DO: Main server processing
	}
	close(socketFd);
	return 0;
}