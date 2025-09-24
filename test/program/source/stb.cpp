#include "stb.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void vertical_flip() { stbi_set_flip_vertically_on_load(true); }
