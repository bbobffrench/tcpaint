#include "canvas.h"

#include <sys/socket.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <string.h>

segment_t *
new_segment(int16_t x, int16_t y, uint8_t color){
	segment_t *segment;

	segment = malloc(sizeof(segment_t));
	segment->color = color;
	segment->head = malloc(sizeof(point_t));
	segment->head->x = x;
	segment->head->y = y;
	segment->head->next = NULL;
	segment->tail = segment->head;
	return segment;
}

void
add_point(segment_t *segment, int16_t x, int16_t y){
	point_t *new;

	new = malloc(sizeof(point_t));
	new->x = x;
	new->y = y;
	new->next = NULL;
	segment->tail->next = new;
	segment->tail = new;
}

void
free_segment(segment_t *segment){
	point_t *curr, *next;

	for(curr = segment->head; curr; curr = next){
		next = curr->next;
		free(curr);
	}
	free(segment);
}

void
add_segment(segment_t *segment, segment_t **head, segment_t **tail){
	if(!segment->head->next) return; /* discard single-point segments */
	if(!*head) *head = *tail = segment;
	else{
		(*tail)->next = segment;
		(*tail) = segment;
	}
}

void
clear_canvas(segment_t **head, segment_t **tail){
	segment_t *curr, *next;

	for(curr = *head; curr; curr = next){
		next = curr->next;
		free_segment(curr);
	}
	*head = *tail = NULL;
}

char
send_ctrl(int sockfd, uint8_t type, uint8_t color, uint16_t num_points){
	uint8_t ctrl[4];

	ctrl[0] = type;
	ctrl[1] = color;
	num_points = htons(num_points);
	memcpy(&ctrl[2], &num_points, sizeof(num_points));
	if(send(sockfd, ctrl, sizeof(ctrl), 0) <= 0) return 0;
	return 1;
}

char
recv_ctrl(int sockfd, uint8_t *color, uint16_t *num_points){
	uint8_t ctrl[4];

	if(!recv(sockfd, ctrl, sizeof(ctrl), 0) <= 0) return 0;
	*color = ctrl[1];
	memcpy(num_points, &ctrl[2], sizeof(uint16_t));
	*num_points = ntohs(*num_points);
	return ctrl[0];
	return 1;
}

char
send_segment(int sockfd, segment_t *segment){
	point_t *curr;
	int16_t *buff;
	uint16_t num_points, i;

	num_points = 0;
	for(curr = segment->head; curr; curr = curr->next) num_points++;

	if(!send_ctrl(sockfd, SEGMENT, segment->color, num_points)) return 0;

	i = 0;
	buff = malloc(sizeof(int16_t) * num_points * 2);
	for(curr = segment->head; curr; curr = curr->next){
		buff[i++] = htons(curr->x);
		buff[i++] = htons(curr->y);
	}
	if(send(sockfd, buff,  sizeof(int16_t) * num_points * 2, 0) <= 0){
		free(buff);
		return 0;
	}
	free(buff);
	return 1;
}

segment_t *
recv_segment(int sockfd, uint8_t color, uint16_t num_points){
	int16_t *buff;
	int i;
	segment_t *new;

	buff = malloc(sizeof(int16_t) * num_points * 2);
	if(recv(sockfd, buff, sizeof(int16_t) * num_points * 2, 0) <= 0){
		free(buff);
		return NULL;
	}
	new = new_segment(ntohs(buff[0]), ntohs(buff[1]), color);
	for(i = 2; i < num_points * 2; i += 2)
		add_point(new, ntohs(buff[i]), ntohs(buff[i + 1]));
	free(buff);
	return new;
}

char
start_point_stream(int sockfd, uint8_t color, int16_t x, int16_t y){
	int16_t buff[2];

	if(!send_ctrl(sockfd, POINT_STREAM_START, color, 0)) return 0;
	buff[0] = htons(x);
	buff[1] = htons(y);
	if(send(sockfd, buff, sizeof(buff), 0) <= 0) return 0;
	return 1;
}

char
end_point_stream(int sockfd){
	return send_ctrl(sockfd, POINT_STREAM_START, 0, 0);
}

char
recv_point(int sockfd, segment_t *segment){
	int16_t buff[2];

	if(recv(sockfd, buff, sizeof(buff), 0) <= 0) return 0;
	add_point(segment, ntohs(buff[0]), ntohs(buff[1]));
	return 1;
}
