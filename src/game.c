#include <time.h>
#include <stdint.h>

#include <SDL_main.h>
#include <SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "res/times24.png.h"
#include "src/game.h"

#define NPOS -1
#define MAX_LEVEL 9
#define MAX_GRID (MAX_LEVEL * (MAX_LEVEL + 1) / 2)

#define GRID_OUTER_PAD_X 12
#define GRID_OUTER_PAD_Y 12
#define GRID_INNER_PAD 6
#define GRID_INSET_PAD 4

#define MENUITEM_WIDTH 120
#define MENUITEM_PAD_Y 10

#define WINBUTTON_WIDTH 160
#define WINBUTTON_OFS_Y 24

enum state {
	STATE_MENU,
	STATE_GAME,
	STATE_WON
};

enum difficulty {
	EASY = 4,
	MEDIUM = 6,
	HARD = 9
};

static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_fontatlas = NULL;
static int s_width = 640;
static int s_height = 480;

static int s_hover = NPOS;
static uint8_t s_grid[MAX_GRID] = { 0 };
static uint8_t s_chain[(MAX_GRID + 7) >> 3] = { 0 };
static uint8_t s_found[(MAX_GRID + 7) >> 3] = { 0 };

static int s_incorrect = 0;
static uint64_t s_starttime = 0;
static uint64_t s_endtime = 0;

static int s_count = 0;
static int s_rows = 0;

static int s_state = STATE_MENU;

int ginit()
{
	srand(time(NULL));

	if(SDL_Init(SDL_INIT_VIDEO) != 0) {
		return 0;
	}

	s_window = SDL_CreateWindow(
		"Memory Game",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		s_width,
		s_height,
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	if(s_window == NULL) {
		return 0;
	}

	s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED);

	if(s_renderer == NULL) {
		return 0;
	}

	int w, h, nchan;
	unsigned char *data = stbi_load_from_memory(times24_data, sizeof(times24_data), &w, &h, &nchan, 0);
	if(data == NULL || nchan != 1) {
		return 0;
	}

	unsigned char *rgba = malloc(w * h * 4);
	if(rgba == NULL) {
		return 0;
	}

	for(size_t i = 0; i < w * h; i++) {
		rgba[i * 4 + 0] = 0xFF;
		rgba[i * 4 + 1] = 0xFF;
		rgba[i * 4 + 2] = 0xFF;
		rgba[i * 4 + 3] = data[i];
	}

	s_fontatlas = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);

	SDL_UpdateTexture(s_fontatlas, NULL, rgba, w * 4);
	SDL_SetTextureBlendMode(s_fontatlas, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);

	free(rgba);
	stbi_image_free(data);

	return 1;
}

static void setuprows()
{
	double rows = sqrtl(s_count);
	s_rows = (int)floor(rows);

	if(s_width > s_height) {
		while(s_count % s_rows != 0) {
			s_rows++;
		}
	}
	else {
		while(s_count % s_rows != 0) {
			s_rows--;
		}
	}
}

static void setupgrid(int level)
{
	s_count = 0;
	s_hover = NPOS;
	s_starttime = SDL_GetTicks64();
	s_state = STATE_GAME;

	memset(s_grid, 0, sizeof(s_grid));
	memset(s_chain, 0, sizeof(s_chain));
	memset(s_found, 0, sizeof(s_found));

	for(int i = 1; i <= level; i++) {
		for(int j = 0; j < i; j++) {
			s_grid[s_count++] = i;
		}
	}

	for(int i = s_count - 1; i > 0; i--) {
		int j = rand() % (i + 1);
		int tmp = s_grid[i];
		s_grid[i] = s_grid[j];
		s_grid[j] = tmp;
	}

	setuprows();
}

static int gridpos(int x, int y)
{
	int w = s_width - (GRID_OUTER_PAD_X * 2);
	int h = s_height - (GRID_OUTER_PAD_Y * 2);

	int cols = s_count / s_rows;
	int cell_w = w / s_rows;
	int cell_h = h / cols;

	int cell_size = cell_w;
	if(cell_size > cell_h) {
		cell_size = cell_h;
	}

	int xofs = GRID_OUTER_PAD_X + (w - cell_size * s_rows) / 2;
	int yofs = GRID_OUTER_PAD_Y + (h - cell_size * cols) / 2;

	for(int i = 0; i < s_count; i++) {
		int dx = xofs + (i % s_rows) * cell_size;
		int dy = yofs + (i / s_rows) * cell_size;

		SDL_Rect dst = { dx + GRID_INNER_PAD,
				 dy + GRID_INNER_PAD,
				 cell_size - GRID_INNER_PAD * 2,
				 cell_size - GRID_INNER_PAD * 2 };

		if(x >= dst.x && y >= dst.y && x <= dst.x + dst.w && y <= dst.y + dst.h) {
			return i;
		}
	}

	return NPOS;
}

static int badchain()
{
	int g = NPOS;
	for(int i = 0; i < s_count; i++) {
		if(s_chain[i >> 3] & (1 << (i & 7))) {
			if(g == NPOS) {
				g = i;
			} else if(s_grid[i] != s_grid[g]) {
				return 1;
			}
		}
	}

	return g == NPOS;
}

static int menuitem(int mx, int my)
{
	int h = 24 + MENUITEM_PAD_Y;
	int dy = 48;

	int w = MENUITEM_WIDTH;
	int x = (s_width - w) / 2;
	int y = 24;

	y += dy;
	if(mx >= x && my >= y && mx <= x + w && my <= y + h) {
		return EASY;
	}

	y += dy;
	if(mx >= x && my >= y && mx <= x + w && my <= y + h) {
		return MEDIUM;
	}

	y += dy;
	if(mx >= x && my >= y && mx <= x + w && my <= y + h) {
		return HARD;
	}

	return NPOS;
}

static void menuevent(const SDL_Event *e)
{
	int x, y;
	int difficulty;

	switch(e->type) {
	case SDL_MOUSEBUTTONDOWN:
		x = e->button.x;
		y = e->button.y;
		difficulty = menuitem(x, y);
		if(difficulty != NPOS) {
			setupgrid(difficulty);
		}
		break;
	case SDL_MOUSEMOTION:
		x = e->motion.x;
		y = e->motion.y;
		s_hover = menuitem(x, y);
		break;
	default:
		break;
	}
}

static int winbutton(int mx, int my)
{
	int h = 24;
	int dy = 32;
	int y = 24;

	y += dy;
	y += dy;

	int w = WINBUTTON_WIDTH;
	int x = (s_width - w) / 2;
	y += dy + WINBUTTON_OFS_Y;
	h += MENUITEM_PAD_Y;

	if(mx >= x && my >= y && mx <= x + w && my <= y + h) {
		return 1;
	} else {
		return NPOS;
	}
}

static void winevent(const SDL_Event *e)
{
	int x, y;
	switch(e->type) {
	case SDL_MOUSEBUTTONDOWN:
		x = e->button.x;
		y = e->button.y;
		if(winbutton(x, y) != NPOS) {
			s_state = STATE_MENU;
		}
		break;
	case SDL_MOUSEMOTION:
		x = e->motion.x;
		y = e->motion.y;
		s_hover = winbutton(x, y);
		break;
	default:
		break;
	}
}

static void gameevent(const SDL_Event *e)
{
	int x, y;
	int gpos, count = 0;

	switch(e->type) {
	case SDL_WINDOWEVENT:
		if(e->window.event == SDL_WINDOWEVENT_RESIZED) {
			setuprows();
		}
		break;
	case SDL_MOUSEBUTTONUP:
		gpos = NPOS;
		count = 0;
		for(int i = 0; i < s_count; i++) {
			if(s_chain[i >> 3] & (1 << (i & 7))) {
				if(gpos == NPOS) {
					gpos = i;
				}
				else if(s_grid[i] != s_grid[gpos]) {
					memset(s_chain, 0, sizeof(s_chain));
					s_incorrect++;
					break;
				}
				count++;
			}
		}

		if(gpos != NPOS) {
			if(count == s_grid[gpos]) {
				memset(s_chain, 0, sizeof(s_chain));
				int won = 1;
				for(int i = 0; i < s_count; i++) {
					if(s_grid[i] == s_grid[gpos]) {
						s_found[i >> 3] |= 1 << (i & 7);
					}
					if((s_found[i >> 3] & (1 << (i & 7))) == 0) {
						won = 0;
					}
				}
				if(won) {
					s_endtime = SDL_GetTicks64();
					s_state = STATE_WON;
					s_hover = NPOS;
				}
			}
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		x = e->button.x;
		y = e->button.y;
		gpos = gridpos(x, y);
		if(gpos != NPOS) {
			if((s_found[gpos >> 3] & (1 << (gpos & 7))) == 0) {
				s_chain[gpos >> 3] |= 1 << (gpos & 7);
			}
		}
		break;
	case SDL_MOUSEMOTION:
		x = e->motion.x;
		y = e->motion.y;
		s_hover = gridpos(x, y);
		break;
	default:
		break;
	}
}

void gevent(const SDL_Event *e)
{
	if(e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_RESIZED) {
		s_width = e->window.data1;
		s_height = e->window.data2;
	}

	switch(s_state) {
	case STATE_GAME:
		gameevent(e);
		break;
	case STATE_MENU:
		menuevent(e);
		break;
	case STATE_WON:
		winevent(e);
		break;
	}
}


static int strwidth(const char *s)
{
	int w = 0;
	while(*s) {
		w += times24_pc[*s].xadvance;
		s++;
	}
	return w;
}


static int drawstr(int x, int y, const char *s)
{
	int dx = x;
	int dy = y;

	while(*s) {
		struct times24_pos p = times24_pc[*s];
		SDL_Rect src = { p.x, p.y, p.w, p.h };
		SDL_Rect dst = { dx + p.left, (dy - p.top) + 24, p.w, p.h };
		// shadow
		dst.x += 1; dst.y += 1;
		SDL_SetTextureColorMod(s_fontatlas, 0x00, 0x00, 0x00);
		SDL_RenderCopy(s_renderer, s_fontatlas, &src, &dst);
		// foreground
		dst.x -= 1; dst.y -= 1;
		SDL_SetTextureColorMod(s_fontatlas, 0xFF, 0xFF, 0xFF);
		SDL_RenderCopy(s_renderer, s_fontatlas, &src, &dst);

		dx += p.xadvance;
		s++;
	}

	return dx - x;
}


static void drawinset(int x, int y, int w, int h)
{
	SDL_Color shade = { 0x00, 0x00, 0x00, 0x77 };
	SDL_Color highlight = { 0xFF, 0xFF, 0xFF, 0x77 };

	SDL_Vertex vtx_inset[] = {
		{{ x, y }, shade, { 0, 0 }}, // 1
		{{ x + w, y }, shade, { 0, 0 }}, // 2
		{{ x + w - GRID_INSET_PAD, y + GRID_INSET_PAD }, shade, { 0, 0 }}, // 3
		{{ x + GRID_INSET_PAD, y + GRID_INSET_PAD }, shade, { 0, 0 }}, // 4
		{{ x + GRID_INSET_PAD, y + h - GRID_INSET_PAD }, shade, { 0, 0 }}, // 5
		{{ x , y + h }, shade, { 0, 0 }}, // 6

		{{ x + GRID_INSET_PAD, y + h - GRID_INSET_PAD }, highlight, { 0, 0 }}, // 7
		{{ x + w, y + h }, highlight, { 0, 0 }}, // 8
		{{ x, y + h }, highlight, { 0, 0 }}, // 9
		{{ x + w - GRID_INSET_PAD, y + h - GRID_INSET_PAD }, highlight, { 0, 0 }}, // 10
		{{ x + w - GRID_INSET_PAD, y + GRID_INSET_PAD }, highlight, { 0, 0 }}, // 11
		{{ x + w, y }, highlight, { 0, 0 }}, // 12
	};

	int indices[] = {
		0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, // shade
		6, 7, 8, 6, 9, 7, 9, 10, 7, 10, 11, 7 // highlight
	};

	SDL_Rect dst = { x, y, w, h };
	SDL_RenderFillRect(s_renderer, &dst);

	SDL_RenderGeometry(s_renderer, NULL, vtx_inset, 12, indices, 24);

	dst.x += GRID_INSET_PAD;
	dst.y += GRID_INSET_PAD;
	dst.w -= GRID_INSET_PAD * 2;
	dst.h -= GRID_INSET_PAD * 2;

	SDL_RenderFillRect(s_renderer, &dst);
}

static void drawoutset(int x, int y, int w, int h)
{
	SDL_Color shade = { 0x00, 0x00, 0x00, 0x77 };
	SDL_Color highlight = { 0xFF, 0xFF, 0xFF, 0x77 };

	SDL_Vertex vtx_inset[] = {
		{{ x, y }, highlight, { 0, 0 }}, // 1
		{{ x + w, y }, highlight, { 0, 0 }}, // 2
		{{ x + w - GRID_INSET_PAD, y + GRID_INSET_PAD }, highlight, { 0, 0 }}, // 3
		{{ x + GRID_INSET_PAD, y + GRID_INSET_PAD }, highlight, { 0, 0 }}, // 4
		{{ x + GRID_INSET_PAD, y + h - GRID_INSET_PAD }, highlight, { 0, 0 }}, // 5
		{{ x , y + h }, highlight, { 0, 0 }}, // 6

		{{ x + GRID_INSET_PAD, y + h - GRID_INSET_PAD }, shade, { 0, 0 }}, // 7
		{{ x + w, y + h }, shade, { 0, 0 }}, // 8
		{{ x, y + h }, shade, { 0, 0 }}, // 9
		{{ x + w - GRID_INSET_PAD, y + h - GRID_INSET_PAD }, shade, { 0, 0 }}, // 10
		{{ x + w - GRID_INSET_PAD, y + GRID_INSET_PAD }, shade, { 0, 0 }}, // 11
		{{ x + w, y }, shade, { 0, 0 }}, // 12
	};

	int indices[] = {
		0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, // hightlight
		6, 7, 8, 6, 9, 7, 9, 10, 7, 10, 11, 7 // shade
	};

	SDL_Rect dst = { x, y, w, h };
	SDL_RenderFillRect(s_renderer, &dst);

	SDL_RenderGeometry(s_renderer, NULL, vtx_inset, 12, indices, 24);

	dst.x += GRID_INSET_PAD;
	dst.y += GRID_INSET_PAD;
	dst.w -= GRID_INSET_PAD * 2;
	dst.h -= GRID_INSET_PAD * 2;

	SDL_RenderFillRect(s_renderer, &dst);
}

static void drawwin()
{
	char buf[64];
	const char msg[] = "You win!";
	int w = strwidth(msg);
	int h = 24;
	int dy = 32;
	int x = (s_width - w) / 2;
	int y = 24;
	drawstr(x, y, msg);

	double time = (double)(s_endtime - s_starttime) / 1000.0;

	if(time > 60.0) {
		snprintf(buf, sizeof(buf), "Time taken: %.0lfm %.0lfs", time / 60.0, fmod(time, 60.0));
	} else {
		snprintf(buf, sizeof(buf), "Time taken: %.0lfs", time);
	}
	y += dy;
	w = strwidth(buf);
	x = (s_width - w) / 2;
	drawstr(x, y, buf);

	snprintf(buf, sizeof(buf), "Incorrect guesses: %d", s_incorrect);
	y += dy;
	w = strwidth(buf);
	x = (s_width - w) / 2;
	drawstr(x, y, buf);

	if(s_hover != NPOS) {
		SDL_SetRenderDrawColor(s_renderer, 0x77, 0x77, 0x77, 0xFF);
	} else {
		SDL_SetRenderDrawColor(s_renderer, 0x55, 0x55, 0x55, 0xFF);
	}

	w = WINBUTTON_WIDTH;
	x = (s_width - w) / 2;
	y += dy + WINBUTTON_OFS_Y;
	drawoutset(x, y, w, h + MENUITEM_PAD_Y);
	const char go_back[] = "Play Again?";
	w = strwidth(go_back);
	x = (s_width - w) / 2;
	drawstr(x, y, go_back);
}

static void drawmenu()
{
	const char msg[] = "Select difficulty:";
	int w = strwidth(msg);
	int h = 24;
	int dy = 48;

	int x = (s_width - w) / 2;
	int y = 24;
	drawstr(x, y, msg);

	if(s_hover == EASY) {
		SDL_SetRenderDrawColor(s_renderer, 0x77, 0x77, 0x77, 0xFF);
	}
	else {
		SDL_SetRenderDrawColor(s_renderer, 0x55, 0x55, 0x55, 0xFF);
	}

	w = MENUITEM_WIDTH;
	x = (s_width - w) / 2;
	y += dy;
	drawoutset(x, y, w, h + MENUITEM_PAD_Y);

	const char easy[] = "Easy";
	w = strwidth(easy);
	x = (s_width - w) / 2;
	drawstr(x, y, easy);

	if(s_hover == MEDIUM) {
		SDL_SetRenderDrawColor(s_renderer, 0x77, 0x77, 0x77, 0xFF);
	}
	else {
		SDL_SetRenderDrawColor(s_renderer, 0x55, 0x55, 0x55, 0xFF);
	}

	w = MENUITEM_WIDTH;
	x = (s_width - w) / 2;
	y += dy;
	drawoutset(x, y, w, h + MENUITEM_PAD_Y);

	const char medium[] = "Medium";
	w = strwidth(medium);
	x = (s_width - w) / 2;
	drawstr(x, y, medium);

	if(s_hover == HARD) {
		SDL_SetRenderDrawColor(s_renderer, 0x77, 0x77, 0x77, 0xFF);
	}
	else {
		SDL_SetRenderDrawColor(s_renderer, 0x55, 0x55, 0x55, 0xFF);
	}

	w = MENUITEM_WIDTH;
	x = (s_width - w) / 2;
	y += dy;
	drawoutset(x, y, w, h + MENUITEM_PAD_Y);

	const char hard[] = "Hard";
	w = strwidth(hard);
	x = (s_width - w) / 2;
	drawstr(x, y, hard);
}

static void drawgrid()
{
	int w = s_width - (GRID_OUTER_PAD_X * 2);
	int h = s_height - (GRID_OUTER_PAD_Y * 2);

	int cols = s_count / s_rows;
	int cell_w = w / s_rows;
	int cell_h = h / cols;

	int cell_size = cell_w;
	if(cell_size > cell_h) {
		cell_size = cell_h;
	}

	int xofs = GRID_OUTER_PAD_X + (w - cell_size * s_rows) / 2;
	int yofs = GRID_OUTER_PAD_Y + (h - cell_size * cols) / 2;

	for(int i = 0; i < s_count; i++) {
		int x = xofs + (i % s_rows) * cell_size;
		int y = yofs + (i / s_rows) * cell_size;

		SDL_Rect dst = { x + GRID_INNER_PAD,
				 y + GRID_INNER_PAD,
				 cell_size - GRID_INNER_PAD * 2,
				 cell_size - GRID_INNER_PAD * 2 };

		int found = s_found[i >> 3] & (1 << (i & 7));
		int chain = s_chain[i >> 3] & (1 << (i & 7));

		if(found) {
			SDL_SetRenderDrawColor(s_renderer, 0x10, 0xAA, 0x10, 0xFF);
		}
		else if(badchain() && chain) {
			SDL_SetRenderDrawColor(s_renderer, 0xFF, 0x00, 0x00, 0xFF);
		}
		else if(i == s_hover && !chain) {
			SDL_SetRenderDrawColor(s_renderer, 0x77, 0x77, 0x77, 0xFF);
		}
		else {
			SDL_SetRenderDrawColor(s_renderer, 0x55, 0x55, 0x55, 0xFF);
		}

		if(chain || found) {
			drawinset(dst.x, dst.y, dst.w, dst.h);
		}
		else {
			drawoutset(dst.x, dst.y, dst.w, dst.h);
		}

		if(chain || found) {
			struct times24_pos p = times24_pc['0' + s_grid[i]];
			SDL_Rect src = { p.x, p.y, p.w, p.h };

			dst.x += (dst.w - p.w) / 2;
			dst.y += (dst.h - p.h) / 2;
			dst.w = p.w;
			dst.h = p.h;

			// shadow
			dst.x += 1; dst.y += 1;
			SDL_SetTextureColorMod(s_fontatlas, 0x00, 0x00, 0x00);
			SDL_RenderCopy(s_renderer, s_fontatlas, &src, &dst);
			// foreground
			dst.x -= 1; dst.y -= 1;
			SDL_SetTextureColorMod(s_fontatlas, 0xFF, 0xFF, 0xFF);
			SDL_RenderCopy(s_renderer, s_fontatlas, &src, &dst);
		}
	}
}


void gdraw()
{
	SDL_SetRenderDrawColor(s_renderer, 0x77, 0x77, 0x77, 0xFF);
	SDL_RenderClear(s_renderer);

	switch(s_state)
	{
	case STATE_MENU:
		drawmenu();
		break;
	case STATE_GAME:
		drawgrid();
		break;
	case STATE_WON:
		drawwin();
		break;
	}

	SDL_RenderPresent(s_renderer);
}

void gfree()
{
	SDL_DestroyTexture(s_fontatlas);
	SDL_DestroyRenderer(s_renderer);
	SDL_DestroyWindow(s_window);
	SDL_Quit();
}