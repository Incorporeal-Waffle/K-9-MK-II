#include "k-9.h"

int messageDump(struct message *pMsg);
int sConnect(char *hostname, char *port);

//Printfs
int sPrintf(int sockfd, char *format, ...);
int rPrintf(char *format, ...);
int ePrintf(char *format, ...);
int iPrintf(char *format, ...);
