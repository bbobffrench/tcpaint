#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>

enum ctrl_types{POINT_STREAM_START = 1, POINT_STREAM_END, SEGMENT};

typedef struct point point_t;
struct point{
	int16_t x;
	int16_t y;
	point_t *next;
};

typedef struct segment segment_t;
struct segment{
	uint8_t color;
	point_t *head;
	point_t *tail;
	segment_t *next;
};

segment_t *
new_segment(int16_t, int16_t, uint8_t);

void
add_point(segment_t *, int16_t, int16_t);

void
free_segment(segment_t *);

void
add_segment(segment_t *, segment_t **, segment_t **);

void
clear_canvas(segment_t **, segment_t **);

char
send_ctrl(int, uint8_t, uint8_t, uint16_t);

char
recv_ctrl(int, uint8_t *, uint16_t *);

char
send_segment(int, segment_t *);

segment_t *
recv_segment(int, uint8_t, uint16_t);

char
start_point_stream(int, uint8_t, int16_t, int16_t);

char
end_point_stream(int);

char
recv_point(int, segment_t *);

#endif
