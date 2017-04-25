#include <stdio.h>
#include <string.h>
#include "ftree.h"


#ifndef PORT
  #define PORT 30000
#endif

int main(int argc, char **argv) {
   if (argc == 4) {
	if ((fcopy_client(argv[1], argv[2], argv[3], PORT)) != 0){
	     printf("fcopy_client failed");
             return -1;
	}
   } else {
	// made for testing, not sure if will cause problem
	printf("in sufficient arguments");
	return -1;
   }
return 0;
}
