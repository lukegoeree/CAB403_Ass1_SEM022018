/**
*CAB403 Semester 02; 2018
*Assignment: Process Management and Distributed Computing
*Assignment 1
*
*Client Side
*Author: Luke Goeree 20/09/2018
**/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>


#define USER_DETAILS_BLOCK 20

volatile int clientFd;

void welcomeScreen(char* playerUsrnm){
	int status;
	char playerPwd[USER_DETAILS_BLOCK];

	fputs("=======================================================\n", stdout);
	fputs("Welcome to the online Minesweeper gaming system\n", stdout);
	fputs("=======================================================\n\n\n", stdout);
	fputs("You are required to logon with your registered user name and password.\n\n", stdout);
	fputs("Username: ", stdout);
	scanf("%s", playerUsrnm);
	fputs("Password: :", stdout);
	scanf("%s", playerPwd);

}

void exitClient(int status){
	fflush(stdout);
	shutdown(clientFd, SHUT_RDWR);
	close(clientFd);
	exit(status);
}

/*
**Signal Interrupt
**Interrupts the process cycle via user input (i.e. CTRL+C)
*/
void sigintHandler(int signal){
	if(signal==SIGINT){
		fputs("Thank you for playing!\n", stdout);
		exitClient(0);
	}
}






//Main Function
int main(int argc, char* argv[]){

	if(argc<3){
		fputs("The program requires a hostname and port number to connect to server\n", stdout);
		exit(1);
	} else{
		fputs("wierd error", stdout);
	}
	signal(SIGINT, sigintHandler);



	return 0;
}