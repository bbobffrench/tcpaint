#include "client.h"

#include <SDL2/SDL.h>

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#define WINDOW_W 800
#define WINDOW_H 800

#define COLORSEL_H 50
#define COLORSEL_HL_H 10

#define SAMPLING_INTERVAL 50
#define FRAME_DELAY 4

const uint8_t colors[][3] = {
	{(uint8_t)0x00, (uint8_t)0x00, (uint8_t)0x00},
	{(uint8_t)0xff, (uint8_t)0x00, (uint8_t)0x00},
	{(uint8_t)0x00, (uint8_t)0xff, (uint8_t)0x00},
	{(uint8_t)0xff, (uint8_t)0xaf, (uint8_t)0x00},
	{(uint8_t)0x00, (uint8_t)0x00, (uint8_t)0xff},
	{(uint8_t)0xff, (uint8_t)0x00, (uint8_t)0xff},
	{(uint8_t)0x00, (uint8_t)0xff, (uint8_t)0xff},
	{(uint8_t)0x96, (uint8_t)0x4b, (uint8_t)0x00}
};

const uint8_t hl_color[] = {(uint8_t)0x89, (uint8_t)0x89, (uint8_t)0x89};

const uint8_t bg_color[] = {(uint8_t)0xff, (uint8_t)0xff, (uint8_t)0xff};

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

static void
clear_window(SDL_Renderer *r){
	SDL_SetRenderDrawColor(r, bg_color[0], bg_color[1], bg_color[2], 0xff);
	SDL_RenderClear(r);
}

static void
draw_line(SDL_Renderer *r, int16_t xi, int16_t yi, int16_t xf, int16_t yf, uint8_t color){
	if(yi < COLORSEL_H && yf < COLORSEL_H) return;
	if(yi < COLORSEL_H) yi = COLORSEL_H;
	if(yf < COLORSEL_H) yf = COLORSEL_H;

	SDL_SetRenderDrawColor(r, colors[color][0], colors[color][1], colors[color][2], 0xff);
	SDL_RenderDrawLine(r, xi - 1, yi, xf - 1, yf);
	SDL_RenderDrawLine(r, xi + 1, yi, xf + 1, yf);
	SDL_RenderDrawLine(r, xi, yi - 1, xf, yf - 1);
	SDL_RenderDrawLine(r, xi, yi + 1, xf, yf + 1);
	SDL_RenderDrawLine(r, xi, yi, xf, yf);
}

static void
draw_segment(SDL_Renderer *r, segment_t *segment){
	point_t *curr, *prev;

	prev = segment->head;
	for(curr = prev->next; curr; curr = curr->next){
		draw_line(r, prev->x, prev->y, curr->x, curr->y, segment->color);
		prev = curr;
	}
}

static void
draw_colors(client_t *c, SDL_Renderer *r){
	int i;
	SDL_Rect rect;

	for(i = 0; i < 8; i++){
		SDL_SetRenderDrawColor(r, colors[i][0], colors[i][1], colors[i][2], 0xff);
		rect.x = (WINDOW_W / 8) * i;
		rect.y = 0;
		rect.w = WINDOW_W / 8;
		rect.h = COLORSEL_H;
		SDL_RenderFillRect(r, &rect);
	}
	SDL_SetRenderDrawColor(r, hl_color[0], hl_color[1], hl_color[2], 0xff);
	rect.x = (WINDOW_W / 8) * c->color;
	rect.y = COLORSEL_H - COLORSEL_HL_H;
	rect.w = WINDOW_W / 8;
	rect.h = COLORSEL_HL_H;
	SDL_RenderFillRect(r, &rect);
}

static char
select_color(client_t *c, int16_t x, int16_t y){
	if(y >= COLORSEL_H) return 0;
	c->color = x / (WINDOW_W / 8);
	return 1;
}

static void
redraw_window(client_t *c, SDL_Renderer *r){
	segment_t *curr;

	clear_window(r);
	draw_colors(c, r);
	for(curr = c->head; curr; curr = curr->next) draw_segment(r, curr);
}

static char
handle_event(client_t *c, SDL_Renderer *r){
	SDL_Event e;
	int16_t xi, yi, xf, yf;
	static uint32_t last_sample;

	if(!SDL_PollEvent(&e)) return 1;
	if(e.type == SDL_QUIT) return 0;

	else if(e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT){
		if(!select_color(c, e.button.x, e.button.y)){
			c->curr = new_segment(e.button.x, e.button.y, c->color);
			last_sample = SDL_GetTicks();
		}
		else{
			draw_colors(c, r);
			SDL_RenderPresent(r);
		}
	}
	else if(e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT){
		if(c->curr){
			add_segment(c->curr, &c->head, &c->tail);
			last_sample = 0;
			c->curr = NULL;
		}
	}
	else if(e.type == SDL_MOUSEMOTION && e.motion.state & SDL_BUTTON_LMASK){
		if(c->curr){
			if(SDL_GetTicks() - last_sample >= SAMPLING_INTERVAL){
				last_sample = SDL_GetTicks();
				xi = c->curr->tail->x;
				yi = c->curr->tail->y;
				xf = e.motion.x;
				yf = e.motion.y;
				draw_line(r, xi, yi, xf, yf, c->color);
				SDL_RenderPresent(r);
				add_point(c->curr, xf, yf);
			}
		}
	}
	else if(e.type == SDL_KEYDOWN && e.key.keysym.sym == ' '){
		clear_canvas(&c->head, &c->tail);
		redraw_window(c, r);
		SDL_RenderPresent(r);
	}
	else if(e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_EXPOSED){
		redraw_window(c, r);
		SDL_RenderPresent(r);
	}
	return 1;
}

int
main(void){
	client_t *c = NULL;
	SDL_Window *win;
	SDL_Renderer *r;

	if(SDL_Init(SDL_INIT_VIDEO)){
		fprintf(stderr, "SDL2 failed to initialize\n");
		return 1;
	}
	win = SDL_CreateWindow(
		"tcpaint",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		WINDOW_W,
		WINDOW_H,
		SDL_WINDOW_SHOWN
	);
	r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

	add_client(&c, -1, 0);

	while(handle_event(c, r)) SDL_Delay(FRAME_DELAY);

	SDL_DestroyRenderer(r);
	SDL_DestroyWindow(win);
	SDL_Quit();
	free_clients(c);
	return 0;
}
