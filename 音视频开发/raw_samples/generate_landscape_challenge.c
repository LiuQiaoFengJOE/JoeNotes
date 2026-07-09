#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Color {
    float r;
    float g;
    float b;
} Color;

static float stepfmodf(float value, float mod);

static float clamp01(float v)
{
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

static uint8_t to_u8(float v)
{
    return (uint8_t)(clamp01(v) * 255.0f + 0.5f);
}

static float mixf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static Color mixc(Color a, Color b, float t)
{
    Color out = {
        mixf(a.r, b.r, t),
        mixf(a.g, b.g, t),
        mixf(a.b, b.b, t),
    };
    return out;
}

static float smoothstepf(float edge0, float edge1, float x)
{
    float t;
    if (edge0 == edge1)
        return x < edge0 ? 0.0f : 1.0f;
    t = clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

static float fractf(float x)
{
    return x - floorf(x);
}

static float hash2(float x, float y)
{
    return fractf(sinf(x * 127.1f + y * 311.7f) * 43758.5453f);
}

static float noise2(float x, float y)
{
    float ix = floorf(x);
    float iy = floorf(y);
    float fx = x - ix;
    float fy = y - iy;
    float a = hash2(ix, iy);
    float b = hash2(ix + 1.0f, iy);
    float c = hash2(ix, iy + 1.0f);
    float d = hash2(ix + 1.0f, iy + 1.0f);
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    return mixf(mixf(a, b, ux), mixf(c, d, ux), uy);
}

static float fbm(float x, float y)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    int octave;
    for (octave = 0; octave < 5; ++octave) {
        value += amplitude * noise2(x, y);
        x *= 2.03f;
        y *= 2.11f;
        amplitude *= 0.5f;
    }
    return value;
}

static float far_ridge(float u)
{
    return 0.48f
         + 0.02f * sinf(u * 5.7f + 0.3f)
         + 0.015f * sinf(u * 13.0f - 1.1f)
         + 0.012f * fbm(u * 2.0f, 0.4f);
}

static float mid_ridge(float u)
{
    float peaks = 0.18f * powf(fabsf(sinf(u * 3.4f + 0.5f)), 1.4f)
                + 0.08f * powf(fabsf(sinf(u * 10.6f - 0.6f)), 2.5f);
    return 0.63f - peaks + 0.012f * sinf(u * 8.0f + 1.2f);
}

static float shore_line(float u)
{
    return 0.64f
         + 0.022f * sinf(u * 4.2f + 1.7f)
         + 0.012f * sinf(u * 15.0f)
         + 0.008f * fbm(u * 4.0f, 2.4f);
}

static Color color_sky(float u, float v)
{
    const float horizon = 0.60f;
    Color top = { 0.04f, 0.09f, 0.28f };
    Color mid = { 0.12f, 0.42f, 0.86f };
    Color warm = { 1.00f, 0.50f, 0.16f };
    Color sky = mixc(top, mid, smoothstepf(0.0f, 0.55f, v / horizon));
    float warm_mix = smoothstepf(0.32f, 0.98f, v / horizon);
    float sun_dx = u - 0.73f;
    float sun_dy = v - 0.20f;
    float sun_dist = sqrtf(sun_dx * sun_dx + sun_dy * sun_dy);
    float glow = expf(-(sun_dist * sun_dist) / 0.020f);
    float sun_core = expf(-(sun_dist * sun_dist) / 0.0012f);
    float cloud = fbm(u * 5.0f + 1.1f, v * 11.0f - 0.7f);
    float cloud2 = fbm(u * 9.0f - 2.3f, v * 17.0f + 4.1f);
    float cloud_mask = smoothstepf(0.60f, 0.80f, 0.65f * cloud + 0.35f * cloud2);
    float cloud_fade = 1.0f - smoothstepf(0.43f, 0.96f, v / horizon);
    Color cloud_color = mixc((Color){ 1.00f, 0.90f, 0.78f }, (Color){ 1.00f, 0.72f, 0.56f }, glow * 0.8f);

    sky = mixc(sky, warm, warm_mix * 0.68f);
    sky = mixc(sky, cloud_color, cloud_mask * cloud_fade * (0.22f + 0.45f * glow));
    sky = mixc(sky, (Color){ 1.00f, 0.86f, 0.62f }, glow * 0.58f);
    sky = mixc(sky, (Color){ 1.00f, 0.96f, 0.88f }, sun_core);

    return sky;
}

static Color color_mountains(float u, float v)
{
    float far = far_ridge(u);
    float mid = mid_ridge(u);
    float shore = shore_line(u);
    Color color = color_sky(u, v);

    if (v > far && v < shore) {
        float haze = smoothstepf(far, shore, v);
        Color far_color = mixc((Color){ 0.22f, 0.34f, 0.52f }, (Color){ 0.34f, 0.42f, 0.56f }, haze);
        color = far_color;
    }

    if (v > mid && v < shore) {
        float shade = clamp01(0.25f + 0.95f * u - 0.7f * (v - mid));
        Color shadow = { 0.12f, 0.14f, 0.21f };
        Color lit = { 0.56f, 0.42f, 0.38f };
        Color rock = mixc(shadow, lit, shade);
        float snow_line = mid + 0.028f + 0.012f * fbm(u * 8.0f, v * 19.0f);
        if (v < snow_line) {
            Color snow = mixc((Color){ 0.76f, 0.82f, 0.90f }, (Color){ 1.00f, 0.97f, 0.93f }, shade * 0.7f);
            rock = mixc(rock, snow, smoothstepf(snow_line - 0.03f, snow_line, v));
        }
        color = mixc(color, rock, 0.95f);
    }

    if (v > shore && v < 0.69f) {
        float tree = fbm(u * 28.0f, v * 45.0f);
        float band = smoothstepf(shore, 0.69f, v);
        Color forest = mixc((Color){ 0.03f, 0.08f, 0.05f }, (Color){ 0.08f, 0.20f, 0.10f }, tree);
        Color sun_hit = { 0.18f, 0.32f, 0.12f };
        forest = mixc(forest, sun_hit, smoothstepf(0.62f, 0.98f, u) * (1.0f - band) * 0.6f);
        color = mixc(color, forest, 1.0f);
    }

    return color;
}

static Color color_upper_scene(float u, float v)
{
    return color_mountains(u, v);
}

static Color color_lake(float u, float v)
{
    float shore = shore_line(u);
    float reflect_y = shore - (v - shore) * 0.86f;
    float ripple = 0.008f * sinf(110.0f * v + u * 20.0f)
                 + 0.006f * (fbm(u * 20.0f, v * 80.0f) - 0.5f);
    Color reflected = color_upper_scene(clamp01(u + ripple * 0.6f), clamp01(reflect_y + ripple));
    Color water_tint = { 0.03f, 0.16f, 0.22f };
    float depth = smoothstepf(shore, 0.90f, v);
    float shimmer = expf(-fabsf(u - 0.73f) * 18.0f) * expf(-(v - shore) * 12.0f);
    float mist = smoothstepf(shore, shore + 0.05f, v) * (1.0f - smoothstepf(shore + 0.05f, shore + 0.12f, v));

    reflected = mixc(reflected, water_tint, 0.48f + 0.22f * depth);
    reflected = mixc(reflected, (Color){ 0.72f, 0.82f, 0.78f }, mist * 0.20f);
    reflected = mixc(reflected, (Color){ 1.00f, 0.84f, 0.58f }, shimmer * 0.38f);
    return reflected;
}

static Color color_foreground(float u, float v)
{
    float meadow_t = smoothstepf(0.86f, 1.0f, v);
    float path = fabsf(u - (0.62f - 0.30f * meadow_t));
    float grass = fbm(u * 35.0f, v * 50.0f);
    Color base = mixc((Color){ 0.02f, 0.05f, 0.02f }, (Color){ 0.12f, 0.24f, 0.06f }, grass);
    Color warm = { 0.24f, 0.20f, 0.08f };
    float path_mask = smoothstepf(0.12f, 0.01f, path) * meadow_t;
    float flowers = smoothstepf(0.83f, 0.93f, grass) * meadow_t;
    Color flower_color = mixc((Color){ 0.90f, 0.80f, 0.24f }, (Color){ 0.85f, 0.22f, 0.18f }, stepfmodf(u * 17.0f + v * 9.0f, 2.0f));
    base = mixc(base, warm, path_mask * 0.7f);
    base = mixc(base, flower_color, flowers * 0.16f);
    return base;
}

/* Tiny deterministic 0/1 selector without pulling in extra randomness. */
static float stepfmodf(float value, float mod)
{
    float wrapped = fmodf(value, mod);
    return wrapped < (mod * 0.5f) ? 0.0f : 1.0f;
}

int main(void)
{
    const int width = 640;
    const int height = 480;
    const size_t rgb_size = (size_t)width * (size_t)height * 3u;
    uint8_t *rgb = (uint8_t *)malloc(rgb_size);
    FILE *raw_fp;
    FILE *ppm_fp;
    int x;
    int y;

    if (!rgb) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            float u = (float)x / (float)(width - 1);
            float v = (float)y / (float)(height - 1);
            float shore = shore_line(u);
            Color c;
            size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 3u;

            if (v < shore) {
                c = color_upper_scene(u, v);
            } else if (v < 0.88f) {
                c = color_lake(u, v);
            } else {
                c = color_foreground(u, v);
            }

            rgb[idx + 0] = to_u8(c.r);
            rgb[idx + 1] = to_u8(c.g);
            rgb[idx + 2] = to_u8(c.b);
        }
    }

    raw_fp = fopen("raw_samples/landscape_challenge_reference_rgb24_640x480.raw", "wb");
    if (!raw_fp) {
        free(rgb);
        fprintf(stderr, "Failed to open raw output\n");
        return 1;
    }
    fwrite(rgb, 1, rgb_size, raw_fp);
    fclose(raw_fp);

    ppm_fp = fopen("raw_samples/landscape_challenge_reference.ppm", "wb");
    if (!ppm_fp) {
        free(rgb);
        fprintf(stderr, "Failed to open ppm output\n");
        return 1;
    }
    fprintf(ppm_fp, "P6\n%d %d\n255\n", width, height);
    fwrite(rgb, 1, rgb_size, ppm_fp);
    fclose(ppm_fp);

    free(rgb);
    printf("Wrote raw_samples/landscape_challenge_reference_rgb24_640x480.raw\n");
    printf("Wrote raw_samples/landscape_challenge_reference.ppm\n");
    return 0;
}
