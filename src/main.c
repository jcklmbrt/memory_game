#include <stdlib.h>
#include <stdio.h>

#include <SDL_main.h>
#include <SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "src/game.h"

static int s_quit = 0;

void loop()
{
	SDL_Event e;
	if(SDL_PollEvent(&e)) {
		gevent(&e);
		switch(e.type) {
		case SDL_QUIT:
			s_quit = 1;
#ifdef __EMSCRIPTEN__
			emscripten_cancel_main_loop();
#endif
			break;
		default:
			break;
		}
	}
	gdraw();
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	if(!ginit()) {
		gfree();
		return EXIT_FAILURE;
	}
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(loop, 0, 1);
#else
	while(s_quit == 0) {
		loop();
	}
#endif
	gfree();
	return EXIT_SUCCESS;
}