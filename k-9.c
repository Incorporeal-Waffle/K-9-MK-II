#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "k-9.h"
#include "config.h"
#include "etc.h"

int main(int argc, char **argv){
	int botPid, retVal;
	char inttostr[4]="255";
	char *shmpath;
	char opt;//The option for getopt will be stored here
	
	//Setting up the bot state shared memory thing
	shmpath=mkdtemp(strdup("/tmp/k-9XXXXXX"));
	if(!shmpath){
		ePrintf("Failed to create the temporary dir\n");
		return 1;
	}
	
	//Setting the defaults
	sHost = HOST;
	sPort = PORT;
	nick = NICK;
	rName = REALNAME;
	userName = USERNAME;
	
	while((opt=getopt(argc, argv, "Vhd:p:n:r:u:")) != -1){//Parse options
		switch(opt){
			case 'd':// Host to connect to 
				sHost=optarg;
				break;
			case 'p':// Port to connect to
				sPort=optarg;
				break;
			case 'n':// Nick
				nick=optarg;
				break;
			case 'r':// Realname
				rName=optarg;
				break;
			case 'u':// Username
				userName=optarg;
				break;
			case 'V':
				printf("%s\n", VERSION);
				return 0;
			case 'h':
			case '?':
				printf("usage: k-9 [-Vh] [-d host] [-p port] [-n nick] [-r realname]\n");
				printf("           [-u username]\n");
				return 1;
			default:
				ePrintf("Dafuq did you pass as an argument?");
				return 1;
		}
	}
	
	iPrintf("Connecting to %s on port %s...\n", sHost, sPort);
	
	sockfd=sConnect(sHost, sPort);
	
	if(sockfd==-1){
		ePrintf("Connection failed.\n");
		return 1;
	}
	
	iPrintf("Connection established.\n");//Yay
	
	sPrintf(sockfd, "NICK %s\r\nUSER %s 0 * :%s\r\n", nick, rName, userName);// REGISTER
	//Store the bot state
	mmAddEntry(shmpath, SHMKEY, "sHost", sHost);
	mmAddEntry(shmpath, SHMKEY, "sPort", sPort);
	mmAddEntry(shmpath, SHMKEY, "nick", nick);
	mmAddEntry(shmpath, SHMKEY, "rName", rName);
	mmAddEntry(shmpath, SHMKEY, "userName", userName);
	snprintf(inttostr, 4, "%d", sockfd);
	mmAddEntry(shmpath, SHMKEY, "sockfd", inttostr);
	
	while(1){
		botPid=fork();
		if(botPid==-1){// Error
			ePrintf("Failed to fork: %s\n", strerror(errno));
			sleep(5);
		}else if(botPid){// Parent
			waitpid(botPid, &retVal, 0);
			if(retVal){// Something went wrong. Restart it.
				iPrintf("Child exited with status %u. Restarting it in 1 sec.\n", retVal);
				sleep(1);
				continue;
			}else{// Return value 0, successful exit
				iPrintf("Exiting.\n");
				break;
			}
		}else{// Child -- IT'S ALIVE!
			iPrintf("%d\n", execl("meow", shmpath, NULL));
			perror("h");
			break;
		}
	}
	return 0;
}
