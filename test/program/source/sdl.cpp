#include "sdl.hpp"

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_timer.h"
#include "SDL3/SDL_video.h"

#include <cstdlib>
#include <iostream>

void create_and_destroy_window()
{
  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
    exit(EXIT_FAILURE);
  }

  SDL_Window *window = SDL_CreateWindow("Hello SDL3", 1280, 720, SDL_WINDOW_HIDDEN);
  if (!window)
  {
    std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
    SDL_Quit();
    exit(EXIT_FAILURE);
  }
  SDL_ShowWindow(window);
  SDL_Delay(3000);
  SDL_DestroyWindow(window);

  SDL_Quit();
}
