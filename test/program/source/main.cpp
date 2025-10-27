#include <cstdlib>
#include <iostream>

#include "SDL3/SDL_main.h"

#include "sdl.hpp"
#include "stb.hpp"

int main(int argc, char *argv[])
{
  if (argc != 1 || argv == nullptr)
  {
    std::cerr << "Unexpected command line arguments." << std::endl;
    return EXIT_FAILURE;
  }

  vertical_flip();
  create_and_destroy_window();

  return EXIT_SUCCESS;
}
