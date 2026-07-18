// Single translation unit that compiles the stb_image implementation.
// (Do NOT define STBI_NO_STDIO — stb checks definedness, and the file-based
//  stbi_load used by the console test needs the stdio path compiled in.)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
