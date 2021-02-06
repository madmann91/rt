#ifndef IO_PNG_H
#define IO_PNG_H

#include <stdbool.h>

struct image;

struct image* load_png_image(const char* file_name);

/* Stores an image into a 24-bit PNG file.
 * A value of 1.0 in the R, G, or B channel is mapped to a byte value of 255.
 */
bool save_png_image(const char* file_name, const struct image* image);

#endif
