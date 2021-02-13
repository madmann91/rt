#ifndef IO_PNG_IMAGE_H
#define IO_PNG_IMAGE_H

#include <stdbool.h>

struct image;

/*
 * These functions load and store images to PNG files.
 * Only images that have an R, G, B, and optionally A channel are accepted.
 */

struct image* load_png_image(const char* file_name);
bool save_png_image(const char* file_name, const struct image* image);

#endif
