#include <stdio.h>

#include <SDL.h>

int main(void)
{
    SDL_Surface *screen;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    screen = SDL_SetVideoMode(320, 240, 16, SDL_SWSURFACE);
    if (screen == NULL) {
        fprintf(stderr, "SDL_SetVideoMode: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    printf("sdl framebuffer %dx%d pitch %d\n", screen->w, screen->h, screen->pitch);
    SDL_Quit();
    return 0;
}
