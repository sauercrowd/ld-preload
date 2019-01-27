#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#define TEXT_FILE "stats.csv"

#define DATA_READ 1
#define DATA_WRITE 2

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


#ifdef DEBUG
#define PRINTF(x) printf x
#else
#define PRINTF
#endif


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
static int init_status = 0;

// try to get the offset in **mappings for the fd, return -1 if nothings found
int get_offset(int fd){
	for(int i=0;i<fd_addr_size;i++){ // KRITISCH
		if(mappings[i]->fd == fd){ //KRITSCH
			return i;
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

	pthread_mutex_lock(&mutex);
	int offset = get_offset(fd);
	if(offset != -1){

		//fd already available, just update the addr
		free(mappings[offset]->addr); //KRITISCH
		mappings[offset]->addr = addrstr_buffer; //KRITSCH
		mappings[offset]->port = ntohs(sin->sin_port); //KRITSCH
		mappings[offset]->write_total = 0; //reset counters //KRITSCH
		mappings[offset]->read_total = 0; //KRITSCH
	}else{

		//if we're here, that means no fd is available --> create a new one
		
		//increase array
		mappings = realloc(mappings, sizeof(struct fd_addr_entry)* (fd_addr_size+1)); // KRITISCH
		//create a new entry
		struct fd_addr_entry *new_entry =  malloc(sizeof(struct fd_addr_entry));

		//setup everything and assign it to the end of the list
		new_entry->fd = fd;
		new_entry->addr = addrstr_buffer;
		new_entry->port = ntohs(sin->sin_port);
		new_entry->write_total = 0;
		new_entry->read_total = 0;

		mappings[fd_addr_size] = new_entry; //KRITISCH
		fd_addr_size++; //increase size //KRTIISCH
	}

	pthread_mutex_unlock(&mutex);

}

char* get(int fd){
	pthread_mutex_lock(&mutex);
	int offset = get_offset(fd);
	if(offset == -1){
		return NULL;
	}
	mappings[offset]->addr; //KRITSCH
	pthread_mutex_unlock(&mutex);

}

void init(){
	pthread_mutex_lock(&mutex);
	if(!init_status){
		//init stuff
	}
	init_status = 1;
	pthread_mutex_unlock(&mutex);
}

void new_data(int fd, const void *buf, size_t count, int data_type){
	pthread_mutex_lock(&mutex);
	int offset = get_offset(fd);

	if(offset != -1){ //no entry for fd, return now
		if(data_type == DATA_READ){
			mappings[offset]->read_total+=count; //add new byte size to total count //KRITSCH
		}else if(data_type == DATA_WRITE){
			mappings[offset]->write_total+=count; //add new byte size to total count //KRITISCH
		}
		//if(mappings[offset]->port == 53){
		//	printf("DNS DATA:\n");
		//	for(int i=0;i<count;i++){
		//		printf("%c", ((char *)buf)[i]);
		//	}
		//	printf("\n");
		//}

		PRINTF(("%s %d %d %d\n",
				mappings[offset]->addr, //KRITISCH
				mappings[offset]->port, //KRITICH
				mappings[offset]->read_total, //KRITSCH
				mappings[offset]->write_total));  //KRITSCH
	}

	pthread_mutex_unlock(&mutex);
}


// connect systemcall replacement
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
	init();
	if(addr->sa_family == AF_INET){
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *) addr;
		add(sin, sockfd);
		char* test_addr = get(sockfd);
		if(test_addr != NULL){
			//printf("[%d] %s\n", sockfd, test_addr);
		}
	}
	else{
		printf("DIFFERENT FAMILY\n");
	}

	orig_connect_type orig_connect;
	orig_connect = (orig_connect_type) dlsym(RTLD_NEXT, "connect");
	int ret = orig_connect(sockfd, addr, addrlen);
	PRINTF(("[OUT] %d\n", ret));
	return ret;

}

// write systemcall replacement
typedef size_t (*orig_write_type)(int fd, const void *buf, size_t count);

size_t write(int fd, const void *buf, size_t count){
	init();
	new_data(fd, buf, count, DATA_WRITE);
	orig_write_type orig_write;
	orig_write = (orig_write_type) dlsym(RTLD_NEXT, "write");
	return orig_write(fd, buf, count);
}

void write_to_file(){

	FILE *f = fopen(TEXT_FILE, "w");
	if (f == NULL)
	{
		printf("Error opening file!\n");
		return;
	}

	pthread_mutex_lock(&mutex);
	fprintf(f, "host,port,read,write\n");
	for(int i=0;i<fd_addr_size;i++){ //KRITISCH
		fprintf(f, "%s,%d,%d,%d\n", 
				mappings[i]->addr, //KRITSCH
				mappings[i]->port, //KRITSCH
				mappings[i]->read_total, //KRITSCH
				mappings[i]->write_total); //KRITSCH
	}
	pthread_mutex_unlock(&mutex);
	fclose(f);
}


// read systemcall replacement
typedef ssize_t (*orig_read_type)(int fd, void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count){
	init();
	new_data(fd, buf, count, DATA_READ);
	orig_read_type orig_read;
	orig_read = (orig_read_type) dlsym(RTLD_NEXT, "read");
	return orig_read(fd, buf, count);
}

//close systemcall replacement
typedef int (*orig_close_type)(int fd);
int close(int fd){
	pthread_mutex_lock(&mutex);
	int res = get_offset(fd);
	pthread_mutex_unlock(&mutex);
	if(res != -1){
		write_to_file();
	}
	orig_close_type orig_close;
	orig_close = (orig_close_type) dlsym(RTLD_NEXT, "close");
	return orig_close(fd);
}

