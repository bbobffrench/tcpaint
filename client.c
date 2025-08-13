#include "canvas.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#define WINDOW_W 800
#define WINDOW_H 800

#define COLORSEL_H 50
#define COLORSEL_HL_H 10

#define SAMPLING_INTERVAL 50
#define FRAME_DELAY 4

const char colors[][3] = {
	{0x00, 0x00, 0x00}, /* black */
	{0xff, 0x00, 0x00}, /* red */
	{0x00, 0xff, 0x00}, /* green */
	{0xff, 0xaf, 0x00}, /* orange */
	{0x00, 0x00, 0xff}, /* blue */
	{0xff, 0x00, 0xff}, /* magenta */
	{0x00, 0xff, 0xff}, /* cyan */
	{0x96, 0x4b, 0x00}  /* brown */
};

const char hl_color[] = {0x89, 0x89, 0x89}; /* gray */

typedef struct client client_t;
struct client{
	SDL_Window *w;
	SDL_Renderer *r;
	uint8_t color;
	uint32_t last_sample;
	segment_t *curr;
	segment_t *head;
	segment_t *tail;
};

char
init_window(client_t *c){
	if(SDL_Init(SDL_INIT_VIDEO)){
		fprintf(stderr, "SDL2 failed to initialize.\n");
		return 0;
	}
	c->w = SDL_CreateWindow(
		"tcpaint",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		WINDOW_W,
		WINDOW_H,
		SDL_WINDOW_SHOWN
	);
	c->r = SDL_CreateRenderer(c->w, -1, SDL_RENDERER_ACCELERATED);
	c->color = 0;
	c->last_sample = 0;
	c->curr = NULL;
	c->head = c->tail = NULL;
	return 1;
}

void
destroy_window(client_t *c){
	SDL_DestroyRenderer(c->r);
	SDL_DestroyWindow(c->w);
	SDL_Quit();
	clear_canvas(&c->head, &c->tail);
	if(c->curr) free_segment(c->curr);
}

void
clear_window(client_t *c){
	SDL_SetRenderDrawColor(c->r, 0xff, 0xff, 0xff, 0xff);
	SDL_RenderClear(c->r);
}

void
draw_line(client_t *c, int16_t xi, int16_t yi, int16_t xf, int16_t yf, uint8_t color){
	uint8_t r, g, b;

	r = colors[color][0];
	g = colors[color][1];
	b = colors[color][2];
	SDL_SetRenderDrawColor(c->r, r, g, b, 0xff);
	SDL_RenderDrawLine(c->r, xi - 1, yi, xf - 1, yf);
	SDL_RenderDrawLine(c->r, xi + 1, yi, xf + 1, yf);
	SDL_RenderDrawLine(c->r, xi, yi - 1, xf, yf - 1);
	SDL_RenderDrawLine(c->r, xi, yi + 1, xf, yf + 1);
	SDL_RenderDrawLine(c->r, xi, yi, xf, yf);
}

void
draw_segment(client_t *c, segment_t *segment){
	point_t *curr, *prev;

	prev = segment->head;
	for(curr = prev->next; curr; curr = curr->next){
		draw_line(c, prev->x, prev->y, curr->x, curr->y, segment->color);
		prev = curr;
	}
}

void
draw_colors(client_t *c){
	int i;
	SDL_Rect rect;

	for(i = 0; i < 8; i++){
		SDL_SetRenderDrawColor(c->r, colors[i][0], colors[i][1], colors[i][2], 0xff);
		rect.x = (WINDOW_W / 8) * i;
		rect.y = 0;
		rect.w = WINDOW_W / 8;
		rect.h = COLORSEL_H;
		SDL_RenderFillRect(c->r, &rect);
	}
	/* highlight currently selected color */
	SDL_SetRenderDrawColor(c->r, hl_color[0], hl_color[1], hl_color[2], 0xff);
	rect.x = (WINDOW_W / 8) * c->color;
	rect.y = COLORSEL_H - COLORSEL_HL_H;
	rect.w = WINDOW_W / 8;
	rect.h = COLORSEL_HL_H;
	SDL_RenderFillRect(c->r, &rect);
}

char
select_color(client_t *c, int16_t x, int16_t y){
	if(y >= COLORSEL_H) return 0;
	c->color = x / (WINDOW_W / 8);
	return 1;
}

void
redraw_window(client_t *c){
	segment_t *curr;

	clear_window(c);
	draw_colors(c);
	for(curr = c->head; curr; curr = curr->next) draw_segment(c, curr);
}

char
handle_event(client_t *c){
	SDL_Event e;
	int16_t xi, yi, xf, yf;

	if(!SDL_PollEvent(&e)) return 1;
	if(e.type == SDL_QUIT) return 0;

	/* either the start of a stroke, or a color selection */
	else if(e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT){
		if(!select_color(c, e.button.x, e.button.y)){
			c->curr = new_segment(e.button.x, e.button.y, c->color);
			c->last_sample = SDL_GetTicks();
		}
		else{
			draw_colors(c);
			SDL_RenderPresent(c->r);
		}
	}

	/* only relevant when a stroke is being finished, otherwise do nothing */
	else if(e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT){
		if(c->curr){
			add_segment(c->curr, &c->head, &c->tail);
			c->curr = NULL;
		}
	}

	/* continuation of a stroke */
	else if(e.type == SDL_MOUSEMOTION && e.motion.state & SDL_BUTTON_LMASK){
		if(c->curr){
			if(SDL_GetTicks() - c->last_sample >= SAMPLING_INTERVAL){
				c->last_sample = SDL_GetTicks();
				xi = c->curr->tail->x;
				yi = c->curr->tail->y;
				xf = e.motion.x;
				yf = e.motion.y;
				draw_line(c, xi, yi, xf, yf, c->color);
				SDL_RenderPresent(c->r);
				add_point(c->curr, xf, yf);
			}
		}
	}

	/* space key clears the canvas */
	else if(e.type == SDL_KEYDOWN && e.key.keysym.sym == ' '){
		clear_canvas(&c->head, &c->tail);
		redraw_window(c);
		SDL_RenderPresent(c->r);
	}

	/* window redisplay */
	else if(e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_EXPOSED){
		redraw_window(c);
		SDL_RenderPresent(c->r);
	}
	return 1;
}

int
main(void){
	client_t c;

	init_window(&c);
	while(handle_event(&c)) SDL_Delay(FRAME_DELAY);
	destroy_window(&c);
	return 0;
}
