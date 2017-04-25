#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>

#include "hash.h"
#include "ftree.h"

void create_struct(char *src_path, char *dest_path, int socres, struct stat buf);
void traverse_dir_files (char *src_path, char *dest_path, int socres);
void check_receivedData(struct fileinfo *receivedData, int fd);

int fcopy_client(char *src_path, char *dest_path, char *host, int port){
   int socres;
   struct sockaddr_in server;
   if ((socres = socket(AF_INET,SOCK_STREAM,0)) == -1){
      // change it to something meaningful
      perror("issues with setting up socket");
      return -1;
   }

   server.sin_family = AF_INET;
   server.sin_port = htons(port);

   if (inet_pton(AF_INET, host, &server.sin_addr) < 1) {
      // change it to something meaningful
      perror("issues with inet_pton");
      close(socres);
      return -1;
   }
   if (connect(socres, (struct sockaddr *)&server, sizeof(server)) == -1) {
      perror("issue with server connect");
      return -1;
   }
    traverse_dir_files (src_path, dest_path, socres);

close(socres);
return 0;
}

void create_struct(char *src_path, char *dest_path, int socres, struct stat buf){
      FILE *openedfile;
      char hashval[HASH_SIZE];

      if (S_ISREG(buf.st_mode)){
	  if((openedfile = fopen(src_path, "r")) == NULL){
	      perror("file couldn't be opened, maybe permission issue ?");
	      exit(1);
          } else{
	       strcpy(hashval, hash(openedfile));   
          }
       }
       char path[MAXPATH];
       strcpy(path, dest_path);
       strcat(path, "\r\n");
       int size = htonl(buf.st_size);
       int mode = htonl(buf.st_mode);

       write(socres, path, MAXPATH);
       write(socres, &mode, sizeof(buf.st_mode));
       write(socres, hashval, HASH_SIZE);
       write(socres, &size, sizeof(size));
}
	
void traverse_dir_files (char *src_path, char *dest_path, int socres){
    DIR *directory;
    FILE *openedfile;
    struct stat buf;
    struct dirent *files;

    if (lstat(src_path, &buf) != 0){
        fprintf(stderr,"lstat: the name '%s' is not a file or directory\n", src_path);
        exit(1);
    }
    else{
	if(!(S_ISLNK(buf.st_mode))){

		char *srcbase= basename(src_path);
		char dest [MAXPATH];
		strcpy(dest, dest_path);
		strcat(dest, "/");
		strcat(dest, srcbase);

		create_struct(src_path, dest, socres , buf);

		int red;
		read(socres, &red, sizeof(int));

		if (red != MATCH_ERROR){
		if (red == MATCH){
			  if (S_ISDIR(buf.st_mode)){
				
				directory = opendir(src_path);
				while ((files = readdir(directory)) != NULL){
					if(files->d_name[0] != '.'){
						char path [(strlen(src_path) + strlen(files->d_name) + 2)];
						strcpy(path, src_path);
						strcat(path, "/");
						strcat(path, files->d_name);
						traverse_dir_files (path, dest, socres);
					}
				}
			   }
	         }
		 else if (red == MISMATCH){
		      	  if (S_ISREG(buf.st_mode)){
				  if((openedfile = fopen(src_path, "r")) == NULL){
				      // SHOULD THIS BE SOMETHING ELSE? USING PRINT BTW
				      perror(src_path);
				      exit(1);
				  }
   				  // ANIF PATCH
    				  fseek(openedfile, 0, SEEK_END); 
   				  int size = ftell(openedfile);
    				  char ch[size];
				  fseek(openedfile,0,SEEK_SET);

    				  for(int i = 0; i < size; i++){
          				 fscanf(openedfile, "%c", &ch[i]);
    				  }
				  write(socres, &ch, sizeof(ch));
				  fclose(openedfile);

				  int x;
				  read(socres, &x, sizeof(int));

				  if (x == TRANSMIT_ERROR){
				      	// SHOULD THIS BE SOMETHING ELSE? USING PRINT BTW
				      	perror("transmit error");
				      	exit(1);
				  }
			   }
		}
		}
	    }
	}
   }
// -------------------------------------------------------------------------------------------------------------
int setup(void) {

  int on = 1, status;
  struct sockaddr_in self;
  int listenfd;

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }

  // Make sure we can reuse the port immediately after the
  // server terminates.
  status = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                      (const char *) &on, sizeof(on));
  if(status == -1) {
    perror("setsockopt -- REUSEADDR");
  }

  self.sin_family = AF_INET;
  self.sin_addr.s_addr = INADDR_ANY;
  self.sin_port = htons(PORT);
  memset(&self.sin_zero, 0, sizeof(self.sin_zero));  // Initialize sin_zero to 0

  if (bind(listenfd, (struct sockaddr *)&self, sizeof(self)) == -1) {
    perror("bind"); // probably means port is in use
    exit(1);
  }

  if (listen(listenfd, 5) == -1) {
    perror("listen");
    exit(1);
  }
  return listenfd;
}

/*Search the first inbuf characters of buf for a network newline ("\r\n").
  Return the location of the '\r' if the network newline is found,
  or -1 otherwise.
  Definitely do not use strchr or any other string function in here. (Why not?)
*/


int find_network_newline(const char *buf, int inbuf) {
  	// Step 1: write this function
	int isthere = -1;
	int yes = 0;
 	for(int i = 0; i < inbuf; i++){
		if(yes == 1){
			if(buf[i] == '\n'){
				isthere = i - 1;
			}
			yes = 0;
		}
		if(buf[i] == '\r'){
			yes = 1;
		}
	}
  	return isthere; // return the location of '\r' if found
}

void fcopy_server(int port){

	int mode, fd, client_fd, size, nbytes;

	char path[255];
	char hash[8];

	uint32_t modeP;
	uint32_t sizeP;

	client_fd = setup();

	struct sockaddr_in peer;
	socklen_t socklen; 

	while(1){
		if ((fd = accept(client_fd, (struct sockaddr *)&peer, &socklen)) < 0) {
	      			perror("accept not working");
		}
		for(int i = 0;i < MAXPATH; i++){
			path[i] = '\0';
		    }
		while ((nbytes = read(fd, path, 255)) > 0) {
		    int where;
		    where = find_network_newline(path, nbytes);
		    path[where] = '\0';
		 
		    read(fd, &modeP, sizeof(uint32_t));
		    mode = ntohl(modeP);
		 
		    nbytes = read(fd, hash, 8);
		    hash[nbytes] = '\0';
		 
		    read(fd, &sizeP, sizeof(uint32_t));
		    size = ntohl(sizeP);
		
		    struct fileinfo *receivedData = malloc(sizeof(struct fileinfo));
		    strcpy(receivedData->path, path);
		    receivedData->mode = mode;
		    strcpy(receivedData->hash, hash);
		    receivedData->size = size;
		    check_receivedData(receivedData, fd);
		    free(receivedData);

		    for(int i = 0; i < MAXPATH; i++){
			path[i] = '\0';
		    }
		    for(int i = 0; i < HASH_SIZE; i++){
			hash[i] = '\0';
		    }
		}
		close(fd);
	}
}

void file_copy(FILE *newDestFile, int fd, struct fileinfo *receivedData){
	// ANIF PATCH
	char ch[receivedData->size];
	read(fd, ch, receivedData->size);
	int res= fwrite(ch,1,sizeof(ch),newDestFile);
	if (res == EOF) {
	}
}

void check_receivedData(struct fileinfo *receivedData, int fd){
	struct stat stDest;
	char *path = receivedData->path;
	int retDest = lstat(path, &stDest);
	int match = MATCH;
	int mismatch = MISMATCH;
	int match_error = MATCH_ERROR;
	int transmit_ok = TRANSMIT_OK;
	// If file not found.
	if(S_ISREG(receivedData->mode)){
			// check if another file with same name exists in dest
			if(retDest == 0){
				// compare size
				if(S_ISDIR(stDest.st_mode)){
					write(fd,&match_error,sizeof(int));
					perror("match error");
				}
				else{
					FILE *newDestFile  = fopen(path,"r");
					int sameSize = 0;
					int sameHash = 0;
					if(receivedData->size == stDest.st_size){
						sameSize = 1;
					}
					// If size is same then compare hash values
					if(sameSize){
						char *hashDest = hash(newDestFile);
						if(strcmp(receivedData->hash, hashDest) == 0){
							sameHash = 1;
						}
					}
					// if not same size and not the same hash then send mismatch and get back contents.
					fclose(newDestFile);
					if(!(sameSize) || !(sameHash)){
						FILE *newDestFile  = fopen(path,"w+");
						write(fd,&mismatch,sizeof(int));
						file_copy(newDestFile, fd, receivedData);
						write(fd,&transmit_ok,sizeof(int));
						fclose(newDestFile);
					}
					else if((sameSize) && (sameHash)){
						if((receivedData->mode & 0777) != (stDest.st_mode & 0777)){
							int change;
							change = chmod(path,stDest.st_mode);
							if(change == -1){
								write(fd,&mismatch,sizeof(int));
							}
						// ANIF PATCH
						}
						else{
							write(fd,&match,sizeof(int));
						}
							
					}
				}
			}
			// if no other file with same name exists
			// then create one and copy data.
			else if(retDest != 0){
				// creating a file
				FILE *newDestFile = fopen(path,"w+");
				write(fd,&mismatch,sizeof(int));
				file_copy(newDestFile, fd, receivedData);
				write(fd,&transmit_ok,sizeof(int));
				// ANIF PATCH
				fclose(newDestFile);
				chmod(path,receivedData->mode);
				
			}
	}
	else if(S_ISDIR(receivedData->mode)){
		// if src exists then change permission if not same
		if(retDest == 0){
			if(S_ISREG(stDest.st_mode) || S_ISLNK(stDest.st_mode)){
				write(fd,&match_error,sizeof(int));
			}
			if((receivedData->mode & 0777) != (stDest.st_mode & 0777)){
				int change;
				change = chmod(path,receivedData->mode);
				if(change == -1){
					write(fd,&mismatch,sizeof(int));
				}
			}
			// ANIF PATCH
			else{
				write(fd,&match,sizeof(int));
			}

		}
		else if(retDest != 0){
			mkdir(path, receivedData->mode);
			write(fd,&match,sizeof(int));
		}
		
	}
}
