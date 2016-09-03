#include "config.h"

int sockfd;
char *sHost, *sPort, *nick, *rName, *userName;


struct prefix{
	char *name;
	char *user;
	char *host;
};

struct params{
	struct params *nParam;
	char *trailing;
};

struct message{
	struct prefix prefix;
	char *command;
	struct params params;
};
