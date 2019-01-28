typedef int (*orig_connect_type)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

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

        orig_connect_type orig_connect;
        orig_connect = (orig_connect_type) dlsym(RTLD_NEXT, "connect");
        int ret = orig_connect(sockfd, addr, addrlen);
        PRINTF(("[OUT] %d\n", ret));
        return ret;

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
