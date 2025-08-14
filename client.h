#ifndef CLIENT_H
#define CLIENT_H

#include "canvas.h"

typedef struct client client_t;
struct client{
	int	sockfd;
	uint8_t uid;
	uint8_t color;
	segment_t *curr;
	segment_t *head;
	segment_t *tail;
	client_t *next;
};

char
add_client(client_t **, int, uint8_t);

void
free_clients(client_t *);

#endif