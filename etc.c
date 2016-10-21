#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include "etc.h"
#include "config.h"

int freeMessage(struct message *msg){//Frees a message struct
	if(msg){
		free(msg->original);
		
		free(msg->name);
		free(msg->user);
		free(msg->host);
		
		free(msg->command);
		
		free(msg->params);
		free(msg->trailing);
		
		free(msg);
	}else{
		return 0;
	}
	return 1;
}

struct message *parseMessage(char *msg){//Parses a message (line)
	struct message *pMsg=malloc(sizeof(struct message));
	char *tmp, *tmp0, *tmp1, *omsg;
	if(pMsg==0){
		ePrintf("Malloc failed when parsing message.\n");
		return NULL;
	}
	
	msg=omsg=strdup(msg);
	pMsg->original=strdup(msg);
	
	if(*msg==':'){//prefix (name, user, host)
		tmp=msg;
		
		msg=strchr(msg, ' ');//Skip the prefix for the rest of the function
		*msg='\0';
		msg++;
		
		tmp++;
		tmp0=strchr(tmp, '!');
		tmp1=strchr(tmp, '@');
		if(tmp1){//There was a '@', get the host
			*tmp1='\0';
			tmp1++;
			pMsg->host=strdup(tmp1);
		}else
			pMsg->host=NULL;
		
		if(tmp0){//There's a '!', get the user
			*tmp0='\0';
			tmp0++;
			pMsg->user=strdup(tmp0);
		}else
			pMsg->user=NULL;
		
		pMsg->name=strdup(tmp);//Name
	}else{
		pMsg->name=NULL;
		pMsg->user=NULL;
		pMsg->host=NULL;
	}
	
	pMsg->command=strndup(msg, strchr(msg, ' ')-msg);
	msg=strchr(msg, ' ');//Let's move the msg beginning past the command
	if(msg)
		*msg='\0';
	else{
		ePrintf("Invalid message\n");
		free(omsg);
		return 0;
	}
	msg++;
	
	tmp0=strchr(msg, ':');
	if(tmp0){//There is a ':', that means everything after it goes in trailing
		*tmp0='\0';
		tmp0++;
		pMsg->trailing=strdup(tmp0);
	}else{//There is no ':', meaning the last element in the params is actually trailing
		tmp0=strrchr(msg, ' ');
		if(tmp0){
			*tmp0='\0';
			tmp0++;
			pMsg->trailing=strdup(tmp0);
		}else{
			pMsg->trailing=strdup(msg);
			*msg='\0';
		}
	}
	pMsg->params=strdup(msg);
	free(omsg);
	return pMsg;
}


int messageDump(struct message *pMsg){//Dumps the contents of a message struct
	if(!pMsg){
		ePrintf("messagedump got a null pointer\n");
		return 0;
	}
	iPrintf("original: ^%s$\n", pMsg->original);
	
	iPrintf("name: ^%s$\n", pMsg->name);
	iPrintf("user: ^%s$\n", pMsg->user);
	iPrintf("host: ^%s$\n", pMsg->host);
	
	iPrintf("command: ^%s$\n", pMsg->command);
	
	iPrintf("params: ^%s$\n", pMsg->params);
	iPrintf("trailing: ^%s$\n", pMsg->trailing);
	return 1;
}

int sConnect(char *hostname, char *port){
	int fd=-1;
	struct addrinfo *host, hints;
	
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=0;
	hints.ai_flags=0;
	
	getaddrinfoeaiagainretrything:
	switch(getaddrinfo(hostname, port, &hints, &host)){//Resolve
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
		fd=socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(connect(fd, addr->ai_addr, addr->ai_addrlen)){//Failed to connect
			close(fd);
			fd=-1;
			continue;
		}
		freeaddrinfo(host);
		break;
	}
	
	return fd;
}

//Printfs
int sPrintf(int sockfd, char *format, ...){// Send and print after parsing and fixing
	va_list ap;
	char *str, *ptr, *optr, *trptr;
	char msgconstruct[512];
	int prefixsize=0;
	int cmdargsize=0;
	int trailingsize=0;
	int skip=0;
	struct message *msg;
	struct mmEntry *entry;
	
	va_start(ap, format);//Get the string
	vasprintf(&str, format, ap);
	// Unfortunately I have to use this nonportable f here for sanity.
	// If you have ideas for getting rid of this, tell me.
	va_end(ap);
	
	ptr=optr=str;
	while((ptr=strchr(ptr, '\r'))){// We can have multiple messages here
		if(ptr)
			*ptr='\0';
		msg=parseMessage(str);
		if(!msg){
			ePrintf("Message not sent\n");
			return 0;
		}
		
		//Calculate the prefix size. We're wasting space on non-prefixed commands
		//, but whatever
		prefixsize++;// :
		entry=mmFind(shmpath, SHMKEY, "nick");
		if(entry){
			prefixsize+=strlen(entry->value);// Nick
			mmFreeEntry(entry);
		}else{
			prefixsize+=strlen(NICK);
		}
		
		prefixsize++;// !
		entry=mmFind(shmpath, SHMKEY, "userName");
		if(entry){
			prefixsize+=strlen(entry->value);//user
			mmFreeEntry(entry);
		}else{
			prefixsize+=strlen(USERNAME);
		}
		
		prefixsize++;// @
		entry=mmFind(shmpath, SHMKEY, "botHost");
		if(entry){
			prefixsize+=strlen(entry->value);//host
			mmFreeEntry(entry);
		}else{
			prefixsize+=20;
		}
		
		prefixsize++;// space after it
		cmdargsize=strlen(msg->command)+strlen(msg->params)+3;// +2 for em spaces and :
		trailingsize=strlen(msg->trailing);
		
		trptr=msg->trailing;
		while(trailingsize>0){
			*msgconstruct='\0';
			if(prefixsize+cmdargsize+2<=512){
				strcat(msgconstruct, msg->command);
				strcat(msgconstruct, " ");
				strcat(msgconstruct, msg->params);
				strcat(msgconstruct, " :");
			}else{
				ePrintf("Prefix+command+arg size >= 512. Wtf are you even...\n");
				free(optr);
				return 1;
			}
			
			skip=512-2-cmdargsize-prefixsize;
			if(*trptr!='\n' && memchr(trptr, '\n', skip)){
				str=memchr(trptr, '\n', skip);
				*str=' ';
				skip=str - trptr;
			}
			strncat(msgconstruct, trptr, skip);
			strcat(msgconstruct, "\r\n");
			trptr+=skip;
			trailingsize-=skip;
			
			printf("\x1b[1;34m");//Colors!
			printf("%s", msgconstruct);
			printf("\x1b[0m");
			dprintf(sockfd, "%s", msgconstruct);
		}
		
		if(*(ptr+1))
			if(strlen(ptr+1)>2)
				ptr+=2;
		str=ptr;
	}
	if(ptr && *ptr)
		iPrintf("Warning: text after the last \\r%s$\n", ptr);
	free(optr);
	return 1;
}

int rawsPrintf(int sockfd, char *format, ...){// Send and print
	va_list ap;
	int retval=0;
	printf("\x1b[1;34m");
	va_start(ap, format);
	vprintf(format, ap);
	va_start(ap, format);
	retval=vdprintf(sockfd, format, ap);
	va_end(ap);
	printf("\x1b[0m");
	return retval;
}

int rPrintf(char *format, ...){// Print received data
	va_list ap;
	printf("\x1b[1;32m");
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	printf("\x1b[0m");
	return 1;
}

int ePrintf(char *format, ...){// Print errors
	va_list ap;
	dprintf(STDERR_FILENO, "\x1b[1;31m");
	va_start(ap, format);
	vdprintf(STDERR_FILENO, format, ap);
	va_end(ap);
	dprintf(STDERR_FILENO, "\x1b[0m");
	return 1;
}

int iPrintf(char *format, ...){// Print info
	va_list ap;
	dprintf(STDERR_FILENO, "\x1b[1;33m");
	va_start(ap, format);
	vdprintf(STDERR_FILENO, format, ap);
	va_end(ap);
	dprintf(STDERR_FILENO, "\x1b[0m");
	return 1;
}

//mm
//int shmsize, [int size, char type[]\0, char value[]\0]*
int mmGetId(char *path, char key){
	int shmid, *shmptr;
	key_t shmkey;
	
	shmkey=ftok(path, key);
	if(shmkey==-1){
		ePrintf("ftok: %s\n", strerror(errno));
		return -1;
	}
	
	shmid=shmget(shmkey, DEFAULTSHMSIZE, 0640|IPC_CREAT);//Initial thing
	if(shmid<0){
		ePrintf("ftok: %s\n", strerror(errno));
		return -1;
	}
	
	shmptr=shmat(shmid, 0, 0);
	if(shmptr==(int*)-1){
		ePrintf("ftok: %s\n", strerror(errno));
		return -1;
	}
	
	if(*shmptr==0)
		*shmptr=DEFAULTSHMSIZE;
	else if(*shmptr!=DEFAULTSHMSIZE){
		shmid=shmget(shmkey, *shmptr, 0640|IPC_CREAT);
		if(shmid<0){
			ePrintf("ftok: %s\n", strerror(errno));
			return -1;
		}
	}
	
	return shmid;
}

char *mmGetPtr(char *path, char key){
	char *shmptr;
	int shmid=mmGetId(path, key);
	
	shmptr=shmat(shmid, 0, 0);
	if(shmptr==(char *)-1){
		ePrintf("ftok: %s\n", strerror(errno));
		return (char*)-1;//This is totally a char ptr.
	}
	
	return shmptr;
}

int mmGrowShm(char *path, char key, int newSize){
	int shmid=mmGetId(path, key);
	int *shmptr=(int*)mmGetPtr(path, key);
	int oldSize=*shmptr;
	key_t shmkey;
	char *shmcopy;
	
	shmcopy=malloc(oldSize);
	memcpy(shmcopy, shmptr, oldSize);//Save it
	shmctl(shmid, IPC_RMID, NULL);//Del the shm
	
	shmkey=ftok(path, key);//Recreate the shm
	if(shmkey==-1){
		ePrintf("ftok: %s\n", strerror(errno));
		return -1;
	}
	
	shmid=shmget(shmkey, oldSize+newSize, 0640|IPC_CREAT);
	if(shmid<0){
		ePrintf("ftok: %s\n", strerror(errno));
		return -1;
	}
	
	shmptr=(int*)mmGetPtr(path, key);
	if(shmptr==(int*)-1){
		ePrintf("ftok: %s\n", strerror(errno));
		return -1;//This is totally a char ptr. Don't worry.
	}
	memcpy(shmptr, shmcopy, oldSize);
	*shmptr=oldSize+newSize;
	
	free(shmcopy);
	return 1;
}

int mmAddEntry(char *path, char key, char *type, char *value){
	//TODO: error checking
	char *shmptr=mmGetPtr(path, key);
	int addSize=strlen(type)+strlen(value)+sizeof(int)+2;//2 for the nulls
	int size=*(int*)shmptr-sizeof(int);//Let's get rid of the header
	shmptr+=sizeof(int);
	while(size>=addSize){//While there's still space to add it
		if(*(int*)shmptr==0){//Found an unused spot to add it
			break;
		}
		else{//Something's here/been here
			if(!strcmp(shmptr+sizeof(int), "u")){//Something's been here, but not anymore
				if(*(int*)shmptr>=addSize)//Got room. could leave some empty space tho...
					break;
				else{
					//Unused but no room.
					size-=*(int*)shmptr;//Go to the next item
					shmptr+=*(int*)shmptr;
				}
			}else{//Used
				size-=*(int*)shmptr;//Go to the next item
				shmptr+=*(int*)shmptr;
			}
		}
	}
	if(size>=addSize){//Everything *SHOULD* be fine, just add it
		if(*(int*)shmptr==0){//Unused spot
			*(int*)shmptr=addSize;
		}
		shmptr+=sizeof(int);
		strcpy(shmptr, type);
		shmptr=strchr(shmptr, '\0')+1;
		strcpy(shmptr, value);
	}else{//Need to fucking add space...
		mmGrowShm(path, key, addSize);
		mmAddEntry(path, key, type, value);
	}
	return 1;
}

struct mmEntry *mmMkEntry(char *type, char *value){
	struct mmEntry *entry;
	entry=calloc(sizeof(struct mmEntry), 1);
	if(!entry)
		return (void*)-1;
	entry->type=strdup(type);
	entry->value=strdup(value);
	entry->next=NULL;
	return entry;
}

int mmFreeEntry(struct mmEntry *entry){
	struct mmEntry *n;
	while(entry){
		n=entry->next;
		free(entry->type);
		free(entry->value);
		free(entry);
		entry=n;
	}
	return 1;
}

int mmDumpEntry(struct mmEntry *entry){
	iPrintf("Type: ^%s$\n", entry->type);
	iPrintf("Value: ^%s$\n", entry->value);
	if(!entry->next)
		iPrintf("Next pointer null.\n");
	return 1;
}

struct mmEntry *mmFind(char *path, char key, char *type){
	struct mmEntry *entry=NULL;//Apparently this isn't 0 by default
	struct mmEntry *lentry=NULL;
	char *shmptr=mmGetPtr(path, key);
	char *ePtr, *ptr;
	int size=*(int*)shmptr;
	shmptr+=sizeof(int);
	ePtr=ptr=shmptr;
	
	while(ePtr<(shmptr+size)){
		if(!(*(int*)ePtr))//Empty area reached
			break;
		ptr=ePtr+sizeof(int);//Skip the size for now
		
		if(type)
			if(!strcmp(ptr, type)){
				if(lentry){
					lentry->next=mmMkEntry(ptr, strchr(ptr, '\0')+1);
					lentry=lentry->next;
				}else{
					entry=mmMkEntry(ptr, strchr(ptr, '\0')+1);
					lentry=entry;
				}
			}
		
		ePtr=ePtr+(*(int*)ePtr);//NEXT
	}
	return entry;
}

int mmDelEntry(char *path, char key, char *type, char *value){
	//TODO: if value NULL then delete all of type
	char *shmptr=mmGetPtr(path, key);
	char *ePtr, *ptr;
	int size=*(int*)shmptr;
	shmptr+=sizeof(int);
	ePtr=ptr=shmptr;
	
	while(ePtr<(shmptr+size)){
		if(!(*(int*)ePtr))//Empty area reached
			break;
		ptr=ePtr+sizeof(int);
		if(!strcmp(ptr, type)){//Same type!
			if(value==NULL){
				*ptr='u';//Mark it as unused
				*(ptr+1)='\0';
			}else if(!strcmp(strchr(ptr, '\0')+1, value)){//Got a match!
				*ptr='u';//Mark it as unused
				*(ptr+1)='\0';
			}
		}
		ePtr=ePtr+(*(int*)ePtr);//NEXT
	}
	return 1;
}
