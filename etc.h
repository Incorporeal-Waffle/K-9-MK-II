#include "k-9.h"

int messageDump(struct message *pMsg);
int sConnect(char *hostname, char *port);

//Printfs
int sPrintf(int sockfd, char *format, ...);
int rPrintf(char *format, ...);
int ePrintf(char *format, ...);
int iPrintf(char *format, ...);

//mm
#define DEFAULTSHMSIZE 2048
#define SHMKEY '9'
struct mmEntry{
	char *type;
	char *value;
	struct mmEntry *next;
};

//Implementing some crappy memory management for IPC purposes cuz... Uh... Fun?
int mmAddEntry(char *path, char key, char *type, char *value);
struct mmEntry *mmFind(char *path, char key, char *type);
int mmFreeEntry(struct mmEntry *entry);
int mmDelEntry(char *path, char key, char *type, char *value);

int mmDumpEntry(struct mmEntry *entry);
int mmGetId(char *path, char key);
char *mmGetPtr(char *path, char key);
int mmGrowShm(char *path, char key, int newSize);
struct mmEntry *mmMkEntry(char *type, char *value);

