#ifndef H264_MP4_MUXER_H
#define H264_MP4_MUXER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * h264_mp4_muxer.h
 *
 * 这是这个小工程对外暴露的接口文件。你可以先看这个文件，理解库的用法：
 * 1. h264_mp4_probe_file()：快速读取裸 H.264 的基本信息。
 * 2. h264_mp4_mux_file()：把 Annex-B 裸 H.264 封装成 MP4 文件。
 *
 * 这里没有使用 FFmpeg。MP4 box、H.264 NALU 解析、SPS/PPS 解析都在本工程中实现。
 */

typedef enum h264_mp4_status {
    /* 成功。 */
    H264_MP4_OK = 0,
    /* 参数错误，比如空指针，或者 fps_num/fps_den 只填了一个。 */
    H264_MP4_ERR_INVALID_ARG = -1,
    /* 输入 H.264 文件打开失败。 */
    H264_MP4_ERR_OPEN_INPUT = -2,
    /* 输出 MP4 文件打开失败。 */
    H264_MP4_ERR_OPEN_OUTPUT = -3,
    /* 读取输入文件失败。 */
    H264_MP4_ERR_READ = -4,
    /* 写入输出文件失败。 */
    H264_MP4_ERR_WRITE = -5,
    /* 没有找到 SPS。SPS 里通常保存宽高、profile、level、帧率等信息。 */
    H264_MP4_ERR_NO_SPS = -6,
    /* 没有找到 PPS。PPS 是 H.264 解码也需要的参数集。 */
    H264_MP4_ERR_NO_PPS = -7,
    /* 没有找到视频帧，也就是没有 slice NALU。 */
    H264_MP4_ERR_NO_SAMPLES = -8,
    /* 当前轻量实现暂不支持的码流或文件规格。 */
    H264_MP4_ERR_UNSUPPORTED = -9,
    /* 内存分配失败。 */
    H264_MP4_ERR_OUT_OF_MEMORY = -10,
    /* 没有从 SPS VUI timing 读到帧率，并且用户也没有手动传入帧率。 */
    H264_MP4_ERR_NO_TIMING = -11
} h264_mp4_status_t;

typedef struct h264_mp4_config {
    /*
     * 输出帧率，用分数表示。
     *
     * 例如：
     *   25fps       => fps_num = 25,    fps_den = 1
     *   29.97fps    => fps_num = 30000, fps_den = 1001
     *   60fps       => fps_num = 60,    fps_den = 1
     *
     * 如果 fps_num = 0 且 fps_den = 0，库会尝试从 SPS 的 VUI timing 自动读取。
     */
    unsigned int fps_num;
    unsigned int fps_den;

    /*
     * 视频宽高。
     * 填 0 表示让库从 SPS 中解析。一般推荐填 0，避免手动传错。
     */
    unsigned int width;
    unsigned int height;

    /*
     * MP4 track 的时间基。
     * 视频里常用 90000，也就是 1 秒等于 90000 个时间单位。
     */
    unsigned int timescale;
} h264_mp4_config_t;

typedef struct h264_mp4_info {
    /* 从 SPS 探测出来的图像宽高。 */
    unsigned int width;
    unsigned int height;
    /* 从 SPS VUI timing 探测出来的帧率。没有 timing 时为 0/0。 */
    unsigned int fps_num;
    unsigned int fps_den;
    /* H.264 profile_idc，例如 100 通常表示 High Profile。 */
    unsigned int profile_idc;
    /* H.264 level_idc，例如 52 表示 Level 5.2。 */
    unsigned int level_idc;
    /*
     * 快速探测到的帧数。
     * 为了兼容小内存和低 IO 平台，probe 找到 SPS、PPS、至少 1 帧后会尽早停止，
     * 所以这里不是完整总帧数，只表示“至少确认有多少帧”。
     */
    unsigned int frame_count;
    /* 是否找到 SPS。 */
    unsigned int has_sps;
    /* 是否找到 PPS。 */
    unsigned int has_pps;
} h264_mp4_info_t;

/*
 * 快速探测 Annex-B 裸 H.264 文件。
 *
 * 这个函数不会生成 MP4，只读取必要的 NALU 来得到：
 * - 宽高
 * - profile/level
 * - 帧率（如果 SPS 里有 VUI timing）
 * - 是否至少存在一帧视频数据
 *
 * 为了适配小内存平台，它不会把整个文件读入内存，也不会为了统计完整帧数读取全部码流。
 */
h264_mp4_status_t h264_mp4_probe_file(const char *h264_path,
                                      h264_mp4_info_t *info);

/*
 * 把 Annex-B 裸 H.264 封装成 MP4。
 *
 * 输入文件格式要求：
 * - 每个 NALU 前有 00 00 01 或 00 00 00 01 起始码。
 * - 码流里必须包含 SPS 和 PPS。
 *
 * 输出 MP4 的基本结构：
 * - ftyp：声明文件类型。
 * - mdat：保存真正的视频 NALU 数据。
 * - moov：保存播放器需要的索引、时间、宽高、SPS/PPS 等元数据。
 */
h264_mp4_status_t h264_mp4_mux_file(const char *h264_path,
                                    const char *mp4_path,
                                    const h264_mp4_config_t *config);

/* 把错误码转换成便于打印的人类可读字符串。 */
const char *h264_mp4_status_string(h264_mp4_status_t status);

#ifdef __cplusplus
}
#endif

#endif