#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h>

#define DATA_READ 1
#define DATA_WRITE 2

typedef int (*orig_connect_type)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

struct fd_addr_entry{
	char *addr;
	int port;
	int fd;
	size_t write_total;
	size_t read_total;
};

static int fd_addr_size = 0;
static struct fd_addr_entry **mappings = NULL;

// try to get the offset in **mappings for the fd, return -1 if nothings found
int get_offset(int fd){
	for(int i=0;i<fd_addr_size;i++){
		int offset = sizeof(struct fd_addr_entry) * i;
		if(mappings[offset]->fd == fd){
			return offset;
		}
	}
	return -1;
}
void add(struct sockaddr_in* addr, int fd){
	struct sockaddr_in *sin;
	sin = (struct sockaddr_in *) addr;
	char *result = inet_ntoa(sin->sin_addr);
	
	char* addrstr_buffer = malloc(strlen(result));
	strcpy(addrstr_buffer, result);

	int offset = get_offset(fd);
	if(offset != -1){
		//fd already available, just update the addr
		free(mappings[offset]->addr);
		mappings[offset]->addr = addrstr_buffer;
		mappings[offset]->port = ntohs(sin->sin_port);
		mappings[offset]->write_total = 0; //reset counters
		mappings[offset]->read_total = 0;
		return;
	}

	//if we're here, that means no fd is available --> create a new one
	
	//increase array
	mappings = realloc(mappings, sizeof(struct fd_addr_entry)* (fd_addr_size+1));

	//create a new entry
	struct fd_addr_entry *new_entry =  malloc(sizeof(struct fd_addr_entry));

	//setup everything and assign it to the end of the list
	new_entry->fd = fd;
	new_entry->addr = addrstr_buffer;
	new_entry->port = ntohs(sin->sin_port);
	new_entry->write_total = 0;
	new_entry->read_total = 0;

	mappings[fd_addr_size] = new_entry;

	fd_addr_size++; //increase size
}

char* get(int fd){
	int offset = get_offset(fd);
	if(offset == -1){
		return NULL;
	}
	mappings[offset]->addr;
}



void new_data(int fd, size_t count, int data_type){
	int offset = get_offset(fd);
	if(offset == -1){ //no entry for fd, return now
		return;
	}

	if(data_type == DATA_READ){
		mappings[offset]->read_total+=count; //add new byte size to total count
	}else if(data_type == DATA_WRITE){
		mappings[offset]->write_total+=count; //add new byte size to total count
	}
	printf("%s %d %d %d\n", mappings[offset]->addr, mappings[offset]->port, mappings[offset]->read_total, mappings[offset]->write_total);
}


// connect systemcall replacement
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
	int ret = orig_connect(sockfd, addr, addrlen);
	printf("[OUT] %d\n", ret);
	return ret;

}

// write systemcall replacement
typedef size_t (*orig_write_type)(int fd, const void *buf, size_t count);

size_t write(int fd, const void *buf, size_t count){
	new_data(fd, count, DATA_WRITE);
	orig_write_type orig_write;
	orig_write = (orig_write_type) dlsym(RTLD_NEXT, "write");
	return orig_write(fd, buf, count);
}

// read systemcall replacement
typedef ssize_t (*orig_read_type)(int fd, void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count){
	new_data(fd, count, DATA_READ);
	orig_read_type orig_read;
	orig_read = (orig_read_type) dlsym(RTLD_NEXT, "read");
	return orig_read(fd, buf, count);
}
