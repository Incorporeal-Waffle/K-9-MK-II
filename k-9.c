#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "k-9.h"

int sPrintf(int sockfd, char *format, ...){// Send and print
	va_list ap;
	int retval=0;
	printf("\x1b[1;34m");
	va_start(ap, format);
	vprintf(format, ap);
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

int main(int argc, char **argv){
	int sockfd;
	char opt;
	char *sHost = HOST;
	char *sPort = PORT;
	char *nick = NICK;
	char *rName = REALNAME;
	char *userName = REALNAME;
	struct addrinfo *host, hints;

	while((opt=getopt(argc, argv, "Vhd:p:n:r:u:")) != -1){
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
			case 'u':// Realname
				userName=optarg;
				break;
			case 'V':
				printf("%s\n", VERSION);
				return 0;
			case 'h':
				printf("TODO: usage information\n");
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
	switch(getaddrinfo(sHost, sPort, &hints, &host)){
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
	for(struct addrinfo *addr=host; addr!=NULL; addr=addr->ai_next){
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

	iPrintf("Connection established.\n");
	
	sPrintf(sockfd, "USER %s 0 0 :%s\r\nNICK %s\r\n", userName, rName, nick);

	return 0;
}
