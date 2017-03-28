#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSIZE 1460

#define EPOLL_SIZE 10000

int main(int argc, char* argv[]) {
	int epoll_fd, i;
	struct epoll_event* events;
	struct epoll_event init_event;
	char message[BUFSIZE + 1];

	events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);
	if(events == NULL) {
		printf("malloc fail\n");
		return 0;
	}

	epoll_fd = epoll_create(EPOLL_SIZE);
	if(epoll_fd == -1) {
		printf("epoll_create error\n");
		return 0;
	}
	
	int server_socket;
	struct sockaddr_in serv_addr;

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket == -1) {
		printf("open server socket error\n");
		return 0;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	if(bind(server_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
		printf("bind() error\n");
		return 0;
	}

	if(listen(server_socket, 3000) == -1) {
		printf("listen() error\n");
		return 0;
	}

	init_event.events = EPOLLIN;
	init_event.data.fd = server_socket;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &init_event);

	while(1) {
		int event_count = epoll_wait(epoll_fd, events, EPOLL_SIZE, 10);
		if(event_count == -1)
			break;

		for(i = 0; i < event_count; i++) {
			if(events[i].data.fd == server_socket) {
				// accept 처리
				struct sockaddr_in clnt_addr;
				int clnt_len = sizeof(clnt_addr);
				int clnt_sock = accept(server_socket, (struct sockaddr*)&clnt_addr, &clnt_len);

				init_event.events = EPOLLIN;
				init_event.data.fd = clnt_sock;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clnt_sock, &init_event);
			} else {
				int sock = events[i].data.fd;
				int str_len = read(sock, message, BUFSIZE);
				if(str_len < 1) {
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock, &init_event);
					close(sock);
					printf("disconn : fd %d\n", sock);
				} else {
					write(sock, message, str_len);
				}
			}
		}
	}

	close(server_socket);
	close(epoll_fd);

	return 0;
}
