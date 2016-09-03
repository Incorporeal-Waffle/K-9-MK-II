#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "printfs.h"
#include "k-9.h"

int recvd(char *rMsg){
	rPrintf("%s", rMsg);
	
	free(rMsg);
	return 1;
}

int main(int argc, char **argv){
	int sockfd=*argv[0];
	int rbytes;
	char buf[513];
	struct pollfd pollfds[2];

	iPrintf("Meow version %s\n", VERSION);
	if(argc<1){
		ePrintf("Meow: Invalid number of arguments.\n");
		return 1;
	}
	
	pollfds[0].fd=STDIN_FILENO;
	pollfds[0].events=POLLIN|POLLPRI;
	pollfds[1].fd=sockfd;
	pollfds[1].events=POLLIN|POLLPRI;
	while(poll(pollfds, 2, -1)>=0){
		if((pollfds[1].revents & POLLERR) || (pollfds[1].revents & POLLHUP)){//sock
			ePrintf("An error has occured on the socket.\n");
			return 0;
		}
		if((pollfds[1].revents&POLLIN)||(pollfds[1].revents&POLLPRI)){
			rbytes=read(sockfd, buf, 512);

			if(rbytes<0){
				ePrintf("Read error: %s\n", strerror(errno));
				return 2;
				break;
			}
			if(rbytes>512){
				iPrintf("rbytes>512\n");
			}
			
			buf[rbytes]=0;
			recvd(strdup(buf));
		}
		if(pollfds[0].revents){//stdin
			rbytes=read(STDIN_FILENO, buf, 512);
			buf[rbytes]=0;
			sPrintf(sockfd, "%s", buf);

		}
	}
	
	return 0;//What is this doing here again?
}
