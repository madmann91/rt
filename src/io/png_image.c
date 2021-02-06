#include <stdio.h>

#include "io/png_image.h"
#include "core/image.h"
#include "core/utils.h"

#include <png.h>

static struct image* load_png_image_from_file(FILE* fp) {
    char sig[8];
    fread(sig, 1, 8, fp);
    if (!png_check_sig((unsigned char*)sig, 8))
        return NULL;

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        return NULL;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    struct image* image = NULL;
    png_byte** row_ptrs = NULL;
    png_byte* row_bytes = NULL;
    if (setjmp(png_jmpbuf(png_ptr))) {
        if (image)     free_image(image);
        if (row_ptrs)  free(row_ptrs);
        if (row_bytes) free(row_bytes);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    png_set_sig_bytes(png_ptr, 8);
    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    size_t width  = png_get_image_width(png_ptr, info_ptr);
    size_t height = png_get_image_height(png_ptr, info_ptr);

    png_uint_32 color_type = png_get_color_type(png_ptr, info_ptr);
    png_uint_32 bit_depth  = png_get_bit_depth(png_ptr, info_ptr);

    // Expand paletted and grayscale images to RGB
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    else if (
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    // Transform to 8 bit per channel
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    // Get alpha channel when there is one
    bool has_alpha = color_type & PNG_COLOR_MASK_ALPHA;
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
        has_alpha = true;
    }

    size_t channel_count = has_alpha ? 4 : 3;
    size_t stride = width * channel_count;
    row_ptrs  = xmalloc(sizeof(png_byte*) * height);
    row_bytes = xmalloc(sizeof(png_byte) * stride * height);
    for (size_t i = 0; i < height; ++i)
        row_ptrs[i] = row_bytes + stride * i;
    png_read_image(png_ptr, row_ptrs);

    // Transform the byte data into a floating point image
    image = new_image(width, height, channel_count);
    for (size_t i = 0; i < height; ++i) {
        const png_byte* row = row_bytes + stride * i;
        const real_t scale = (real_t)1 / (real_t)255;
        if (has_alpha) {
            for (size_t j = 0; j < width; ++j) {
                set_rgba_pixel(image, j, i, &(struct rgba) {
                    .r = (real_t)row[j * 3 + 0] * scale,
                    .g = (real_t)row[j * 3 + 1] * scale,
                    .b = (real_t)row[j * 3 + 2] * scale,
                });
            }
        } else {
            for (size_t j = 0; j < width; ++j) {
                set_rgba_pixel(image, j, i, &(struct rgba) {
                    .r = (real_t)row[j * 4 + 0] * scale,
                    .g = (real_t)row[j * 4 + 1] * scale,
                    .b = (real_t)row[j * 4 + 2] * scale,
                    .a = (real_t)row[j * 4 + 3] * scale
                });
            }
        }
    }
    free(row_ptrs);
    free(row_bytes);
    row_ptrs = NULL;
    row_bytes = NULL;

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return image;
}

struct image* load_png_image(const char* file_name) {
    FILE* fp = fopen(file_name, "rb");
    if (!fp)
        return NULL;
    struct image* image = load_png_image_from_file(fp);
    fclose(fp);
    return image;
}

static bool save_png_image_to_file(FILE* fp, const struct image* image) {
    assert(is_rgb_or_rgba_image(image));

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        return false;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return false;
    }

    png_byte** row_ptrs = NULL;
    png_byte* row_bytes = NULL;
    if (setjmp(png_jmpbuf(png_ptr))) {
        if (row_ptrs)  free(row_ptrs);
        if (row_bytes) free(row_bytes);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(
        png_ptr, info_ptr,
        image->width, image->height, 8,
        image->channel_count == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);

    bool has_alpha = image->channel_count >= 4;
    size_t stride = image->width * image->channel_count;
    row_ptrs  = xmalloc(sizeof(png_byte*) * image->height);
    row_bytes = xmalloc(sizeof(png_byte) * stride * image->height);
    for (size_t i = 0, n = image->height; i < n; ++i) {
        row_ptrs[i] = row_bytes + stride * i;
        png_byte* row = row_bytes + stride * i;
        if (has_alpha) {
            for (size_t j = 0, m = image->width; j < m; ++j) {
                struct rgba pixel = get_rgba_pixel(image, j, i);
                row[j * 4 + 0] = clamp_real(pixel.r, 0, 1) * (real_t)255;
                row[j * 4 + 1] = clamp_real(pixel.g, 0, 1) * (real_t)255;
                row[j * 4 + 2] = clamp_real(pixel.b, 0, 1) * (real_t)255;
                row[j * 4 + 3] = clamp_real(pixel.a, 0, 1) * (real_t)255;
            }
        } else {
            for (size_t j = 0, m = image->width; j < m; ++j) {
                struct rgb pixel = get_rgb_pixel(image, j, i);
                row[j * 3 + 0] = clamp_real(pixel.r, 0, 1) * (real_t)255;
                row[j * 3 + 1] = clamp_real(pixel.g, 0, 1) * (real_t)255;
                row[j * 3 + 2] = clamp_real(pixel.b, 0, 1) * (real_t)255;
            }
        }
    }
    png_write_image(png_ptr, row_ptrs);
    free(row_ptrs);
    free(row_bytes);
    row_ptrs = NULL;
    row_bytes = NULL;

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return true;
}

bool save_png_image(const char* file_name, const struct image* image) {
    if (!is_rgb_or_rgba_image(image))
        return false;
    FILE* fp = fopen(file_name, "wb");
    if (!fp)
        return false;
    bool ok = save_png_image_to_file(fp, image);
    fclose(fp);
    return ok;
}
