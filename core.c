#include "core.h"
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char **argv){
	int opt;
	struct addrinfo *host, hints;

	while((opt=getopt(argc, argv, "Vhd:p:")) != -1){
		switch(opt){
			case 'V':
				printf("%s\n", VERSION);
				return 0;
			case 'h':
				printf("TODO: usage information\n");
				return 0;
			case 'd':
				serverHost=optarg;
				break;
			case 'p':
				serverPort=optarg;
				break;
			default:
				return 1;
		}
	}

	if(serverHost==NULL)
		serverHost="127.0.0.1";
	if(serverPort==NULL)
		serverPort="6667";
	
	printf("Host: %s\nPort: %s\n", serverHost, serverPort);

	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=0;
	hints.ai_flags=0;
	
	getaddrinfoeaiagainretrything://Single use.
	switch(getaddrinfo(serverHost, serverPort, &hints, &host)){
		case 0:
			//Success!
			break;
		case EAI_AGAIN:
			printf("getaddrinfo returned EAI_AGAIN. Retrying in 5 seconds\n");
			sleep(5);
			goto getaddrinfoeaiagainretrything;//Got a problem with this? Fuck you too! :D
			break;
		case EAI_SYSTEM:
			perror("getaddrinfo");
			return 1;
		default:
			printf("getaddrinfo failed.\n");
			return 1;
	}
	

	for(struct addrinfo *addr=host; addr!=NULL; addr=addr->ai_next){
		sockfd=socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(connect(sockfd, addr->ai_addr, addr->ai_addrlen)){//Couldn't connect
			close(sockfd);//Nope. Next!
			sockfd=-1;
			continue;
		}
		break;//We've got a connection!
	}

	if(sockfd==-1){
		perror("connect");
		printf("Could not connect.");
		return 1;
	}
	
	return 0;
}
