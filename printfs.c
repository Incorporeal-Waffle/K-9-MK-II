#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include "printfs.h"

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
