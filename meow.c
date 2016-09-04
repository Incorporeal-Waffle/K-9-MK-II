#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include "printfs.h"
#include "k-9.h"
int sockfd;

int doStuffWithMessage(struct message * msg){
	rPrintf("%s\n", msg->original);
	if(strcmp(msg->command, "PING")==0)
		sPrintf(sockfd, "PONG :%s\r\n", msg->trailing);
	if(strcmp(msg->command, "001")==0)
		for(int i=0; autoJoinChans[i]!=NULL; i++)
			sPrintf(sockfd, "JOIN %s\r\n", autoJoinChans[i]);
	
	return 1;
}
void reapChildren(int idk){
	idk=idk;//Getting rid of unused parameter warning.
	//iPrintf("Received SIGCHILD %d\n", idk);
	while(waitpid(-1, NULL, WNOHANG)>0);
	//iPrintf("Done reaping\n", idk);
}

int messageDump(struct message *pMsg){
	iPrintf("original: %s$\n", pMsg->original);
	
	iPrintf("name: %s$\n", pMsg->name);
	iPrintf("user: %s$\n", pMsg->user);
	iPrintf("host: %s$\n", pMsg->host);
	
	iPrintf("command: %s$\n", pMsg->command);
	
	iPrintf("params: %s$\n", pMsg->params);
	iPrintf("trailing: %s$\n", pMsg->trailing);
	return 1;
}

int freeMessage(struct message *msg){
	free(msg->original);

	free(msg->name);
	free(msg->user);
	free(msg->host);

	free(msg->command);

	free(msg->params);
	free(msg->trailing);

	free(msg);
	return 1;
}

struct message *parseMessage(char *msg){
	/*
	https://tools.ietf.org/html/rfc1459#section-2.3.1
	I would copy paste the relevant part here, but idk if that's allowed.
	*/
	
	struct message *pMsg=malloc(sizeof(struct message));
	char *tmp, *tmp0, *tmp1;
	
	if(pMsg==0){
		ePrintf("Malloc failed when parsing message.\n");
		return NULL;
	}
	
	pMsg->original=strdup(msg);
	
	if(*msg==':'){//prefix (name, user, host)
		tmp=msg;
		msg=strchr(msg, ' ');
		*msg='\0';
		msg++;//Are these pointer modifications portable to other arches than x86?
		tmp0=strchr(tmp, '!');
		tmp1=strchr(tmp, '@');
		if(tmp0){
			if(tmp1)
				pMsg->user=strndup(tmp0, tmp1-tmp0);
			else
				pMsg->user=strdup(tmp0);
			
			*tmp0='\0';
			pMsg->name=strdup(tmp);
			*tmp0='!';
		}else
			pMsg->user=NULL;
		
		if(tmp1){
			pMsg->host=strdup(tmp1);
			if(!tmp0){
				*tmp1='\0';
				pMsg->name=strdup(tmp);
				//*tmp1='@';
			}
		}else
			pMsg->host=NULL;
		
		if(!tmp0 && !tmp1)
			pMsg->name=strdup(tmp);
		
	}else{
		pMsg->name=NULL;
		pMsg->user=NULL;
		pMsg->host=NULL;
	}
	
	pMsg->command=strndup(msg, strchr(msg, ' ')-msg);// Is this fine? (pointer math)
	//Appears to work for me on x86-64, but idk.
	
	msg=strchr(msg, ' ');
	*msg='\0';
	msg++;
	
	tmp0=strchr(msg, ':');
	if(tmp0){
		pMsg->trailing=strdup(tmp0+1);
		*tmp0='\0';
	}else{
		tmp0=strrchr(msg, ' ');
		pMsg->trailing=strdup(tmp0+1);
		*tmp0='\0';
	}
	pMsg->params=strdup(msg);
	
	return pMsg;
}

int recvd(char *rMsg){
	char *tok=strtok(rMsg, "\n");
	char *cr;
	struct message *parsedMsg;
	int pid;
	
	static char buf[513];
	static int contflag;
	
	do{//Separate messages
		if((cr=strchr(tok, '\r'))){//The whole line was received
			*cr='\0';
			if(contflag){//Continue incase of missing \r\n in last message
				contflag=0;
				if(strlen(tok)+strlen(buf)>=512){//Fuck this
					iPrintf("Split message too long\n");
					continue;
				}else{
					strcpy(strchr(buf, '\0'), tok);
					tok=buf;
				}
			}
			
			pid=fork();
			switch(pid){
				case -1:
					ePrintf("Fork failed\n");
					break;
				case 0:
					//rPrintf("%s\n", tok);// THIS IS WHERE THE MESSAGE IS GOOD FOR USE
					parsedMsg=parseMessage(tok);
					doStuffWithMessage(parsedMsg);
					//messageDump(parsedMsg);
					freeMessage(parsedMsg);
					_Exit(0);
					break;
				default:
					break;
			}
		}else{//Message will continue later
			if(contflag){//Ugh... Should I even support this? Not gonna bother.
				iPrintf("Two messages in a row that are lacking a \\r\\n\n");
				contflag=0;
				continue;
			}
			if(strlen(tok)>512){
				rPrintf("Message too long\n");
				continue;
			}else{
				strcpy(buf, tok);
			}
			contflag=1;
			continue;
		}
	}while((tok=strtok(NULL, "\n"))!=0);
	
	free(rMsg);
	return 1;
}

int main(int argc, char **argv){
	sockfd=*argv[0];
	int rbytes;
	char buf[513];
	struct pollfd pollfds[2];
	
	iPrintf("Meow version %s\n", VERSION);
	if(argc<1){
		ePrintf("Meow: Invalid number of arguments.\n");
		return 1;
	}
	
	pollfds[0].fd=STDIN_FILENO;
	pollfds[0].events=POLLIN|POLLPRI;
	pollfds[1].fd=sockfd;
	pollfds[1].events=POLLIN|POLLPRI;
	while(poll(pollfds, 2, -1)>=0 || errno==EINTR){
		signal(SIGCHLD, reapChildren);
		if((pollfds[1].revents & POLLERR) || (pollfds[1].revents & POLLHUP)){//sock
			ePrintf("An error has occured on the socket.\n");
			return 0;
		}
		if((pollfds[1].revents&POLLIN)||(pollfds[1].revents&POLLPRI)){
			rbytes=read(sockfd, buf, 512);
			
			if(rbytes==0){
				iPrintf("Received 0 bytes. Idk what this means,");
				iPrintf(" but it appears to happen when the connection's closed\n");
				return 0;
			}
			if(rbytes<0){
				ePrintf("Read error: %s\n", strerror(errno));
				return 2;
				break;
			}
			if(rbytes>512){
				iPrintf("rbytes>512\n");
			}
			
			buf[rbytes]=0;
			recvd(strdup(buf));
		}
		if(pollfds[0].revents){//stdin
			rbytes=read(STDIN_FILENO, buf, 512);
			buf[rbytes]=0;
			sPrintf(sockfd, "%s", buf);
		}
	}
	
	iPrintf("The main loop has finished somehow.\n");
	return 0;//What is this doing here again?
}
