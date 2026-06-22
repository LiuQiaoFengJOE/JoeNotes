#include "h264_mp4_muxer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 这个示例程序演示完整调用流程：
 *
 * 1. 先调用 h264_mp4_probe_file() 快速读取 H.264 规格。
 * 2. 如果命令行指定了 fps，就使用用户传入的 fps。
 * 3. 如果没有指定 fps，就使用 SPS VUI timing 中探测到的 fps。
 * 4. 调用 h264_mp4_mux_file() 生成 MP4。
 */

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s input.h264 output.mp4 [fps|fps_num/fps_den]\n"
            "Example: %s output.h264 output.mp4\n"
            "Example: %s output.h264 output.mp4 60\n"
            "Example: %s output.h264 output.mp4 30000/1001\n",
            prog, prog, prog, prog);
}

static int parse_fps(const char *text, unsigned int *num, unsigned int *den)
{
    /*
     * 支持两种帧率输入方式：
     * - 60
     * - 30000/1001
     */
    const char *slash = strchr(text, '/');
    int n;
    int d = 1;

    if (slash) {
        n = atoi(text);
        d = atoi(slash + 1);
    } else {
        n = atoi(text);
    }

    if (n <= 0 || d <= 0) {
        return 0;
    }
    *num = (unsigned int)n;
    *den = (unsigned int)d;
    return 1;
}

static void print_info(const char *path, const h264_mp4_info_t *info)
{
    /* 打印 probe 结果，方便确认输入 H.264 的真实规格。 */
    printf("Input: %s\n", path);
    printf("  size: %ux%u\n", info->width, info->height);
    printf("  profile_idc: %u\n", info->profile_idc);
    printf("  level_idc: %u\n", info->level_idc);
    printf("  frames: at least %u (fast probe stops early)\n",
           info->frame_count);
    if (info->fps_num && info->fps_den) {
        printf("  fps: %u/%u\n", info->fps_num, info->fps_den);
    } else {
        printf("  fps: not present in SPS VUI timing\n");
    }
}

int main(int argc, char **argv)
{
    h264_mp4_config_t cfg;
    h264_mp4_info_t info;
    h264_mp4_status_t st;

    /* 至少需要输入文件和输出文件两个参数。 */
    if (argc < 3 || argc > 4) {
        print_usage(argv[0]);
        return 2;
    }

    /*
     * 0/0 表示“自动从 SPS VUI timing 读取帧率”。
     * width/height 也填 0，表示自动从 SPS 读取。
     */
    cfg.fps_num = 0;
    cfg.fps_den = 0;
    cfg.width = 0;
    cfg.height = 0;
    cfg.timescale = 90000;

    /* 第一步：快速探测 H.264 裸码流规格。 */
    st = h264_mp4_probe_file(argv[1], &info);
    if (st != H264_MP4_OK) {
        fprintf(stderr, "h264_mp4_probe_file failed: %s (%d)\n",
                h264_mp4_status_string(st), (int)st);
        return 1;
    }
    print_info(argv[1], &info);

    if (argc == 4) {
        /* 用户显式传了 fps，就优先使用用户传入值。 */
        if (!parse_fps(argv[3], &cfg.fps_num, &cfg.fps_den)) {
            fprintf(stderr, "Invalid fps: %s\n", argv[3]);
            return 2;
        }
        printf("Using fps override: %u/%u\n", cfg.fps_num, cfg.fps_den);
    } else if (info.fps_num == 0 || info.fps_den == 0) {
        /* 没有探测到 fps 时，不能盲目封装，否则容易出现慢动作或快动作。 */
        fprintf(stderr, "No SPS VUI timing found. Please pass fps explicitly.\n");
        return 2;
    } else {
        /* 正常情况：使用 probe 从 SPS VUI timing 里读到的帧率。 */
        printf("Using probed fps: %u/%u\n", info.fps_num, info.fps_den);
    }

    /* 第二步：真正执行 H.264 -> MP4 封装。 */
    st = h264_mp4_mux_file(argv[1], argv[2], &cfg);
    if (st != H264_MP4_OK) {
        fprintf(stderr, "h264_mp4_mux_file failed: %s (%d)\n",
                h264_mp4_status_string(st), (int)st);
        return 1;
    }

    printf("Wrote %s\n", argv[2]);
    return 0;
}