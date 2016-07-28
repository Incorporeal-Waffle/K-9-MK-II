#include "core.h"
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <dirent.h>
#include <dlfcn.h>

#define NPOLLFDS 2

int loadModule(char *modulePath){
	printf("Loading module: %s\n", modulePath);
	void *dlhandle=dlopen(modulePath, RTLD_LAZY);
	if(dlhandle==NULL){
		printf("%s\n", dlerror());
		printf("Failed to load module: %s\n", modulePath);
		return 0;
	}
	dlclose(dlhandle);
	return 1;
}

int loadModules(char *moduleDir){
	DIR *mDir=opendir(moduleDir);
	struct dirent *entt;
	char *dirName;

	printf("Loading modules from: %s\n", moduleDir);

	if(mDir==NULL){
		perror("opendir");
		return 0;
	}
	
	while((entt=readdir(mDir))!=NULL){
		if(strcmp(entt->d_name, ".")&&strcmp(entt->d_name, "..")){
			dirName=strdup(moduleDir);
			dirName=realloc(dirName, strlen(dirName)+2+strlen(entt->d_name));
			strcat(dirName, "/");
			strcat(dirName, entt->d_name);
			loadModule(dirName);
		}
	}
	
	return 1;
}

int main(int argc, char **argv){
	int opt, rbytes;
	struct addrinfo *host, hints;
	struct pollfd pollfds[NPOLLFDS];
	char *dirName;
	char buf[513];
	buf[512]='\0';
	
	gArgc=argc;
	gArgv=argv;

	while((opt=getopt(argc, argv, "Vhd:p:m:")) != -1){
		switch(opt){
			case 'V':
				printf("%s\n", VERSION);
				return 0;
			case 'h':
				printf("TODO: usage information\n");
				return 0;
			case 'd'://Host
				serverHost=optarg;
				break;
			case 'p'://Port
				serverPort=optarg;
				break;
			case 'm'://Module dir
				moduleDir=optarg;
				break;
			default:
				return 1;
		}
	}
	//TODO: -v for verbose, make it silencier by default

	if(serverHost==NULL)
		serverHost="127.0.0.1";
	if(serverPort==NULL)
		serverPort="6667";
	
	//Loading core modules
	if(moduleDir){
		dirName=strdup(moduleDir);
		dirName=realloc(dirName, strlen(dirName)+1+strlen("/core"));//+1 for null
		strcat(dirName, "/core");
	}else{
		dirName=strdup(gArgv[0]);
		dirName=realloc(dirName, strlen(dirName)+1+strlen("/modules/core"));//+1 for null
		dirName=dirname(dirName);//dirname modifies it's args
		strcat(dirName, "/modules/core");
	}

	if(!loadModules(dirName)){
		printf("Failed to load modules!\n");
	}
	free(dirName);

	printf("Connecting to: %s:%s...\n", serverHost, serverPort);

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
		freeaddrinfo(host);
		break;
	}

	if(sockfd==-1){
		perror("connect");
		printf("Could not connect.\n");
		return 1;
	}

	printf("We've got a connection!\n");

	pollfds[0].fd=STDIN_FILENO;
	pollfds[0].events=POLLIN;

	pollfds[1].fd=sockfd;
	pollfds[1].events=POLLIN;
	
	while(poll(pollfds, NPOLLFDS, -1)>0){
		if(pollfds[1].revents){//sock
			rbytes=read(sockfd, buf, 512);
			write(STDOUT_FILENO, buf, rbytes);
			buf[rbytes]=0;
		}

		if(pollfds[0].revents){//stdin
			rbytes=read(STDIN_FILENO, buf, 512);
			write(sockfd, buf, rbytes);
		}
	}

	return 0;
}
