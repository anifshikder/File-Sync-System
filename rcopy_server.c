#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "ftree.h"
#include "hash.h"

#ifndef PORT
  #define PORT 30000
#endif


int main(int argc, char **argv) {
	if(argc == 1){
		fcopy_server(PORT);
	}
	else{
		perror("more arguments than needed");
		exit(-1);
	}
	return 0;
}
