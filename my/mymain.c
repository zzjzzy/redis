//
// Created by root on 4/2/26.
//
#include "stdio.h"
#include "sys/socket.h"
#include "sys/epoll.h"
#include "arpa/inet.h"
#include "unistd.h"

void epollDemo() {
    printf("startEpoll\n");
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("listen_fd is %d\n", listen_fd);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 128);
    int epfd = epoll_create1(0);
    struct epoll_event ev, events[10];
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);
    printf("start sleep\n");
    sleep(1);
    printf("stop sleep\n");
    while (1) {
        int n = epoll_wait(epfd, events, 10, -1);
        printf("n is: %d\n", n);
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == listen_fd) {
                int conn_fd = accept(listen_fd, NULL, NULL);
                printf("new conn: %d\n", conn_fd);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev);
            } else {
                char buf[1024];
                int len = read(events[i].data.fd, buf, sizeof(buf));
                printf("receive data, read len: %d\n", len);
                if (len <= 0) {
                    close(events[i].data.fd);
                } else {
                    write(events[i].data.fd, buf, len);
                }
            }
        }
    }

}

void redisCliDemo() {

}

void forkDemo() {
    printf("start fork\n");
    int pid = fork();
    if (pid == 0) {
        printf("child process\n");
    } else {
        printf("parent process\n");
    }
}

void main() {
//    epollDemo();
    forkDemo();
}



