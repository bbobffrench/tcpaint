#include "canvas.h"

#include <stdlib.h>

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