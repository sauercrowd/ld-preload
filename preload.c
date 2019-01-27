#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h>

typedef int (*orig_connect_type)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

struct fd_addr_entry{
	char *addr;
	int fd;
};

static int fd_addr_size = 0;
static struct fd_addr_entry **mappings = NULL;

void add(struct sockaddr_in* addr, int fd){
	int offset;

	struct sockaddr_in *sin;
	sin = (struct sockaddr_in *) addr;
	char *result = inet_ntoa(sin->sin_addr);
	
	char* addrstr_buffer = malloc(strlen(result));
	strcpy(addrstr_buffer, result);

	for(int i=0;i<fd_addr_size;i++){
		offset = sizeof(struct fd_addr_entry) * i;
		if(mappings[offset]->fd == fd){
			//fd already available, just update the addr
			free(mappings[offset]->addr);
			mappings[offset]->addr = addrstr_buffer;
			return;
		}
	}

	//if we're here, that means no fd is available --> create a new one
	
	//increase array
	mappings = realloc(mappings, sizeof(struct fd_addr_entry)* (fd_addr_size+1));

	//create a new entry
	struct fd_addr_entry *new_entry =  malloc(sizeof(struct fd_addr_entry));

	//setup everything and assign it to the end of the list
	new_entry->fd = fd;
	new_entry->addr = addrstr_buffer;
	mappings[fd_addr_size] = new_entry;

	fd_addr_size++; //increase size
}

char* get(int fd){
	for(int i=0;i<fd_addr_size;i++){
		int offset = sizeof(struct fd_addr_entry) * i;
		if(mappings[offset]->fd == fd){
			return mappings[offset]->addr;
		}
	}
	return NULL;
}


int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
	struct sockaddr_in *sin;
	sin = (struct sockaddr_in *) addr;
	add(sin, sockfd);
	char* test_addr = get(sockfd);
	if(test_addr != NULL){
		printf("[%d] %s\n", sockfd, test_addr);
	}

	orig_connect_type orig_connect;
	orig_connect = (orig_connect_type) dlsym(RTLD_NEXT, "connect");
	return orig_connect(sockfd, addr, addrlen);

}

