#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir((path), 0755)
#endif

typedef struct Config {
    const char *base_rgb_path;
    const char *input_bayer_path;
    const char *generated_bayer_path;
    const char *output_dir;
    int width;
    int height;
    int black_level;
    float wb_r;
    float wb_g;
    float wb_b;
    float lsc_strength;
    float gamma;
    float ccm_strength;
    float saturation;
    float artifact_strength;
    bool generate_previews;
    bool skip_generate;
} Config;

static float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static uint8_t clamp_u8(float value)
{
    if (value < 0.0f)
        return 0;
    if (value > 255.0f)
        return 255;
    return (uint8_t)(value + 0.5f);
}

static float clamp_non_negative(float value)
{
    return value < 0.0f ? 0.0f : value;
}

static int ensure_dir(const char *path)
{
    int ret = MKDIR(path);
    if (ret == 0 || errno == EEXIST)
        return 0;
    fprintf(stderr, "Failed to create directory %s\n", path);
    return -1;
}

static uint8_t *read_file_exact(const char *path, size_t expected_size)
{
    FILE *fp = fopen(path, "rb");
    uint8_t *buffer;
    size_t read_size;

    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", path);
        return NULL;
    }

    buffer = (uint8_t *)malloc(expected_size);
    if (!buffer) {
        fclose(fp);
        fprintf(stderr, "Out of memory reading %s\n", path);
        return NULL;
    }

    read_size = fread(buffer, 1, expected_size, fp);
    fclose(fp);

    if (read_size != expected_size) {
        fprintf(stderr, "Size mismatch for %s: got %lu, expected %lu\n",
                path, (unsigned long)read_size, (unsigned long)expected_size);
        free(buffer);
        return NULL;
    }

    return buffer;
}

static int write_file(const char *path, const void *payload, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to write %s\n", path);
        return -1;
    }

    if (fwrite(payload, 1, size, fp) != size) {
        fclose(fp);
        fprintf(stderr, "Short write for %s\n", path);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int write_pgm(const char *path, int width, int height, const uint8_t *payload, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to write %s\n", path);
        return -1;
    }
    fprintf(fp, "P5\n%d %d\n255\n", width, height);
    if (fwrite(payload, 1, size, fp) != size) {
        fclose(fp);
        fprintf(stderr, "Short write for %s\n", path);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int write_ppm(const char *path, int width, int height, const uint8_t *payload, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to write %s\n", path);
        return -1;
    }
    fprintf(fp, "P6\n%d %d\n255\n", width, height);
    if (fwrite(payload, 1, size, fp) != size) {
        fclose(fp);
        fprintf(stderr, "Short write for %s\n", path);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int generate_preview_png(const char *raw_path,
                                const char *preview_path,
                                const char *pixel_format,
                                int width,
                                int height)
{
    char command[1600];
    int written = snprintf(
        command,
        sizeof(command),
        "ffmpeg -y -hide_banner -loglevel error -f rawvideo -pixel_format %s "
        "-video_size %dx%d -i \"%s\" -frames:v 1 \"%s\"",
        pixel_format,
        width,
        height,
        raw_path,
        preview_path);

    if (written <= 0 || written >= (int)sizeof(command)) {
        fprintf(stderr, "Preview command too long for %s\n", raw_path);
        return -1;
    }

    if (system(command) != 0) {
        fprintf(stderr, "Warning: failed to generate preview for %s\n", raw_path);
        return -1;
    }

    return 0;
}

static void plane_to_u8(const float *plane, uint8_t *out, int count)
{
    int i;
    for (i = 0; i < count; ++i)
        out[i] = clamp_u8(clamp01(plane[i]) * 255.0f);
}

static int generate_extreme_sensor_raw(const Config *cfg)
{
    const size_t rgb_size = (size_t)cfg->width * (size_t)cfg->height * 3u;
    const size_t bayer_size = (size_t)cfg->width * (size_t)cfg->height;
    uint8_t *rgb = read_file_exact(cfg->base_rgb_path, rgb_size);
    uint8_t *out;
    int x;
    int y;
    float strength = clamp_non_negative(cfg->artifact_strength);

    if (!rgb)
        return -1;

    out = (uint8_t *)malloc(bayer_size);
    if (!out) {
        free(rgb);
        fprintf(stderr, "Out of memory generating extreme sensor raw\n");
        return -1;
    }

    for (y = 0; y < cfg->height; ++y) {
        float ny = (2.0f * (float)y / (float)(cfg->height - 1)) - 1.0f;
        for (x = 0; x < cfg->width; ++x) {
            const size_t rgb_idx = ((size_t)y * (size_t)cfg->width + (size_t)x) * 3u;
            const size_t out_idx = (size_t)y * (size_t)cfg->width + (size_t)x;
            float nx = (2.0f * (float)x / (float)(cfg->width - 1)) - 1.0f;
            float radius2 = (nx * nx + ny * ny) * 0.5f;
            float r = (float)rgb[rgb_idx + 0] / 255.0f;
            float g = (float)rgb[rgb_idx + 1] / 255.0f;
            float b = (float)rgb[rgb_idx + 2] / 255.0f;
            float sample;
            float r_gain;
            float g_gain;
            float b_gain;
            float row_bias;
            float col_bias;
            float x_norm = (float)x / (float)(cfg->width - 1);
            float y_norm = (float)y / (float)(cfg->height - 1);

            if (radius2 > 1.0f)
                radius2 = 1.0f;

            r_gain = (1.0f - 0.78f * strength) * (1.0f - 0.68f * strength * radius2);
            g_gain = (1.0f - 0.42f * strength) * (1.0f - 0.48f * strength * radius2);
            b_gain = (1.0f - 0.05f * strength) * (1.0f - 0.22f * strength * radius2);
            row_bias = 1.0f + strength * (-0.12f + 0.18f * y_norm);
            col_bias = 1.0f + strength * (-0.10f + 0.16f * x_norm);

            r *= r_gain * row_bias;
            g *= g_gain * col_bias;
            b *= b_gain * (1.0f - 0.04f * strength * radius2);

            if ((y & 1) == 0)
                sample = (x & 1) == 0 ? r : g;
            else
                sample = (x & 1) == 0 ? g : b;

            out[out_idx] = clamp_u8((float)cfg->black_level + sample * (255.0f - (float)cfg->black_level));
        }
    }

    if (write_file(cfg->generated_bayer_path, out, bayer_size) != 0) {
        free(out);
        free(rgb);
        return -1;
    }

    printf("Generated extreme Bayer raw: %s\n", cfg->generated_bayer_path);

    free(out);
    free(rgb);
    return 0;
}

static void subtract_black_level(const uint8_t *raw, float *out, int count, int black_level)
{
    int i;
    float scale = 1.0f / (float)((255 - black_level) > 0 ? (255 - black_level) : 1);
    for (i = 0; i < count; ++i)
        out[i] = clamp01(((float)raw[i] - (float)black_level) * scale);
}

static void apply_lsc_and_wb(const float *plane, float *out, const Config *cfg)
{
    int x;
    int y;
    float w_max = (float)(cfg->width - 1);
    float h_max = (float)(cfg->height - 1);

    for (y = 0; y < cfg->height; ++y) {
        float ny = (2.0f * (float)y / h_max) - 1.0f;
        for (x = 0; x < cfg->width; ++x) {
            size_t idx = (size_t)y * (size_t)cfg->width + (size_t)x;
            float nx = (2.0f * (float)x / w_max) - 1.0f;
            float radius2 = (nx * nx + ny * ny) * 0.5f;
            float lsc_gain;
            float color_gain;

            if (radius2 > 1.0f)
                radius2 = 1.0f;

            lsc_gain = 1.0f + cfg->lsc_strength * radius2;

            if ((y & 1) == 0 && (x & 1) == 0)
                color_gain = cfg->wb_r;
            else if ((y & 1) == 1 && (x & 1) == 1)
                color_gain = cfg->wb_b;
            else
                color_gain = cfg->wb_g;

            out[idx] = clamp01(plane[idx] * lsc_gain * color_gain);
        }
    }
}

static float sample_plane(const float *plane, int width, int height, int x, int y)
{
    if (x < 0)
        x = 0;
    else if (x >= width)
        x = width - 1;

    if (y < 0)
        y = 0;
    else if (y >= height)
        y = height - 1;

    return plane[(size_t)y * (size_t)width + (size_t)x];
}

static void demosaic_and_finish(const float *plane,
                                uint8_t *linear_rgb,
                                uint8_t *display_rgb,
                                const Config *cfg)
{
    static const float base_ccm[3][3] = {
        { 1.20f, -0.12f, -0.08f },
        { -0.05f, 1.10f, -0.05f },
        { -0.03f, -0.22f, 1.25f },
    };
    float ccm[3][3];

    int x;
    int y;
    float inv_gamma = 1.0f / cfg->gamma;
    float ccm_strength = cfg->ccm_strength;
    float saturation = cfg->saturation;
    int row;
    int col;

    if (ccm_strength < 0.0f)
        ccm_strength = 0.0f;
    if (ccm_strength > 2.0f)
        ccm_strength = 2.0f;
    if (saturation < 0.0f)
        saturation = 0.0f;
    if (saturation > 2.5f)
        saturation = 2.5f;

    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            float identity = (row == col) ? 1.0f : 0.0f;
            ccm[row][col] = identity + (base_ccm[row][col] - identity) * ccm_strength;
        }
    }

    for (y = 0; y < cfg->height; ++y) {
        for (x = 0; x < cfg->width; ++x) {
            float r;
            float g;
            float b;
            float rr;
            float gg;
            float bb;
            size_t idx = ((size_t)y * (size_t)cfg->width + (size_t)x) * 3u;

            if ((y & 1) == 0 && (x & 1) == 0) {
                r = sample_plane(plane, cfg->width, cfg->height, x, y);
                g = (sample_plane(plane, cfg->width, cfg->height, x - 1, y) +
                     sample_plane(plane, cfg->width, cfg->height, x + 1, y) +
                     sample_plane(plane, cfg->width, cfg->height, x, y - 1) +
                     sample_plane(plane, cfg->width, cfg->height, x, y + 1)) * 0.25f;
                b = (sample_plane(plane, cfg->width, cfg->height, x - 1, y - 1) +
                     sample_plane(plane, cfg->width, cfg->height, x + 1, y - 1) +
                     sample_plane(plane, cfg->width, cfg->height, x - 1, y + 1) +
                     sample_plane(plane, cfg->width, cfg->height, x + 1, y + 1)) * 0.25f;
            } else if ((y & 1) == 1 && (x & 1) == 1) {
                b = sample_plane(plane, cfg->width, cfg->height, x, y);
                g = (sample_plane(plane, cfg->width, cfg->height, x - 1, y) +
                     sample_plane(plane, cfg->width, cfg->height, x + 1, y) +
                     sample_plane(plane, cfg->width, cfg->height, x, y - 1) +
                     sample_plane(plane, cfg->width, cfg->height, x, y + 1)) * 0.25f;
                r = (sample_plane(plane, cfg->width, cfg->height, x - 1, y - 1) +
                     sample_plane(plane, cfg->width, cfg->height, x + 1, y - 1) +
                     sample_plane(plane, cfg->width, cfg->height, x - 1, y + 1) +
                     sample_plane(plane, cfg->width, cfg->height, x + 1, y + 1)) * 0.25f;
            } else if ((y & 1) == 0) {
                g = sample_plane(plane, cfg->width, cfg->height, x, y);
                r = (sample_plane(plane, cfg->width, cfg->height, x - 1, y) +
                     sample_plane(plane, cfg->width, cfg->height, x + 1, y)) * 0.5f;
                b = (sample_plane(plane, cfg->width, cfg->height, x, y - 1) +
                     sample_plane(plane, cfg->width, cfg->height, x, y + 1)) * 0.5f;
            } else {
                g = sample_plane(plane, cfg->width, cfg->height, x, y);
                r = (sample_plane(plane, cfg->width, cfg->height, x, y - 1) +
                     sample_plane(plane, cfg->width, cfg->height, x, y + 1)) * 0.5f;
                b = (sample_plane(plane, cfg->width, cfg->height, x - 1, y) +
                     sample_plane(plane, cfg->width, cfg->height, x + 1, y)) * 0.5f;
            }

            linear_rgb[idx + 0] = clamp_u8(clamp01(r) * 255.0f);
            linear_rgb[idx + 1] = clamp_u8(clamp01(g) * 255.0f);
            linear_rgb[idx + 2] = clamp_u8(clamp01(b) * 255.0f);

            rr = clamp01(ccm[0][0] * r + ccm[0][1] * g + ccm[0][2] * b);
            gg = clamp01(ccm[1][0] * r + ccm[1][1] * g + ccm[1][2] * b);
            bb = clamp01(ccm[2][0] * r + ccm[2][1] * g + ccm[2][2] * b);

            {
                float luma = 0.299f * rr + 0.587f * gg + 0.114f * bb;
                rr = clamp01(luma + saturation * (rr - luma));
                gg = clamp01(luma + saturation * (gg - luma));
                bb = clamp01(luma + saturation * (bb - luma));
            }

            rr = powf(rr, inv_gamma);
            gg = powf(gg, inv_gamma);
            bb = powf(bb, inv_gamma);

            display_rgb[idx + 0] = clamp_u8(clamp01(rr) * 255.0f);
            display_rgb[idx + 1] = clamp_u8(clamp01(gg) * 255.0f);
            display_rgb[idx + 2] = clamp_u8(clamp01(bb) * 255.0f);
        }
    }
}

static void rgb_to_yuv420p(const uint8_t *rgb, uint8_t *yuv, const Config *cfg)
{
    const int width = cfg->width;
    const int height = cfg->height;
    const int uv_width = width / 2;
    const int uv_height = height / 2;
    uint8_t *y_plane = yuv;
    uint8_t *u_plane = yuv + width * height;
    uint8_t *v_plane = u_plane + uv_width * uv_height;
    uint8_t *u_full = (uint8_t *)malloc((size_t)width * (size_t)height);
    uint8_t *v_full = (uint8_t *)malloc((size_t)width * (size_t)height);
    int x;
    int y;

    if (!u_full || !v_full) {
        free(u_full);
        free(v_full);
        fprintf(stderr, "Out of memory converting RGB to YUV420P\n");
        exit(1);
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            size_t rgb_idx = ((size_t)y * (size_t)width + (size_t)x) * 3u;
            size_t pos = (size_t)y * (size_t)width + (size_t)x;
            float r = (float)rgb[rgb_idx + 0];
            float g = (float)rgb[rgb_idx + 1];
            float b = (float)rgb[rgb_idx + 2];
            float y_val = 0.299f * r + 0.587f * g + 0.114f * b;
            float u_val = -0.168736f * r - 0.331264f * g + 0.5f * b + 128.0f;
            float v_val = 0.5f * r - 0.418688f * g - 0.081312f * b + 128.0f;

            y_plane[pos] = clamp_u8(y_val);
            u_full[pos] = clamp_u8(u_val);
            v_full[pos] = clamp_u8(v_val);
        }
    }

    for (y = 0; y < height; y += 2) {
        for (x = 0; x < width; x += 2) {
            unsigned sum_u = 0;
            unsigned sum_v = 0;
            int dy;
            int dx;
            size_t uv_pos = (size_t)(y / 2) * (size_t)uv_width + (size_t)(x / 2);

            for (dy = 0; dy < 2; ++dy) {
                for (dx = 0; dx < 2; ++dx) {
                    int sx = x + dx;
                    int sy = y + dy;
                    size_t pos;

                    if (sx >= width)
                        sx = width - 1;
                    if (sy >= height)
                        sy = height - 1;

                    pos = (size_t)sy * (size_t)width + (size_t)sx;
                    sum_u += u_full[pos];
                    sum_v += v_full[pos];
                }
            }

            u_plane[uv_pos] = (uint8_t)((sum_u + 2u) / 4u);
            v_plane[uv_pos] = (uint8_t)((sum_v + 2u) / 4u);
        }
    }

    free(u_full);
    free(v_full);
}

static int run_pipeline(const Config *cfg)
{
    const int pixel_count = cfg->width * cfg->height;
    const size_t bayer_size = (size_t)pixel_count;
    const size_t rgb_size = (size_t)pixel_count * 3u;
    const size_t yuv_size = (size_t)pixel_count + 2u * (size_t)(cfg->width / 2) * (size_t)(cfg->height / 2);
    uint8_t *sensor_u8 = read_file_exact(cfg->input_bayer_path, bayer_size);
    float *black_plane;
    float *lsc_wb_plane;
    uint8_t *black_u8;
    uint8_t *lsc_wb_u8;
    uint8_t *linear_rgb;
    uint8_t *display_rgb;
    uint8_t *display_yuv;
    char path[512];
    char raw00_path[512];
    char raw01_path[512];
    char raw02_path[512];
    char raw03_path[512];
    char raw04_path[512];
    char raw05_path[512];
    char preview00_path[512];
    char preview01_path[512];
    char preview02_path[512];
    char preview03_path[512];
    char preview04_path[512];
    char preview05_path[512];

    if (!sensor_u8)
        return -1;

    if (ensure_dir(cfg->output_dir) != 0) {
        free(sensor_u8);
        return -1;
    }

    black_plane = (float *)malloc(sizeof(float) * (size_t)pixel_count);
    lsc_wb_plane = (float *)malloc(sizeof(float) * (size_t)pixel_count);
    black_u8 = (uint8_t *)malloc(bayer_size);
    lsc_wb_u8 = (uint8_t *)malloc(bayer_size);
    linear_rgb = (uint8_t *)malloc(rgb_size);
    display_rgb = (uint8_t *)malloc(rgb_size);
    display_yuv = (uint8_t *)malloc(yuv_size);

    if (!black_plane || !lsc_wb_plane || !black_u8 || !lsc_wb_u8 ||
        !linear_rgb || !display_rgb || !display_yuv) {
        fprintf(stderr, "Out of memory in ISP pipeline\n");
        free(sensor_u8);
        free(black_plane);
        free(lsc_wb_plane);
        free(black_u8);
        free(lsc_wb_u8);
        free(linear_rgb);
        free(display_rgb);
        free(display_yuv);
        return -1;
    }

    subtract_black_level(sensor_u8, black_plane, pixel_count, cfg->black_level);
    apply_lsc_and_wb(black_plane, lsc_wb_plane, cfg);
    plane_to_u8(black_plane, black_u8, pixel_count);
    plane_to_u8(lsc_wb_plane, lsc_wb_u8, pixel_count);
    demosaic_and_finish(lsc_wb_plane, linear_rgb, display_rgb, cfg);
    rgb_to_yuv420p(display_rgb, display_yuv, cfg);

    snprintf(raw00_path, sizeof(raw00_path), "%s/00_sensor_mosaic_rggb8.raw", cfg->output_dir);
    write_file(raw00_path, sensor_u8, bayer_size);
    snprintf(path, sizeof(path), "%s/00_sensor_mosaic.pgm", cfg->output_dir);
    write_pgm(path, cfg->width, cfg->height, sensor_u8, bayer_size);
    snprintf(preview00_path, sizeof(preview00_path), "%s/00_sensor_preview.png", cfg->output_dir);

    snprintf(raw01_path, sizeof(raw01_path), "%s/01_black_level_gray8.raw", cfg->output_dir);
    write_file(raw01_path, black_u8, bayer_size);
    snprintf(path, sizeof(path), "%s/01_black_level.pgm", cfg->output_dir);
    write_pgm(path, cfg->width, cfg->height, black_u8, bayer_size);
    snprintf(preview01_path, sizeof(preview01_path), "%s/01_black_level_preview.png", cfg->output_dir);

    snprintf(raw02_path, sizeof(raw02_path), "%s/02_lsc_wb_gray8.raw", cfg->output_dir);
    write_file(raw02_path, lsc_wb_u8, bayer_size);
    snprintf(path, sizeof(path), "%s/02_lsc_wb.pgm", cfg->output_dir);
    write_pgm(path, cfg->width, cfg->height, lsc_wb_u8, bayer_size);
    snprintf(preview02_path, sizeof(preview02_path), "%s/02_lsc_wb_preview.png", cfg->output_dir);

    snprintf(raw03_path, sizeof(raw03_path), "%s/03_demosaic_linear_rgb24.raw", cfg->output_dir);
    write_file(raw03_path, linear_rgb, rgb_size);
    snprintf(path, sizeof(path), "%s/03_demosaic_linear.ppm", cfg->output_dir);
    write_ppm(path, cfg->width, cfg->height, linear_rgb, rgb_size);
    snprintf(preview03_path, sizeof(preview03_path), "%s/03_demosaic_linear_preview.png", cfg->output_dir);

    snprintf(raw04_path, sizeof(raw04_path), "%s/04_display_rgb24.raw", cfg->output_dir);
    write_file(raw04_path, display_rgb, rgb_size);
    snprintf(path, sizeof(path), "%s/04_display.ppm", cfg->output_dir);
    write_ppm(path, cfg->width, cfg->height, display_rgb, rgb_size);
    snprintf(preview04_path, sizeof(preview04_path), "%s/04_display_preview.png", cfg->output_dir);

    snprintf(raw05_path, sizeof(raw05_path), "%s/05_display_yuv420p.raw", cfg->output_dir);
    write_file(raw05_path, display_yuv, yuv_size);
    snprintf(preview05_path, sizeof(preview05_path), "%s/05_display_yuv420p_preview.png", cfg->output_dir);

    if (cfg->generate_previews) {
        generate_preview_png(raw00_path, preview00_path, "gray", cfg->width, cfg->height);
        generate_preview_png(raw01_path, preview01_path, "gray", cfg->width, cfg->height);
        generate_preview_png(raw02_path, preview02_path, "gray", cfg->width, cfg->height);
        generate_preview_png(raw03_path, preview03_path, "rgb24", cfg->width, cfg->height);
        generate_preview_png(raw04_path, preview04_path, "rgb24", cfg->width, cfg->height);
        generate_preview_png(raw05_path, preview05_path, "yuv420p", cfg->width, cfg->height);
    }

    printf("C ISP demo finished.\n");
    printf("Input Bayer : %s\n", cfg->input_bayer_path);
    printf("Output dir  : %s\n", cfg->output_dir);
    printf("View commands:\n");
    printf("  ffplay -f rawvideo -pixel_format gray -video_size %dx%d %s/00_sensor_mosaic_rggb8.raw\n",
           cfg->width, cfg->height, cfg->output_dir);
    printf("  ffplay -f rawvideo -pixel_format gray -video_size %dx%d %s/02_lsc_wb_gray8.raw\n",
           cfg->width, cfg->height, cfg->output_dir);
    printf("  ffplay -f rawvideo -pixel_format rgb24 -video_size %dx%d %s/04_display_rgb24.raw\n",
           cfg->width, cfg->height, cfg->output_dir);
    printf("  ffplay -f rawvideo -pixel_format yuv420p -video_size %dx%d %s/05_display_yuv420p.raw\n",
           cfg->width, cfg->height, cfg->output_dir);
    if (cfg->generate_previews) {
        printf("Preview PNGs:\n");
        printf("  %s\n", preview00_path);
        printf("  %s\n", preview01_path);
        printf("  %s\n", preview02_path);
        printf("  %s\n", preview03_path);
        printf("  %s\n", preview04_path);
        printf("  %s\n", preview05_path);
    }

    free(sensor_u8);
    free(black_plane);
    free(lsc_wb_plane);
    free(black_u8);
    free(lsc_wb_u8);
    free(linear_rgb);
    free(display_rgb);
    free(display_yuv);
    return 0;
}

static void print_usage(const char *program)
{
    printf("Usage: %s [options]\n", program);
    printf("Options:\n");
    printf("  --base-rgb PATH           RGB24 source image used to synthesize Bayer raw\n");
    printf("  --skip-generate           Use existing Bayer input instead of regenerating it\n");
    printf("  --input-bayer PATH        Bayer input path\n");
    printf("  --generated-bayer PATH    Output path for generated Bayer input\n");
    printf("  --output-dir PATH         Output directory for ISP stages\n");
    printf("  --black-level N           Sensor black level offset, default 64\n");
    printf("  --wb-r F                  Red white-balance gain, default 3.2\n");
    printf("  --wb-g F                  Green white-balance gain, default 1.35\n");
    printf("  --wb-b F                  Blue white-balance gain, default 0.72\n");
    printf("  --lsc-strength F          Lens shading correction strength, default 1.35\n");
    printf("  --gamma F                 Display gamma, default 2.2\n");
    printf("  --ccm-strength F          Color correction matrix strength, default 1.0\n");
    printf("  --saturation F            Color saturation, default 1.0\n");
    printf("  --artifact-strength F     Fake sensor artifact strength, default 1.0\n");
    printf("  --no-previews             Skip automatic preview PNG generation\n");
    printf("  --help                    Show this message\n");
}

static int parse_int_value(const char *text, int *out)
{
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (!text[0] || (end && *end != '\0'))
        return -1;
    *out = (int)value;
    return 0;
}

static int parse_float_value(const char *text, float *out)
{
    char *end = NULL;
    float value = strtof(text, &end);
    if (!text[0] || (end && *end != '\0'))
        return -1;
    *out = value;
    return 0;
}

static int parse_args(int argc, char **argv, Config *cfg)
{
    int i;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--base-rgb") == 0 && i + 1 < argc) {
            cfg->base_rgb_path = argv[++i];
        } else if (strcmp(argv[i], "--skip-generate") == 0) {
            cfg->skip_generate = true;
        } else if (strcmp(argv[i], "--input-bayer") == 0 && i + 1 < argc) {
            cfg->input_bayer_path = argv[++i];
            cfg->skip_generate = true;
        } else if (strcmp(argv[i], "--generated-bayer") == 0 && i + 1 < argc) {
            cfg->generated_bayer_path = argv[++i];
            cfg->input_bayer_path = cfg->generated_bayer_path;
        } else if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            cfg->output_dir = argv[++i];
        } else if (strcmp(argv[i], "--black-level") == 0 && i + 1 < argc) {
            if (parse_int_value(argv[++i], &cfg->black_level) != 0) {
                fprintf(stderr, "Invalid --black-level value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--wb-r") == 0 && i + 1 < argc) {
            if (parse_float_value(argv[++i], &cfg->wb_r) != 0) {
                fprintf(stderr, "Invalid --wb-r value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--wb-g") == 0 && i + 1 < argc) {
            if (parse_float_value(argv[++i], &cfg->wb_g) != 0) {
                fprintf(stderr, "Invalid --wb-g value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--wb-b") == 0 && i + 1 < argc) {
            if (parse_float_value(argv[++i], &cfg->wb_b) != 0) {
                fprintf(stderr, "Invalid --wb-b value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--lsc-strength") == 0 && i + 1 < argc) {
            if (parse_float_value(argv[++i], &cfg->lsc_strength) != 0) {
                fprintf(stderr, "Invalid --lsc-strength value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gamma") == 0 && i + 1 < argc) {
            if (parse_float_value(argv[++i], &cfg->gamma) != 0) {
                fprintf(stderr, "Invalid --gamma value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--ccm-strength") == 0 && i + 1 < argc) {
            if (parse_float_value(argv[++i], &cfg->ccm_strength) != 0) {
                fprintf(stderr, "Invalid --ccm-strength value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--saturation") == 0 && i + 1 < argc) {
            if (parse_float_value(argv[++i], &cfg->saturation) != 0) {
                fprintf(stderr, "Invalid --saturation value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--artifact-strength") == 0 && i + 1 < argc) {
            if (parse_float_value(argv[++i], &cfg->artifact_strength) != 0) {
                fprintf(stderr, "Invalid --artifact-strength value\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--no-previews") == 0) {
            cfg->generate_previews = false;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 1;
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    Config cfg = {
        "raw_samples/synthetic_rgb24_640x480.raw",
        "raw_samples/synthetic_sensor_extreme_rggb8_640x480.raw",
        "raw_samples/synthetic_sensor_extreme_rggb8_640x480.raw",
        "raw_samples/isp_demo_out_c_extreme",
        640,
        480,
        64,
        3.2f,
        1.35f,
        0.72f,
        1.35f,
        2.2f,
        1.0f,
        1.0f,
        1.0f,
        true,
        false,
    };
    int parse_ret = parse_args(argc, argv, &cfg);
    if (parse_ret != 0)
        return parse_ret > 0 ? 0 : 1;

    if (!cfg.skip_generate) {
        if (generate_extreme_sensor_raw(&cfg) != 0)
            return 1;
    }

    if (run_pipeline(&cfg) != 0)
        return 1;

    return 0;
}
