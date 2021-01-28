#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#include "core/thread_pool.h"
#include "core/utils.h"

struct global_data {
    double x_min, x_max;
    double y_min, y_max;
    int count_max;
    int n, m;
    uint32_t* pixels;
};

struct tile {
    int i, j, n, m;
};

static inline uint32_t encode_pixel(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r) | (((uint32_t)g) << 8) | (((uint32_t)b) << 16);
}

static inline void decode_pixel(uint32_t pixel, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = pixel;
    *g = pixel >> 8;
    *b = pixel >> 16;
}

static void write_ppm(const char* file_name, uint32_t* pixels, int n, int m) {
    FILE* fp = fopen(file_name, "w");

    fprintf(fp, "P3\n");
    fprintf(fp, "%d %d\n", n, m);
    fprintf(fp, "%d\n", 255);

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; ++j) {
            uint8_t r, g, b;
            decode_pixel(pixels[i * n + j], &r, &g, &b);
            fprintf(fp, "%"PRIu8" %"PRIu8" %"PRIu8" ", r, g, b);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
}

static void render_tile(const struct tile* tile, const struct global_data* data) {
    int m = tile->m, n = tile->n;
#ifdef USE_OPENMP
#pragma omp parallel for collapse(2) schedule(dynamic)
#endif
    for (int i = tile->i; i < m; i++) {
        for (int j = tile->j; j < n; j++) {
            double y = ((i - 1) * data->y_max + (data->m - i) * data->y_min) / (data->m - 1);
            double x = ((j - 1 ) * data->x_max + (data->n - j) * data->x_min) / (data->n - 1);

            double x1 = x;
            double y1 = y;
            int k;
            for (k = 1; k <= data->count_max; k++) {
                double x2 = x1 * x1 - y1 * y1 + x;
                double y2 = 2 * x1 * y1 + y;

                if (x2 < -2.0 || 2.0 < x2 || y2 < -2.0 || 2.0 < y2)
                    break;

                x1 = x2;
                y1 = y2;
            }

            uint8_t r, g, b;
            if (k % 2 == 1) {
                r = g = b = 255;
            } else {
                uint8_t c = 255.0 * pow((double)k / (double)data->count_max, 0.125);
                r = 3 * c / 5;
                g = 3 * c / 5;
                b = c;
            }
            data->pixels[i * data->n + j] = encode_pixel(r, g, b);
        }
    }
}

#ifndef USE_OPENMP
struct render_job {
    struct work_item work_item;
    struct tile tile;
    struct global_data* global_data;
};

static void render_job(struct work_item* item) {
    struct render_job* job = (void*)item;
    render_tile(&job->tile, job->global_data);
}

static inline void init_render_jobs(
    struct render_job* jobs,
    size_t count,
    struct global_data* global_data) {
    for (size_t i = 1; i < count; ++i) {
        jobs[i - 1].work_item.work_fn = render_job;
        jobs[i - 1].work_item.next = &jobs[i].work_item;
        jobs[i - 1].global_data = global_data;
    }
    jobs[count - 1].work_item.work_fn = render_job;
    jobs[count - 1].work_item.next = NULL;
    jobs[count - 1].global_data = global_data;

}
#endif

int main() {
    int m = 2000;
    int n = 2000;

    struct global_data global_data = {
        .x_max =   1.25,
        .x_min = - 2.25,
        .y_max =   1.75,
        .y_min = - 1.75,
        .count_max = 2000,
        .n = n,
        .m = m,
        .pixels = malloc(n * m * sizeof(uint32_t))
    };

#ifdef USE_OPENMP
    const char* output_file = "mandelbrot_omp.ppm";
#else
    size_t thread_count = detect_system_thread_count();
    struct thread_pool* thread_pool = new_thread_pool(thread_count);
    const char* output_file = "mandelbrot.ppm";
    printf("Thread pool with %zu thread(s) created\n", thread_count);
#endif

    struct timespec t_start;
    timespec_get(&t_start, TIME_UTC);
#ifdef USE_OPENMP
    render_tile(&(struct tile) {
        .i = 0, .j = 0, .n = n, .m = m
    }, &global_data);
#else
    struct render_job jobs[thread_count * 3];
    init_render_jobs(jobs, thread_count * 3, &global_data);
    struct render_job* current_job = jobs;
    struct render_job* first_job = jobs;
    struct render_job* previous_job = NULL;
    for (int i = 0; i < m; i += 50) {
        for (int j = 0; j < n; j += 50) {
            assert(current_job);
            current_job->tile = (struct tile) {
                .i = i,
                .j = j,
                .n = j + 50,
                .m = i + 50
            };
            previous_job = current_job;
            current_job = (struct render_job*)current_job->work_item.next;
            if (!current_job) {
                submit_work(thread_pool, &first_job->work_item, &previous_job->work_item);
                current_job = first_job = (struct render_job*)wait_for_completion(thread_pool, thread_count);
                previous_job = NULL;
            }
        }
    }
    if (previous_job)
        submit_work(thread_pool, &first_job->work_item, &previous_job->work_item);
    wait_for_completion(thread_pool, 0);
#endif
    struct timespec t_end;
    timespec_get(&t_end, TIME_UTC);

#ifndef USE_OPENMP
    free_thread_pool(thread_pool);
#endif
    printf("Rendering took %g seconds\n", elapsed_seconds(&t_start, &t_end));

    write_ppm(output_file, global_data.pixels, n, m);
    printf("Image written to \"%s\"\n", output_file);

    free(global_data.pixels);
    return 0;
}
