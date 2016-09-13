#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "k-9.h"
#include "config.h"
#include "etc.h"

int main(int argc, char **argv){
	int botPid, retVal;
	char opt;//The option for getopt will be stored here
	struct addrinfo *host, hints;
	
	//Setting the defaults
	sHost = HOST;
	sPort = PORT;
	nick = NICK;
	rName = REALNAME;
	userName = REALNAME;
	
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
				printf("usage: k-9 [-Vh] [-d host] [-p port] [-n nick] [-r realname]\n");
				printf("           [-u username]\n");
				return 0;
			case '?':
				return 1;
			default:
				ePrintf("Dafuq did you pass as an argument?");
				return 1;
		}
	}
	
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=0;
	hints.ai_flags=0;
	
	iPrintf("Connecting to %s on port %s...\n", sHost, sPort);
	
	getaddrinfoeaiagainretrything:
	switch(getaddrinfo(sHost, sPort, &hints, &host)){//Resolve
		case 0:
			break;
		case EAI_AGAIN:
			sleep(5);
			// If you've got a significantly better way to handle this, lemme know.
			goto getaddrinfoeaiagainretrything;
			break;
		case EAI_SYSTEM:
			ePrintf("getaddrinfo: %s\n", strerror(errno));
			return 1;
		default:
			ePrintf("Some sort of a getaddrinfo issue occurred. Check your args.\n");
			ePrintf("Other than that? *shrug*\n");
			return 1;
	}
	
	for(struct addrinfo *addr=host; addr!=NULL; addr=addr->ai_next){//Connect
		sockfd=socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(connect(sockfd, addr->ai_addr, addr->ai_addrlen)){//Failed to connect
			close(sockfd);
			sockfd=-1;
			continue;
		}
		freeaddrinfo(host);
		break;
	}
	
	if(sockfd==-1){
		ePrintf("Connection failed.\n");
		return 1;
	}
	
	iPrintf("Connection established.\n");//Yay
	
	sPrintf(sockfd, "USER %s 0 * :%s\r\nNICK %s\r\n", userName, rName, nick);// REGISTER
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
			iPrintf("%d\n", execl("meow", (char *)&sockfd, NULL));
			perror("h");
			break;
		}
	}
	return 0;
}
