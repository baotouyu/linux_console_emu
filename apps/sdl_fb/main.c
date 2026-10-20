#include <stdio.h>
#include <unistd.h>

#include <SDL.h>

int main(void)
{
    SDL_Surface *screen;
    char driver[32];
    Uint32 green;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    driver[0] = '\0';
    SDL_VideoDriverName(driver, sizeof(driver));

    screen = SDL_SetVideoMode(320, 240, 16, SDL_SWSURFACE);
    if (screen == NULL) {
        fprintf(stderr, "SDL_SetVideoMode: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    green = SDL_MapRGB(screen->format, 0, 180, 0);
    if (SDL_FillRect(screen, NULL, green) != 0) {
        fprintf(stderr, "SDL_FillRect: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_UpdateRect(screen, 0, 0, 0, 0);

    printf("sdl driver %s surface %dx%d pitch %d\n", driver, screen->w, screen->h, screen->pitch);
    fflush(stdout);
    sleep(5);

    SDL_Quit();
    return 0;
}
