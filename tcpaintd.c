#include "client.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INCOMING 5
#define BACKLOG 5

typedef struct server server_t;
struct server{
	int sockfd;
	struct pollfd pending[MAX_INCOMING];
	client_t *c;
};

static char
init_server(server_t *s){
	struct addrinfo hints, *res;
	int opt = 1, i;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if(getaddrinfo(NULL, PORT, &hints, &res) < 0) return 0;

	s->sockfd = socket(PF_INET, SOCK_STREAM, 0);
	setsockopt(s->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(bind(s->sockfd, res->ai_addr, res->ai_addrlen) < 0){
		close(s->sockfd);
		return 0;
	}
	freeaddrinfo(res);
	listen(s->sockfd, BACKLOG);

	s->c = NULL;

	for(i = 0; i < MAX_INCOMING; i++){
		s->pending[i].fd = -1;
		s->pending[i].events = POLLIN;
	}
	return 1;
}

static char
handle_incoming(server_t *s){
	struct pollfd pfds;
	int i;
	uint8_t uid;

	pfds.fd = s->sockfd;
	pfds.events = POLLIN;
	poll(&pfds, 1, 0);
	for(i = 0; pfds.revents & POLLIN && i < MAX_INCOMING; i++){
		if(s->pending[i].fd == -1){
			s->pending[i].fd = accept(s->sockfd, NULL, NULL);
			printf("Connection accepted, waiting for UID...\n");
			break;
		}
	}

	poll(s->pending, MAX_INCOMING, 0);
	for(i = 0; i < MAX_INCOMING; i++){
		if(s->pending[i].fd != -1 && s->pending[i].revents & POLLIN){
			recv(s->pending[i].fd, &uid, 1, 0);
			s->pending[i].fd = -1;
			printf("Connection complete from UID %d.\n", uid);
			return 0;
		}
	}
	return 1;
}

int
main(void){
	server_t s;

	if(!init_server(&s)){
		fprintf(stderr, "Failed to initalize server.\n");
		return 1;
	}

	while(handle_incoming(&s));

	return 0;
}