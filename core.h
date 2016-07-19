#define VERSION "0.0.0"
#include "config.h"

int sockfd, gArgc;
char **gArgv;
char *serverHost='\0';
char *serverPort='\0';
char *moduleDir='\0';

//Functions
int main(int argc, char **argv);
int loadModule(char *modulePath);
int loadModules(char *moduleDir);

