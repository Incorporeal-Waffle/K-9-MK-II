#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include "etc.h"

int messageDump(struct message *pMsg){//Dumps the contents of a message struct
	iPrintf("original: %s$\n", pMsg->original);
	
	iPrintf("name: %s$\n", pMsg->name);
	iPrintf("user: %s$\n", pMsg->user);
	iPrintf("host: %s$\n", pMsg->host);
	
	iPrintf("command: %s$\n", pMsg->command);
	
	iPrintf("params: %s$\n", pMsg->params);
	iPrintf("trailing: %s$\n", pMsg->trailing);
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
