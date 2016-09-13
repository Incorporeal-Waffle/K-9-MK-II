#ifndef K9H
#define K9H

int sockfd;
char *sHost, *sPort, *nick, *rName, *userName;

struct message{
	char *original;
	
	//prefix
	char *name;
	char *user;
	char *host;
	
	//command
	char *command;
	
	//params
	char *params;
	char *trailing;
};

#endif
