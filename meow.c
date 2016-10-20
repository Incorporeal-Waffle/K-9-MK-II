#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include "k-9.h"
#include "config.h"
#include "etc.h"

#ifdef USE_CURL
#include <curl/curl.h>
#endif

#define IPUTCMDPREFIX '/'

int sockfd;//Gonna need to use it from several places.
char *shmpath;

size_t writedatafromcurl(void *buffer, size_t size, size_t nmemb, void *userp){
	size_t total=size*nmemb;
	struct mthing *thing=(struct mthing *)userp;
	
	thing->mem=realloc(thing->mem, thing->size+total);
	if(!thing->mem){
		ePrintf("Failed to allocate memory for curl\n");
		return 0;
	}
	
	memcpy(thing->mem+thing->size-1, buffer, total);
	thing->size += total;
	thing->mem[thing->size-1]='\0';
	
	return total;
}

int doStuffWithMessage(struct message * msg){// The main place for adding new stuff
	char *targetChan, *cptmp, *cptmp0, *postCmd;
	struct mmEntry *entry;
	//cptmp is a temporary variable. Assume it's value unknown at the beginning
	//of the code of each command
	//rPrintf("%s\n", msg->original);
	if(!strcmp(msg->command, "PING"))
		sPrintf(sockfd, "PONG :%s\r\n", msg->trailing);
	if(!strcmp(msg->command, "001")){
		for(int i=0; autoJoinChans[i]!=NULL; i++)
			sPrintf(sockfd, "JOIN %s\r\n", autoJoinChans[i]);
		for(int i=0; autoRunCmds[i]!=NULL; i++)
			sPrintf(sockfd, "%s\r\n", autoRunCmds[i]);
	}
	
	if(!strcmp("433", msg->command))//TODO: alt nicknames
		iPrintf("Nick already used.\n");
	if(!strcmp("432", msg->command))
		iPrintf("Erroneus nickname.\n");
	if(!strcmp("NICK", msg->command)){
		entry=mmFind(shmpath, SHMKEY, "nick");
		cptmp=strdup(entry->value);
		mmFreeEntry(entry);
		//iPrintf("Current nick ^%s$ adg ^%s$\n", cptmp, msg->name);
		if(!strcmp(msg->name, cptmp)){
			iPrintf("Nickname changed from ^%s$ to ^%s$.\n", cptmp, msg->trailing);
			mmDelEntry(shmpath, SHMKEY, "nick", cptmp);
			mmAddEntry(shmpath, SHMKEY, "nick", msg->trailing);
		}
		free(cptmp);
	}
	
	if(!strcmp("PRIVMSG", msg->command)){
		cptmp=strchr(msg->params, ' ');
		if(cptmp){
			*cptmp='\0';
			entry=mmFind(shmpath, SHMKEY, "nick");
			cptmp0=strdup(entry->value);
			mmFreeEntry(entry);
			if(!strncmp(cptmp0, msg->params, strlen(cptmp0))){//Reply to the sender,
				//not yourself
				targetChan=msg->name;
			}else
				targetChan=strdup(msg->params);
			free(cptmp0);
			*cptmp=' ';
		}
		
		//CTCP. You probably don't want to add any commands before or after this.
		if(*msg->trailing=='\x01'){
			iPrintf("That was a CTCP. Be informed.\n");
			msg->trailing++;
			postCmd=strchr(msg->trailing, ' ')+1;
			if(!strncmp("VERSION", msg->trailing, 7))//VERSION
				sPrintf(sockfd, "NOTICE %s :\x01VERSION K-9-MK-II %s\x01\r\n",
					targetChan, VERSION);
			else if(!strncmp("SOURCE", msg->trailing, 6))//SOURCE
				sPrintf(sockfd, "NOTICE %s :\x01SOURCE %s\x01\r\n",
					targetChan, SOURCE);
			else if(!strncmp("PING", msg->trailing, 4)){//PING
				cptmp=strchr(msg->trailing, '\x01');
				if(cptmp){
					*cptmp='\0';
					sPrintf(sockfd, "NOTICE %s :\x01PING %s\x01\r\n",
						targetChan, postCmd);
					*cptmp='\x01';
				}else
					sPrintf(sockfd, "NOTICE %s :\x01PING \x01\r\n",
						targetChan);
			}
			
			msg->trailing--;
			return 1;
		}
		
		if(!strcmp("h", msg->trailing))
			sPrintf(sockfd, "PRIVMSG %s :h\r\n", targetChan);
		
		//Prefixed commands go here
		if(*msg->trailing==PREFIXC){
			msg->trailing++;//Ignore the prefix
			postCmd=strchr(msg->trailing, ' ')+1;
			
			if(!strncmp("echo", msg->trailing, 4)){//Echo
				sPrintf(sockfd, "PRIVMSG %s :%s\r\n", targetChan, postCmd);
			}
			#ifdef USE_CURL
			#define TINYURLREQUESTSTRPREFIX "tinyurl.com/api-create.php?url="
			else if(!strncmp("tiny", msg->trailing, 4)){
				CURL *hnd;
				struct mthing retstr;
				retstr.mem = malloc(1);//INitialize
				retstr.size = 1;
				
				hnd=curl_easy_init();
				//request string
				cptmp0=curl_easy_escape(hnd, postCmd, strlen(postCmd));
				cptmp=malloc(strlen(TINYURLREQUESTSTRPREFIX)+strlen(cptmp0)+1);
				strcpy(cptmp, TINYURLREQUESTSTRPREFIX);
				strcat(cptmp, cptmp0);
				
				curl_easy_setopt(hnd, CURLOPT_URL, cptmp);
				//"tinyurl.com/api-create.php?url=http%3a%2f%2fexample.org%2f");
				curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, writedatafromcurl);
				curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &retstr);
				curl_easy_perform(hnd);
				//Request done, use the answer
				sPrintf(sockfd, "PRIVMSG %s :%s\r\n", targetChan, retstr.mem);
				//Clean up
				free(cptmp);
				free(retstr.mem);
				retstr.size=0;
				
				curl_free(cptmp0);
				curl_easy_cleanup(hnd);
				hnd=NULL;
				/*
				//Ugly tinyurl thing
				//Warning: This function contains some really ugly http stuff
				int tinyFd;
				char buf[513];
				tinyFd=sConnect("tinyurl.com", "80");
				if(tinyFd<0){
					sPrintf(sockfd, "PRIVMSG %s :Failed to connect\r\n", targetChan);
				}else{
					sPrintf(tinyFd, 
					"GET /api-create.php?url=%s HTTP/1.1\r\nHost: tinyurl.com\r\n\r\n",
					strrchr(msg->trailing, ' ')+1);
					read(tinyFd, buf, 512);
					cptmp=strstr(buf, "\r\n\r\n");
					cptmp+=4;
					cptmp=strchr(cptmp, '\n')+1;
					*strchr(cptmp, '\r')='\0';
					sPrintf(sockfd, "PRIVMSG %s :%s\r\n", targetChan, cptmp);
				}
				close(tinyFd);
				*/
				
			}
			#endif
			
			msg->trailing--;//Restore the prefix
		}
	}
	return 1;
}

void reapChildren(int idk){//Reap em zombies
	(void)idk;
	//iPrintf("Received SIGCHILD %d\n", idk);
	while(waitpid(-1, NULL, WNOHANG)>0);
	//iPrintf("Done reaping\n", idk);
}

int freeMessage(struct message *msg){//Frees a message struct
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

struct message *parseMessage(char *msg){//Parses a message (line)
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
		msg++;
		tmp++;
		tmp0=strchr(tmp, '!');
		tmp1=strchr(tmp, '@');
		if(tmp0){//There's a '!', get the user
			if(tmp1)
				pMsg->user=strndup(tmp0+1, tmp1-tmp0-1);
			else
				pMsg->user=strdup(tmp0+1);
			
			*tmp0='\0';
			pMsg->name=strdup(tmp);//Name
			*tmp0='!';
		}else
			pMsg->user=NULL;
		
		if(tmp1){//There was a '@', get the host
			pMsg->host=strdup(tmp1+1);
			if(!tmp0){
				*tmp1='\0';
				pMsg->name=strdup(tmp);//Name
				//*tmp1='@';
			}
		}else
			pMsg->host=NULL;
		
		if(!tmp0 && !tmp1)
			pMsg->name=strdup(tmp);//Name
		
	}else{
		pMsg->name=NULL;
		pMsg->user=NULL;
		pMsg->host=NULL;
	}
	
	pMsg->command=strndup(msg, strchr(msg, ' ')-msg);// Is this fine? (pointer math)
	
	msg=strchr(msg, ' ');//Let's move the msg beginning past the command
	*msg='\0';
	msg++;
	
	tmp0=strchr(msg, ':');
	if(tmp0){//There is a ':', that means everything after it goes in trailing
		pMsg->trailing=strdup(tmp0+1);
		*tmp0='\0';
	}else{//There is no ':', meaning the last element in the params is actually trailing
		tmp0=strrchr(msg, ' ');
		pMsg->trailing=strdup(tmp0+1);
		*tmp0='\0';
	}
	pMsg->params=strdup(msg);
	
	return pMsg;
}

int recvd(char *rMsg){//Initial processing of freshly received data
	char *tok=strtok(rMsg, "\n");
	char *cr;
	struct message *parsedMsg;
	int pid;
	
	static char buf[513];//Need to preserve these across function calls
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
			
			rPrintf("%s\n", tok);// THIS IS WHERE THE MESSAGE IS GOOD FOR USE
			pid=fork();//Fork so we don't block when replying to the message
			//Messages can be processed out of order after this
			switch(pid){
				case -1:
					ePrintf("Fork failed\n");
					break;
				case 0:
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
				iPrintf("Message too long\n");
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

int gotInput(char *buf){
	char *tmp;
	if(*buf==IPUTCMDPREFIX){
		buf++;
		*strchr(buf, '\n')='\0';
		if(!strcmp(buf, "reload")){
			iPrintf("Reload command received, exiting\n");
			_Exit(1);
		}if(!strcmp(buf, "dumpstate")){//TODO: make this thing more helpful
			iPrintf("Dumping bot state.\n");
			tmp=mmGetPtr(shmpath, SHMKEY);
			write(1, tmp, *(int*)tmp);
		}else{
			iPrintf("Unknown command\n");
		}
		buf--;
	}else
		sPrintf(sockfd, "%s", buf);
	
	free(buf);
	return 1;
}

int main(int argc, char **argv){
	int rbytes;
	char buf[513];
	struct pollfd pollfds[2];
	shmpath=argv[0];
	struct mmEntry *entry;
	entry=mmFind(shmpath, SHMKEY, "sockfd");
	sockfd=strtol(entry->value, NULL, 10);
	mmFreeEntry(entry);
	
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
		if((pollfds[1].revents&POLLIN)||(pollfds[1].revents&POLLPRI)){//sock
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
			gotInput(strdup(buf));
			//sPrintf(sockfd, "%s", buf);
		}
	}
	
	iPrintf("The main loop has finished somehow.\n");
	return 0;//What is this doing here again?
}
