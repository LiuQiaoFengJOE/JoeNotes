#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Color {
    float r;
    float g;
    float b;
} Color;

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
        x *= 2.02f;
        y *= 2.07f;
        amplitude *= 0.5f;
    }
    return value;
}

static float hill_line(float u)
{
    return 0.58f
         + 0.03f * sinf(u * 6.2f + 0.4f)
         + 0.02f * sinf(u * 14.0f - 0.8f)
         + 0.02f * fbm(u * 2.0f, 0.5f);
}

static float meadow_line(float u)
{
    return 0.80f
         + 0.01f * sinf(u * 8.0f - 0.3f)
         + 0.008f * fbm(u * 4.0f, 1.2f);
}

static float segment_distance(float px, float py, float ax, float ay, float bx, float by)
{
    float abx = bx - ax;
    float aby = by - ay;
    float apx = px - ax;
    float apy = py - ay;
    float ab2 = abx * abx + aby * aby;
    float t = 0.0f;
    float dx;
    float dy;

    if (ab2 > 0.0f)
        t = clamp01((apx * abx + apy * aby) / ab2);

    dx = px - (ax + abx * t);
    dy = py - (ay + aby * t);
    return sqrtf(dx * dx + dy * dy);
}

static float circle_mask(float u, float v, float cx, float cy, float radius, float feather)
{
    float dx = u - cx;
    float dy = v - cy;
    float d = sqrtf(dx * dx + dy * dy);
    return 1.0f - smoothstepf(radius, radius + feather, d);
}

static float box_mask(float u, float v, float x0, float y0, float x1, float y1, float feather)
{
    float mx = smoothstepf(x0 - feather, x0 + feather, u) * (1.0f - smoothstepf(x1 - feather, x1 + feather, u));
    float my = smoothstepf(y0 - feather, y0 + feather, v) * (1.0f - smoothstepf(y1 - feather, y1 + feather, v));
    return mx * my;
}

static float diamond_mask(float u, float v, float cx, float cy, float rx, float ry, float feather)
{
    float d = fabsf((u - cx) / rx) + fabsf((v - cy) / ry);
    return 1.0f - smoothstepf(1.0f, 1.0f + feather, d);
}

static float line_mask(float u, float v, float ax, float ay, float bx, float by, float radius)
{
    float d = segment_distance(u, v, ax, ay, bx, by);
    return 1.0f - smoothstepf(radius, radius + radius * 0.7f, d);
}

static Color color_sky(float u, float v)
{
    Color top = { 0.17f, 0.40f, 0.92f };
    Color horizon = { 0.62f, 0.84f, 0.98f };
    Color cloud = { 0.95f, 0.98f, 1.00f };
    Color warm = { 0.92f, 0.97f, 1.00f };
    float t = smoothstepf(0.0f, 0.62f, v / 0.62f);
    float cloud_field = fbm(u * 7.0f + 1.2f, v * 14.0f - 2.5f);
    float cloud_mask = smoothstepf(0.68f, 0.84f, cloud_field) * (1.0f - smoothstepf(0.40f, 0.95f, v));
    float sun_dx = u - 0.80f;
    float sun_dy = v - 0.18f;
    float sun_d = sqrtf(sun_dx * sun_dx + sun_dy * sun_dy);
    float glow = expf(-(sun_d * sun_d) / 0.014f);
    float core = expf(-(sun_d * sun_d) / 0.0014f);
    Color c = mixc(top, horizon, t);

    c = mixc(c, cloud, cloud_mask * 0.35f);
    c = mixc(c, warm, glow * 0.22f);
    c = mixc(c, (Color){ 1.0f, 0.98f, 0.84f }, core * 0.9f);
    return c;
}

static Color color_hills(float u, float v)
{
    float hill = hill_line(u);
    Color c = color_sky(u, v);

    if (v > hill && v < 0.80f) {
        float shade = clamp01(0.18f + 0.82f * (0.9f - v) + 0.18f * sinf(u * 7.0f));
        Color dark = { 0.18f, 0.44f, 0.19f };
        Color light = { 0.42f, 0.66f, 0.28f };
        Color hill_color = mixc(dark, light, shade);
        float flowers = smoothstepf(0.77f, 0.96f, fbm(u * 22.0f, v * 30.0f));
        hill_color = mixc(hill_color, (Color){ 0.88f, 0.82f, 0.34f }, flowers * 0.08f);
        c = hill_color;
    }

    return c;
}

static Color color_meadow(float u, float v)
{
    float grass = fbm(u * 35.0f, v * 55.0f);
    float depth = smoothstepf(0.80f, 1.0f, v);
    Color base = mixc((Color){ 0.10f, 0.38f, 0.08f }, (Color){ 0.28f, 0.64f, 0.12f }, grass);
    Color sun = { 0.44f, 0.70f, 0.18f };
    float path = fabsf(u - (0.48f + 0.05f * sinf(v * 20.0f)));
    float path_mask = smoothstepf(0.13f, 0.03f, path) * depth;
    float flowers = smoothstepf(0.84f, 0.96f, fbm(u * 30.0f + 2.0f, v * 45.0f - 1.5f));

    base = mixc(base, sun, smoothstepf(0.0f, 0.85f, 1.0f - depth) * 0.18f);
    base = mixc(base, (Color){ 0.58f, 0.48f, 0.18f }, path_mask * 0.32f);
    base = mixc(base, (Color){ 0.96f, 0.82f, 0.24f }, flowers * 0.10f);
    return base;
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
            float hill = hill_line(u);
            float meadow = meadow_line(u);
            Color c;
            size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 3u;

            if (v < hill)
                c = color_sky(u, v);
            else if (v < meadow)
                c = color_hills(u, v);
            else
                c = color_meadow(u, v);

            if (v < hill) {
                float cloud1 = circle_mask(u, v, 0.18f, 0.21f, 0.05f, 0.02f);
                float cloud2 = circle_mask(u, v, 0.25f, 0.19f, 0.045f, 0.02f);
                float cloud3 = circle_mask(u, v, 0.31f, 0.22f, 0.04f, 0.02f);
                float cloud = clamp01(cloud1 + cloud2 + cloud3);
                c = mixc(c, (Color){ 0.98f, 0.99f, 1.0f }, cloud * 0.45f);
            }

            {
                float kite = diamond_mask(u, v, 0.30f, 0.22f, 0.045f, 0.060f, 0.08f);
                float kite_cross1 = line_mask(u, v, 0.26f, 0.22f, 0.34f, 0.22f, 0.0022f);
                float kite_cross2 = line_mask(u, v, 0.30f, 0.16f, 0.30f, 0.28f, 0.0022f);
                float tail = line_mask(u, v, 0.30f, 0.28f, 0.36f, 0.42f, 0.0018f);
                float bow1 = diamond_mask(u, v, 0.322f, 0.335f, 0.010f, 0.012f, 0.18f);
                float bow2 = diamond_mask(u, v, 0.345f, 0.388f, 0.009f, 0.011f, 0.18f);
                float bow3 = diamond_mask(u, v, 0.364f, 0.434f, 0.008f, 0.010f, 0.18f);

                c = mixc(c, (Color){ 0.86f, 0.08f, 0.10f }, kite);
                c = mixc(c, (Color){ 0.92f, 0.78f, 0.20f }, kite_cross1 * 0.9f);
                c = mixc(c, (Color){ 0.92f, 0.78f, 0.20f }, kite_cross2 * 0.9f);
                c = mixc(c, (Color){ 0.98f, 0.96f, 0.92f }, tail * 0.80f);
                c = mixc(c, (Color){ 0.96f, 0.92f, 0.82f }, bow1 + bow2 + bow3);
            }

            {
                float body = box_mask(u, v, 0.62f, 0.66f, 0.655f, 0.78f, 0.004f);
                float head = circle_mask(u, v, 0.637f, 0.63f, 0.022f, 0.01f);
                float left_leg = line_mask(u, v, 0.635f, 0.78f, 0.620f, 0.90f, 0.0035f);
                float right_leg = line_mask(u, v, 0.641f, 0.78f, 0.655f, 0.90f, 0.0035f);
                float left_arm = line_mask(u, v, 0.628f, 0.70f, 0.603f, 0.76f, 0.0032f);
                float right_arm = line_mask(u, v, 0.650f, 0.69f, 0.690f, 0.60f, 0.0030f);
                float shirt = box_mask(u, v, 0.621f, 0.665f, 0.654f, 0.735f, 0.004f);
                float pants = box_mask(u, v, 0.622f, 0.735f, 0.654f, 0.79f, 0.004f);
                float skin = head + line_mask(u, v, 0.650f, 0.69f, 0.665f, 0.655f, 0.0030f);

                c = mixc(c, (Color){ 0.96f, 0.80f, 0.62f }, skin);
                c = mixc(c, (Color){ 0.98f, 0.86f, 0.18f }, shirt * 0.95f);
                c = mixc(c, (Color){ 0.18f, 0.34f, 0.76f }, pants * 0.95f);
                c = mixc(c, (Color){ 0.20f, 0.14f, 0.10f }, body + left_leg + right_leg + left_arm + right_arm);
            }

            {
                float string1 = line_mask(u, v, 0.690f, 0.60f, 0.55f, 0.45f, 0.0012f);
                float string2 = line_mask(u, v, 0.55f, 0.45f, 0.30f, 0.28f, 0.0012f);
                c = mixc(c, (Color){ 0.94f, 0.94f, 0.92f }, (string1 + string2) * 0.9f);
            }

            rgb[idx + 0] = to_u8(c.r);
            rgb[idx + 1] = to_u8(c.g);
            rgb[idx + 2] = to_u8(c.b);
        }
    }

    raw_fp = fopen("kite_challenge_reference_rgb24_640x480.raw", "wb");
    if (!raw_fp) {
        free(rgb);
        fprintf(stderr, "Failed to open raw output\n");
        return 1;
    }
    fwrite(rgb, 1, rgb_size, raw_fp);
    fclose(raw_fp);

    ppm_fp = fopen("kite_challenge_reference.ppm", "wb");
    if (!ppm_fp) {
        free(rgb);
        fprintf(stderr, "Failed to open ppm output\n");
        return 1;
    }
    fprintf(ppm_fp, "P6\n%d %d\n255\n", width, height);
    fwrite(rgb, 1, rgb_size, ppm_fp);
    fclose(ppm_fp);

    free(rgb);
    printf("Wrote kite_challenge_reference_rgb24_640x480.raw\n");
    printf("Wrote kite_challenge_reference.ppm\n");
    return 0;
}
