#ifndef _GCONTEXT_HPP
#define _GCONTEXT_HPP

int ginit();
void gfree();
void gdraw();
void gevent(const union SDL_Event *e);

#endif