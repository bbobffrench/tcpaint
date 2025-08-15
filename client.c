#include "client.h"

#include <unistd.h>

#include <stdlib.h>

char
add_client(client_t **client, int sockfd, uint8_t uid){
	client_t *curr, *new;

	new = malloc(sizeof(client_t));
	new->sockfd = sockfd;
	new->uid = uid;
	new->color = 0;
	new->curr = new->head = new->tail = NULL;
	new->next = NULL;

	if(!*client){
		*client = new;
		return 1;
	}
	for(curr = *client; curr; curr = curr->next){
		if(curr->uid == uid){
			if(curr->sockfd == -1){
				curr->sockfd = sockfd;
				break;
			}
			else{
				free(new);
				return 0;
			}
		}
		if(!curr->next){
			curr->next = new;
			break;
		}
	}
	return 1;
}

void
free_clients(client_t *head){
	client_t *curr, *next;

	for(curr = head; curr; curr = next){
		if(curr->sockfd != -1) close(curr->sockfd);
		if(curr->curr) free_segment(curr->curr);
		clear_canvas(&curr->head, &curr->tail);
		next = curr->next;
		free(curr);
	}
}