#include <stdio.h>
void __attribute__ ((constructor)) omnominit(){
	printf("Hello from the test module!\n");
}
void __attribute__ ((destructor)) omnomdes(){
	printf("Test module destructor called!\n");
}
