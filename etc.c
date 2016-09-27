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

int messageDump(struct message *pMsg){//Dumps the contents of a message struct
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
int sPrintf(int sockfd, char *format, ...){// Send and print
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
			if(!strcmp(strchr(ptr, '\0')+1, value)){//Got a match!
				*ptr='u';//Mark it as unused
				*(ptr+1)='\0';
			}
		}
		ePtr=ePtr+(*(int*)ePtr);//NEXT
	}
	return 1;
}
