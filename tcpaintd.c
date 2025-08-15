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
		s->pending[i].events = POLLIN | POLLHUP;
	}
	return 1;
}

static void
stop_server(server_t *s){
	int i;

	free_clients(s->c);
	close(s->sockfd);
	for(i = 0; i < MAX_INCOMING; i++)
		if(s->pending[i].fd != -1) close(s->pending[i].fd);
}

static char
send_state(int sockfd, server_t *s){
	client_t *curr_c;
	segment_t *curr_seg;

	for(curr_c = s->c; curr_c; curr_c = curr_c->next){
		for(curr_seg = curr_c->head; curr_seg; curr_seg = curr_seg->next){
			if(send(sockfd, &curr_c->uid, 1, 0) <= 0) return 0;
			if(!send_segment(sockfd, curr_seg)) return 0;
		}
	}
	return 1;
}

static void
handle_incoming(server_t *s){
	struct pollfd pfds;
	int i;
	uint8_t uid;

	pfds.fd = s->sockfd;
	pfds.events = POLLIN;
	poll(&pfds, 1, 0);
	for(i = 0; pfds.revents & POLLIN && i < MAX_INCOMING; i++){
		if(pfds.revents & POLLIN && s->pending[i].fd == -1){
			s->pending[i].fd = accept(s->sockfd, NULL, NULL);
			printf("Connection accepted, waiting for UID...\n");
			break;
		}
	}

	poll(s->pending, MAX_INCOMING, 0);
	for(i = 0; i < MAX_INCOMING; i++){
		if(s->pending[i].fd != -1 && s->pending[i].revents & POLLHUP){
			close(s->pending[i].fd);
			s->pending[i].fd = -1;
		}
		else if(s->pending[i].fd != -1 && s->pending[i].revents & POLLIN){
			if(recv(s->pending[i].fd, &uid, 1, 0) <= 0) close(s->pending[i].fd);
			else if(send_state(s->pending[i].fd, s)){
				printf("UID %d connected.\n", uid);
				if(!add_client(&s->c, s->pending[i].fd, uid)) close(s->pending[i].fd);
			}
			else close(s->pending[i].fd);
			s->pending[i].fd = -1;
		}
	}
}

static void
disconnect_client(client_t *c){
	printf("UID %d disconnected.\n", c->uid);
	close(c->sockfd);
	c->sockfd = -1;
}

static void
forward_ctrl(server_t *s, uint8_t src_uid, uint8_t type, uint8_t color){
	client_t *curr;

	for(curr = s->c; curr && curr->sockfd != -1 && curr->uid != src_uid; curr = curr->next)
		if(send(curr->sockfd, &src_uid, 1, 0) <= 0 || !send_ctrl(curr->sockfd, type, color, 0))
			disconnect_client(curr);
}

static void
forward_point(server_t *s, uint8_t src_uid, int16_t x, int16_t y){
	client_t *curr;

	for(curr = s->c; curr && curr->sockfd != -1 && curr->uid != src_uid; curr = curr->next)
		if(send(curr->sockfd, &src_uid, 1, 0) <= 0 || !send_point(curr->sockfd, x, y))
			disconnect_client(curr);
}

static void
handle_events(server_t *s){
	client_t *curr;
	struct pollfd pfds;
	uint8_t type, color;
	int16_t x, y;

	for(curr = s->c; curr && curr->sockfd != -1; curr = curr->next){
		pfds.fd = curr->sockfd;
		pfds.events = POLLIN | POLLHUP;
		poll(&pfds, 1, 0);
		if(pfds.revents & POLLHUP) disconnect_client(curr);
		else if(pfds.revents & POLLIN){
			if(curr->curr){
				if(!recv_point(curr->sockfd, &x, &y)) disconnect_client(curr);
				else{
					add_point(curr->curr, x, y);
					forward_point(s, curr->uid, x, y);
				}
			}
			else if((type = recv_ctrl(curr->sockfd, &color, NULL))){
				forward_ctrl(s, curr->uid, type, color);
				switch(type){
					case POINT_STREAM_START:
						if(!recv_point(curr->sockfd, &x, &y)) disconnect_client(curr);
						else{
							curr->curr = new_segment(x, y, color);
							forward_point(s, curr->uid, x, y);
						}
						break;
					case POINT_STREAM_END:
						add_segment(curr->curr, &curr->head, &curr->tail);
						curr->curr = NULL;
						break;
					case CLEAR:
						clear_canvas(&curr->head, &curr->tail);
						break;
				}
			}
			else disconnect_client(curr);
		}
	}
}

static char
quit_requested(){
	struct pollfd pfds;

	pfds.fd = STDIN_FILENO;
	pfds.events = POLLIN;
	poll(&pfds, 1, 0);
	if(pfds.revents & POLLIN){
		if(getchar() == 'q') return 1;
		else while(getchar() != EOF);
	}
	return 0;
}

int
main(void){
	server_t s;

	if(!init_server(&s)){
		fprintf(stderr, "Failed to initalize server.\n");
		return 1;
	}

	while(!quit_requested()){
		handle_incoming(&s);
		handle_events(&s);
	}

	stop_server(&s);
	return 0;
}
