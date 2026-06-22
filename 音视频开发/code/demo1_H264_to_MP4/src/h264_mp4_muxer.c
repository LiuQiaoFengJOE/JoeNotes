#include "h264_mp4_muxer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define H264_MP4_DEFAULT_TIMESCALE 90000U
#define H264_MP4_MAX_PARAM_SET 65535U
#define H264_MP4_MAX_BOX_SIZE 0xffffffffUL



/*
 * h264_mp4_muxer.c 阅读路线：
 *
 * 1. 先看 H.264 Annex-B 读取：reader_next_nalu()。
 *    裸 H.264 用 00 00 01 或 00 00 00 01 起始码分隔 NALU。
 *
 * 2. 再看 SPS 解析：parse_sps_info()。
 *    SPS 里能解析出宽高、profile、level，有些码流还带 VUI timing 帧率。
 *
 * 3. 然后看 MP4 数据写入：process_stream_file() 和 append_nal_to_mdat()。
 *    MP4 里的 H.264 NALU 不再使用起始码，而是写成 4 字节长度 + NALU 数据。
 *
 * 4. 最后看 MP4 索引写入：build_moov() 以及 append_stsd/stts/stsz/stco 等函数。
 *    播放器依靠 moov 里的这些表找到每帧位置、大小、时间和解码参数。
 */
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;


/* 动态字节数组。用于保存 SPS/PPS、临时 NALU、moov box 等可变长度数据。 */
typedef struct byte_buffer {
    u8 *data;
    size_t size;
    size_t cap;
    int failed;
} byte_buffer_t;


/* MP4 中一帧视频称为一个 sample。这里记录每个 sample 在 mdat 中的位置和大小。 */
typedef struct sample_info {
    u32 offset;
    u32 size;
    int is_sync;
} sample_info_t;


/* sample 表。写 stsz、stco、stss 这些 MP4 索引 box 时会用到。 */
typedef struct sample_table {
    sample_info_t *items;
    size_t count;
    size_t cap;
} sample_table_t;


/* 一次封装过程的上下文。它保存输出文件、SPS/PPS、帧表、宽高、帧率和 mdat 状态。 */
typedef struct mp4_writer {
    
    FILE *out;
    
    byte_buffer_t sps;
    byte_buffer_t pps;
    
    sample_table_t samples;
    
    u32 width;
    u32 height;
    u32 fps_num;
    u32 fps_den;
    u32 reorder_frames;
    u32 chroma_format_idc;
    u32 bit_depth_luma_minus8;
    u32 bit_depth_chroma_minus8;
    
    u32 timescale;
    u32 sample_delta;
    
    u32 mdat_start;
    u32 mdat_payload_size;
    
    int wrote_vcl;
} mp4_writer_t;


/* H.264 SPS 使用 bit 级字段和 Exp-Golomb 编码，不能按普通结构体直接读取。 */
typedef struct bit_reader {
    const u8 *data;
    size_t size;
    size_t bit_pos;
} bit_reader_t;


/* 流式 NALU 读取器。只保存少量状态，适合小内存平台，不会把整个 H.264 文件读入内存。 */
typedef struct h264_nalu_reader {
    FILE *fp;
    int started;
    int done;
} h264_nalu_reader_t;


/* SPS 解析结果。 */
typedef struct h264_sps_info {
    u32 width;
    u32 height;
    u32 fps_num;
    u32 fps_den;
    u32 profile_idc;
    u32 level_idc;
    u32 reorder_frames;
    u32 chroma_format_idc;
    u32 bit_depth_luma_minus8;
    u32 bit_depth_chroma_minus8;
} h264_sps_info_t;

/* 当前正在写入的 MP4 sample。这里只记录 offset/size，不缓存整帧数据。 */
typedef struct pending_sample {
    u32 offset;
    u32 size;
    int has_data;
    int is_sync;
} pending_sample_t;

/* 释放动态数组，并清零字段，避免调用方继续看到旧指针和旧长度。 */
static void buffer_free(byte_buffer_t *buf)
{
    
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->cap = 0;
    buf->failed = 0;
}

/* 确保动态数组至少还能追加 add 字节；内部会做 size_t 溢出检查和 realloc 扩容。 */
static int buffer_reserve(byte_buffer_t *buf, size_t add)
{
    
    size_t need;
    size_t new_cap;
    u8 *new_data;

    if (!buf || add > (size_t)-1 - buf->size) {
        if (buf) {
            buf->failed = 1;
        }
        return 0;
    }
    need = buf->size + add;
    if (need <= buf->cap) {
        return 1;
    }

    new_cap = buf->cap ? buf->cap : 4096;
    while (new_cap < need) {
        if (new_cap > ((size_t)-1) / 2) {
            return 0;
        }
        new_cap *= 2;
    }

    new_data = (u8 *)realloc(buf->data, new_cap);
    if (!new_data) {
        buf->failed = 1;
        return 0;
    }

    buf->data = new_data;
    buf->cap = new_cap;
    return 1;
}

/* 向动态数组末尾追加一段数据。返回 0 表示内存不足或参数错误。 */
static int buffer_append(byte_buffer_t *buf, const void *data, size_t size)
{
    
    if (!buf || (!data && size > 0)) {
        if (buf) {
            buf->failed = 1;
        }
        return 0;
    }
    if (!buffer_reserve(buf, size)) {
        return 0;
    }
    memcpy(buf->data + buf->size, data, size);
    buf->size += size;
    return 1;
}

/* 向 sample 表增加一帧记录。每帧的 offset/size 后面会写入 stco/stsz。 */
static int table_add(sample_table_t *table, u32 offset, u32 size, int is_sync)
{
    
    sample_info_t *new_items;
    size_t new_cap;

    if (table->count == table->cap) {
        new_cap = table->cap ? table->cap * 2 : 256;
        if (new_cap < table->cap ||
            new_cap > (size_t)-1 / sizeof(sample_info_t)) {
            return 0;
        }
        new_items = (sample_info_t *)realloc(table->items,
                                             new_cap * sizeof(sample_info_t));
        if (!new_items) {
            return 0;
        }
        table->items = new_items;
        table->cap = new_cap;
    }

    table->items[table->count].offset = offset;
    table->items[table->count].size = size;
    table->items[table->count].is_sync = is_sync;
    table->count++;
    return 1;
}

/* 释放 sample 表。 */
static void table_free(sample_table_t *table)
{
    
    free(table->items);
    table->items = NULL;
    table->count = 0;
    table->cap = 0;
}

/* MP4 文件中的整数使用大端序，所以需要手动拆字节写入。 */
static void be16(byte_buffer_t *buf, u32 v)
{
    
    u8 b[2];
    b[0] = (u8)(v >> 8);
    b[1] = (u8)v;
    (void)buffer_append(buf, b, sizeof(b));
}

/* 写 32 位大端整数到内存 buffer。 */
static void be32(byte_buffer_t *buf, u32 v)
{
    
    u8 b[4];
    b[0] = (u8)(v >> 24);
    b[1] = (u8)(v >> 16);
    b[2] = (u8)(v >> 8);
    b[3] = (u8)v;
    (void)buffer_append(buf, b, sizeof(b));
}

/* 向内存 buffer 追加原始字节，常用于写 box type，例如 "moov"。 */
static void put_bytes(byte_buffer_t *buf, const void *data, size_t size)
{
    
    (void)buffer_append(buf, data, size);
}

/* 写 MP4 标准要求的保留 0 字段。 */
static void put_zeros(byte_buffer_t *buf, size_t count)
{
    
    const u8 zeros[32] = {0};
    while (count > 0) {
        size_t n = count > sizeof(zeros) ? sizeof(zeros) : count;
        put_bytes(buf, zeros, n);
        count -= n;
    }
}

/*
 * 开始写一个 MP4 box。
 * MP4 box 基本格式是：4 字节 size + 4 字节 type + payload。
 * 开始时还不知道最终 size，所以先写 0，box_end() 时再回填。
 */
static size_t box_begin(byte_buffer_t *buf, const char type[4])
{
    
    size_t pos = buf->size;
    be32(buf, 0);
    put_bytes(buf, type, 4);
    return pos;
}

/* 结束一个 MP4 box，计算 box 大小，并把 size 回填到 box 开头。 */
static void box_end(byte_buffer_t *buf, size_t pos)
{
    
    u32 size = (u32)(buf->size - pos);
    buf->data[pos + 0] = (u8)(size >> 24);
    buf->data[pos + 1] = (u8)(size >> 16);
    buf->data[pos + 2] = (u8)(size >> 8);
    buf->data[pos + 3] = (u8)size;
}

/* 直接向文件写 32 位大端整数，主要用于写 mdat size 和 NALU length。 */
static int file_write_be32(FILE *fp, u32 v)
{
    
    u8 b[4];
    b[0] = (u8)(v >> 24);
    b[1] = (u8)(v >> 16);
    b[2] = (u8)(v >> 8);
    b[3] = (u8)v;
    return fwrite(b, 1, sizeof(b), fp) == sizeof(b);
}

/* 获取当前文件偏移。stco box 需要记录每帧在 MP4 文件中的位置。 */
static long file_tell(FILE *fp)
{
    
    return ftell(fp);
}

/* 跳转到指定文件偏移。mdat 的 size 需要等数据写完后再回填。 */
static int file_seek(FILE *fp, long off)
{
    
    return fseek(fp, off, SEEK_SET) == 0;
}


/* 在 H.264 Annex-B 文件流中查找下一个起始码：00 00 01 或 00 00 00 01。 */
static int reader_find_start_code(FILE *fp, size_t *prefix)
{
    int c;
    unsigned int zero_count = 0;

    while ((c = fgetc(fp)) != EOF) {
        if (c == 0) {
            zero_count++;
            continue;
        }
        if (c == 1 && zero_count >= 2) {
            *prefix = zero_count > 2 ? 4U : 3U;
            return 1;
        }
        zero_count = 0;
    }

    return 0;
}

/* 打开 H.264 输入文件，并初始化流式 NALU 读取器。 */
static h264_mp4_status_t reader_open(h264_nalu_reader_t *r,
                                     const char *path)
{
    if (!r || !path) {
        return H264_MP4_ERR_INVALID_ARG;
    }
    memset(r, 0, sizeof(*r));
    r->fp = fopen(path, "rb");
    if (!r->fp) {
        return H264_MP4_ERR_OPEN_INPUT;
    }
    return H264_MP4_OK;
}

/* 关闭流式 NALU 读取器持有的文件。 */
static void reader_close(h264_nalu_reader_t *r)
{
    if (r && r->fp) {
        fclose(r->fp);
        r->fp = NULL;
    }
}

/*
 * 读取下一个 NALU。
 * 返回的 nalbuf 不包含 Annex-B 起始码，只包含 NAL header + NAL payload。
 * 这个函数每次只保留当前 NALU，适合小内存平台。
 */
static h264_mp4_status_t reader_next_nalu(h264_nalu_reader_t *r,
                                          byte_buffer_t *nal,
                                          int *got_nal)
{
    int c;
    unsigned int zero_count;
    size_t prefix;

    if (!r || !r->fp || !nal || !got_nal) {
        return H264_MP4_ERR_INVALID_ARG;
    }

    *got_nal = 0;
    nal->size = 0;

    if (r->done) {
        return H264_MP4_OK;
    }

    if (!r->started) {
        if (!reader_find_start_code(r->fp, &prefix)) {
            if (ferror(r->fp)) {
                return H264_MP4_ERR_READ;
            }
            r->done = 1;
            return H264_MP4_OK;
        }
        r->started = 1;
    }

    zero_count = 0;
    while ((c = fgetc(r->fp)) != EOF) {
        unsigned char byte = (unsigned char)c;

        if (!buffer_append(nal, &byte, 1)) {
            return H264_MP4_ERR_OUT_OF_MEMORY;
        }

        if (byte == 0) {
            zero_count++;
            continue;
        }

        if (byte == 1 && zero_count >= 2) {
            size_t remove = zero_count > 2 ? 4U : 3U;
            if (nal->size >= remove) {
                nal->size -= remove;
            } else {
                nal->size = 0;
            }
            while (nal->size > 0 && nal->data[nal->size - 1] == 0) {
                nal->size--;
            }
            if (nal->size > 0) {
                *got_nal = 1;
                return H264_MP4_OK;
            }
            zero_count = 0;
            continue;
        }

        zero_count = 0;
    }

    if (ferror(r->fp)) {
        return H264_MP4_ERR_READ;
    }

    r->done = 1;
    while (nal->size > 0 && nal->data[nal->size - 1] == 0) {
        nal->size--;
    }
    *got_nal = nal->size > 0;
    return H264_MP4_OK;
}

/*
 * H.264 为了避免码流内部误出现起始码，会插入 emulation prevention byte 0x03。
 * 解析 SPS/slice 前要把 00 00 03 中的 03 去掉，EBSP 才能变成真正的 RBSP。
 */
static int rbsp_from_ebsp(const u8 *src, size_t src_size, byte_buffer_t *dst)
{
    
    size_t i;
    int zero_count = 0;

    dst->size = 0;
    for (i = 0; i < src_size; i++) {
        if (zero_count >= 2 && src[i] == 0x03) {
            zero_count = 0;
            continue;
        }
        if (!buffer_append(dst, src + i, 1)) {
            return 0;
        }
        if (src[i] == 0) {
            zero_count++;
        } else {
            zero_count = 0;
        }
    }
    return 1;
}

/* 从 bit_reader 中读取 1 bit。 */
static u32 br_read_bit(bit_reader_t *br)
{
    
    u32 bit;
    if (br->bit_pos >= br->size * 8) {
        return 0;
    }
    bit = (br->data[br->bit_pos / 8] >> (7 - (br->bit_pos % 8))) & 1U;
    br->bit_pos++;
    return bit;
}

/* 从 bit_reader 中读取 1 bit。 */
/* 连续读取 count 个 bit，并拼成一个整数。 */
static u32 br_read_bits(bit_reader_t *br, unsigned int count)
{
    
    u32 v = 0;
    while (count--) {
        v = (v << 1) | br_read_bit(br);
    }
    return v;
}

/* 读取 H.264 常用的无符号 Exp-Golomb 编码值。 */
static u32 br_read_ue(bit_reader_t *br)
{
    
    unsigned int zeros = 0;
    while (br->bit_pos < br->size * 8 && br_read_bit(br) == 0) {
        zeros++;
        if (zeros > 31) {
            return 0;
        }
    }
    return ((1U << zeros) - 1U) + br_read_bits(br, zeros);
}

/* 读取 H.264 常用的有符号 Exp-Golomb 编码值。 */
static int br_read_se(bit_reader_t *br)
{
    
    u32 ue = br_read_ue(br);
    int v = (int)((ue + 1U) / 2U);
    return (ue & 1U) ? v : -v;
}

/* 最大公约数，用于把帧率分数约简，例如 60000/1000 约成 60/1。 */
static u32 gcd_u32(u32 a, u32 b)
{
    
    while (b != 0) {
        u32 t = a % b;
        a = b;
        b = t;
    }
    return a ? a : 1U;
}

/* SPS 中可能存在 scaling list。这里不使用它，但必须按规范跳过，否则后续字段会读错位。 */
static void skip_scaling_list(bit_reader_t *br, int count)
{
    
    int last_scale = 8;
    int next_scale = 8;
    int j;

    for (j = 0; j < count; j++) {
        if (next_scale != 0) {
            int delta_scale = br_read_se(br);
            next_scale = (last_scale + delta_scale + 256) % 256;
        }
        last_scale = (next_scale == 0) ? last_scale : next_scale;
    }
}

/* 解析 SPS 中的 VUI 信息。这里最重要的是 timing_info，它可以给出原始帧率。 */
static void parse_vui_parameters(bit_reader_t *br, h264_sps_info_t *info)
{
    
    u32 aspect_ratio_info_present_flag;
    u32 overscan_info_present_flag;
    u32 video_signal_type_present_flag;
    u32 chroma_loc_info_present_flag;
    u32 timing_info_present_flag;
    u32 nal_hrd_parameters_present_flag;
    u32 vcl_hrd_parameters_present_flag;

    aspect_ratio_info_present_flag = br_read_bit(br);
    if (aspect_ratio_info_present_flag) {
        u32 aspect_ratio_idc = br_read_bits(br, 8);
        if (aspect_ratio_idc == 255) {
            (void)br_read_bits(br, 16);
            (void)br_read_bits(br, 16);
        }
    }

    overscan_info_present_flag = br_read_bit(br);
    if (overscan_info_present_flag) {
        (void)br_read_bit(br);
    }

    video_signal_type_present_flag = br_read_bit(br);
    if (video_signal_type_present_flag) {
        (void)br_read_bits(br, 3);
        (void)br_read_bit(br);
        if (br_read_bit(br)) {
            (void)br_read_bits(br, 8);
            (void)br_read_bits(br, 8);
            (void)br_read_bits(br, 8);
        }
    }

    chroma_loc_info_present_flag = br_read_bit(br);
    if (chroma_loc_info_present_flag) {
        (void)br_read_ue(br);
        (void)br_read_ue(br);
    }

    timing_info_present_flag = br_read_bit(br);
    if (timing_info_present_flag) {
        
        u32 num_units_in_tick = br_read_bits(br, 32);
        u32 time_scale = br_read_bits(br, 32);
        u32 fixed_frame_rate_flag = br_read_bit(br);
        (void)fixed_frame_rate_flag;
        if (num_units_in_tick != 0 && time_scale != 0) {
            u32 den = num_units_in_tick * 2U;
            u32 g = gcd_u32(time_scale, den);
            info->fps_num = time_scale / g;
            info->fps_den = den / g;
        }
    }

    nal_hrd_parameters_present_flag = br_read_bit(br);
    if (nal_hrd_parameters_present_flag) {
        u32 cpb_cnt_minus1 = br_read_ue(br);
        u32 i;
        (void)br_read_bits(br, 4);
        (void)br_read_bits(br, 4);
        for (i = 0; i <= cpb_cnt_minus1; i++) {
            (void)br_read_ue(br);
            (void)br_read_ue(br);
            (void)br_read_bit(br);
        }
        (void)br_read_bits(br, 5);
        (void)br_read_bits(br, 5);
        (void)br_read_bits(br, 5);
        (void)br_read_bits(br, 5);
    }

    vcl_hrd_parameters_present_flag = br_read_bit(br);
    if (vcl_hrd_parameters_present_flag) {
        u32 cpb_cnt_minus1 = br_read_ue(br);
        u32 i;
        (void)br_read_bits(br, 4);
        (void)br_read_bits(br, 4);
        for (i = 0; i <= cpb_cnt_minus1; i++) {
            (void)br_read_ue(br);
            (void)br_read_ue(br);
            (void)br_read_bit(br);
        }
        (void)br_read_bits(br, 5);
        (void)br_read_bits(br, 5);
        (void)br_read_bits(br, 5);
        (void)br_read_bits(br, 5);
    }

    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
        (void)br_read_bit(br);
    }
    (void)br_read_bit(br);
    if (br_read_bit(br)) {
        (void)br_read_bit(br);
        (void)br_read_ue(br);
        (void)br_read_ue(br);
        (void)br_read_ue(br);
        (void)br_read_ue(br);
        info->reorder_frames = br_read_ue(br);
        (void)br_read_ue(br);
    }
}

/*
 * 解析 SPS NALU。
 * 主要得到：宽高、profile、level、VUI timing 帧率。
 * 注意传入的 sps 包含 H.264 NAL header，所以真正 RBSP 从 sps + 1 开始。
 */
static int parse_sps_info(const u8 *sps, size_t sps_size,
                          h264_sps_info_t *info)
{
    
    byte_buffer_t rbsp;
    bit_reader_t br;
    u32 profile_idc;
    u32 chroma_format_idc = 1;
    u32 bit_depth_luma_minus8 = 0;
    u32 bit_depth_chroma_minus8 = 0;
    u32 pic_order_cnt_type;
    u32 pic_width_in_mbs_minus1;
    u32 pic_height_in_map_units_minus1;
    u32 frame_mbs_only_flag;
    u32 frame_crop_left_offset = 0;
    u32 frame_crop_right_offset = 0;
    u32 frame_crop_top_offset = 0;
    u32 frame_crop_bottom_offset = 0;
    u32 crop_unit_x;
    u32 crop_unit_y;
    u32 i;
    int separate_colour_plane_flag = 0;

    if (!sps || sps_size < 4 || !info) {
        return 0;
    }
    memset(info, 0, sizeof(*info));

    memset(&rbsp, 0, sizeof(rbsp));
    if (!rbsp_from_ebsp(sps + 1, sps_size - 1, &rbsp)) {
        return 0;
    }

    memset(&br, 0, sizeof(br));
    br.data = rbsp.data;
    br.size = rbsp.size;

    profile_idc = br_read_bits(&br, 8);
    info->profile_idc = profile_idc;
    (void)br_read_bits(&br, 8);
    info->level_idc = br_read_bits(&br, 8);
    (void)br_read_ue(&br);

    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
        profile_idc == 244 || profile_idc == 44 || profile_idc == 83 ||
        profile_idc == 86 || profile_idc == 118 || profile_idc == 128 ||
        profile_idc == 138 || profile_idc == 144) {
        
        chroma_format_idc = br_read_ue(&br);
        if (chroma_format_idc == 3) {
            separate_colour_plane_flag = (int)br_read_bit(&br);
        }
        bit_depth_luma_minus8 = br_read_ue(&br);
        bit_depth_chroma_minus8 = br_read_ue(&br);
        (void)br_read_bit(&br);
        if (br_read_bit(&br)) {
            u32 count = (chroma_format_idc != 3) ? 8U : 12U;
            for (i = 0; i < count; i++) {
                if (br_read_bit(&br)) {
                    skip_scaling_list(&br, i < 6 ? 16 : 64);
                }
            }
        }
    }

    (void)br_read_ue(&br);
    pic_order_cnt_type = br_read_ue(&br);
    if (pic_order_cnt_type == 0) {
        
        (void)br_read_ue(&br);
    } else if (pic_order_cnt_type == 1) {
        
        u32 count;
        (void)br_read_bit(&br);
        (void)br_read_se(&br);
        (void)br_read_se(&br);
        count = br_read_ue(&br);
        for (i = 0; i < count; i++) {
            (void)br_read_se(&br);
        }
    }

    (void)br_read_ue(&br);
    (void)br_read_bit(&br);
    pic_width_in_mbs_minus1 = br_read_ue(&br);
    pic_height_in_map_units_minus1 = br_read_ue(&br);
    frame_mbs_only_flag = br_read_bit(&br);
    if (!frame_mbs_only_flag) {
        (void)br_read_bit(&br);
    }
    (void)br_read_bit(&br);

    if (br_read_bit(&br)) {
        
        frame_crop_left_offset = br_read_ue(&br);
        frame_crop_right_offset = br_read_ue(&br);
        frame_crop_top_offset = br_read_ue(&br);
        frame_crop_bottom_offset = br_read_ue(&br);
    }

    if (chroma_format_idc == 0 || separate_colour_plane_flag) {
        crop_unit_x = 1;
        crop_unit_y = 2 - frame_mbs_only_flag;
    } else if (chroma_format_idc == 1) {
        crop_unit_x = 2;
        crop_unit_y = 2 * (2 - frame_mbs_only_flag);
    } else {
        crop_unit_x = 1;
        crop_unit_y = 2 - frame_mbs_only_flag;
    }

    info->width = ((pic_width_in_mbs_minus1 + 1) * 16) -
                  (frame_crop_left_offset + frame_crop_right_offset) * crop_unit_x;
    info->height = ((2 - frame_mbs_only_flag) *
                    (pic_height_in_map_units_minus1 + 1) * 16) -
                   (frame_crop_top_offset + frame_crop_bottom_offset) * crop_unit_y;
    info->chroma_format_idc = chroma_format_idc;
    info->bit_depth_luma_minus8 = bit_depth_luma_minus8;
    info->bit_depth_chroma_minus8 = bit_depth_chroma_minus8;

    if (br_read_bit(&br)) {
        
        parse_vui_parameters(&br, info);
    }

    buffer_free(&rbsp);
    return info->width > 0 && info->height > 0;
}

/* 读取 slice header 的 first_mb_in_slice，用来判断一个新的视频帧是否开始。 */
static u32 read_first_mb_in_slice(const u8 *nal, size_t nal_size)
{
    
    byte_buffer_t rbsp;
    bit_reader_t br;
    u32 first_mb;

    memset(&rbsp, 0, sizeof(rbsp));
    if (!nal || nal_size < 2 || !rbsp_from_ebsp(nal + 1, nal_size - 1, &rbsp)) {
        return 0;
    }
    memset(&br, 0, sizeof(br));
    br.data = rbsp.data;
    br.size = rbsp.size;
    first_mb = br_read_ue(&br);
    buffer_free(&rbsp);
    return first_mb;
}

/* 写 ftyp box。它告诉播放器：这是一个 ISO BMFF/MP4 兼容文件。 */
static h264_mp4_status_t write_ftyp(FILE *out)
{
    
    const u8 ftyp[] = {
        0x00, 0x00, 0x00, 0x20, 'f', 't', 'y', 'p',
        'i', 's', 'o', 'm', 0x00, 0x00, 0x02, 0x00,
        'i', 's', 'o', 'm', 'i', 's', 'o', '2',
        'a', 'v', 'c', '1', 'm', 'p', '4', '1'
    };
    return fwrite(ftyp, 1, sizeof(ftyp), out) == sizeof(ftyp)
        ? H264_MP4_OK : H264_MP4_ERR_WRITE;
}

/* 写 mvhd box。mvhd 是整个 movie 的头，保存总时间基和总时长。 */
static void append_mvhd(byte_buffer_t *moov, u32 timescale, u32 duration)
{
    
    size_t box = box_begin(moov, "mvhd");
    be32(moov, 0);
    be32(moov, 0);
    be32(moov, 0);
    be32(moov, timescale);
    be32(moov, duration);
    be32(moov, 0x00010000);
    be16(moov, 0x0100);
    be16(moov, 0);
    put_zeros(moov, 8);
    be32(moov, 0x00010000); be32(moov, 0); be32(moov, 0);
    be32(moov, 0); be32(moov, 0x00010000); be32(moov, 0);
    be32(moov, 0); be32(moov, 0); be32(moov, 0x40000000);
    put_zeros(moov, 24);
    be32(moov, 2);
    box_end(moov, box);
}

/* 写 tkhd box。tkhd 是视频 track 的头，保存 track id、时长、宽高。 */
static void append_tkhd(byte_buffer_t *trak, u32 track_id, u32 duration,
                        u32 width, u32 height)
{
    
    size_t box = box_begin(trak, "tkhd");
    be32(trak, 0x00000007);
    be32(trak, 0);
    be32(trak, 0);
    be32(trak, track_id);
    be32(trak, 0);
    be32(trak, duration);
    put_zeros(trak, 8);
    be16(trak, 0);
    be16(trak, 0);
    be16(trak, 0);
    be16(trak, 0);
    be32(trak, 0x00010000); be32(trak, 0); be32(trak, 0);
    be32(trak, 0); be32(trak, 0x00010000); be32(trak, 0);
    be32(trak, 0); be32(trak, 0); be32(trak, 0x40000000);
    be32(trak, width << 16);
    be32(trak, height << 16);
    box_end(trak, box);
}

static void append_edts(byte_buffer_t *trak, u32 duration, u32 media_time)
{
    /*
     * edts/elst 是 MP4 的编辑列表。
     *
     * 含 B 帧的 H.264 码流通常需要先解码几帧“预滚帧”，这些帧用于填满
     * 解码器重排队列，但不应该从 0 秒开始显示。SPS VUI 里的
     * num_reorder_frames 告诉我们这种预滚深度。
     *
     * 这里把 media_time 设置为 reorder_frames * sample_delta，相当于告诉
     * 播放器：媒体数据从这个时间点开始作为展示时间线的 0 秒。这样可以避免
     * 结尾因为 B 帧重排补偿而出现卡顿或跳帧。
     */
    size_t edts;
    size_t elst;

    if (media_time == 0) {
        return;
    }

    edts = box_begin(trak, "edts");
    elst = box_begin(trak, "elst");
    be32(trak, 0);
    be32(trak, 1);
    be32(trak, duration);
    be32(trak, media_time);
    be16(trak, 1);
    be16(trak, 0);
    box_end(trak, elst);
    box_end(trak, edts);
}

/* 写 mdhd box。mdhd 是媒体层的头，保存该 track 的时间基和时长。 */
static void append_mdhd(byte_buffer_t *mdia, u32 timescale, u32 duration)
{
    
    size_t box = box_begin(mdia, "mdhd");
    be32(mdia, 0);
    be32(mdia, 0);
    be32(mdia, 0);
    be32(mdia, timescale);
    be32(mdia, duration);
    be16(mdia, 0x55c4);
    be16(mdia, 0);
    box_end(mdia, box);
}

/* 写 hdlr box。这里声明当前 track 的 handler type 是 vide，也就是视频轨。 */
static void append_hdlr(byte_buffer_t *mdia)
{
    
    size_t box = box_begin(mdia, "hdlr");
    be32(mdia, 0);
    be32(mdia, 0);
    put_bytes(mdia, "vide", 4);
    put_zeros(mdia, 12);
    put_bytes(mdia, "VideoHandler", 13);
    box_end(mdia, box);
}

/* 写 vmhd box。视频轨需要这个 video media header。 */
static void append_vmhd(byte_buffer_t *minf)
{
    
    size_t box = box_begin(minf, "vmhd");
    be32(minf, 0x00000001);
    be16(minf, 0);
    be16(minf, 0);
    be16(minf, 0);
    be16(minf, 0);
    box_end(minf, box);
}

/* 写 dinf/dref box。这里表示媒体数据就在当前 MP4 文件内部。 */
static void append_dinf(byte_buffer_t *minf)
{
    
    size_t dinf = box_begin(minf, "dinf");
    size_t dref = box_begin(minf, "dref");
    size_t url = 0;
    be32(minf, 0);
    be32(minf, 1);
    url = box_begin(minf, "url ");
    be32(minf, 0x00000001);
    box_end(minf, url);
    box_end(minf, dref);
    box_end(minf, dinf);
}

/*
 * 写 avcC box。
 * MP4 不把 SPS/PPS 当普通帧放进 mdat，而是放在 avcC 中供播放器初始化解码器。
 * avcC 也声明 mdat 中每个 NALU 前面的 length 字段长度是 4 字节。
 */
static void append_avcc(byte_buffer_t *stsd, const mp4_writer_t *w)
{
    
    size_t avcc = box_begin(stsd, "avcC");
    const byte_buffer_t *sps = &w->sps;
    const byte_buffer_t *pps = &w->pps;
    u8 profile = sps->size > 1 ? sps->data[1] : 0x64;
    u8 compat = sps->size > 2 ? sps->data[2] : 0;
    u8 level = sps->size > 3 ? sps->data[3] : 0x1f;

    be32(stsd, 0x01U << 24 | ((u32)profile << 16) |
               ((u32)compat << 8) | (u32)level);
    be16(stsd, 0xffe1);
    be16(stsd, (u32)sps->size);
    put_bytes(stsd, sps->data, sps->size);
    put_bytes(stsd, "\x01", 1);
    be16(stsd, (u32)pps->size);
    put_bytes(stsd, pps->data, pps->size);
    if (profile == 100 || profile == 110 || profile == 122 ||
        profile == 144) {
        put_bytes(stsd, "\xfc", 1);
        put_bytes(stsd, &(u8){(u8)(0xfc | (w->chroma_format_idc & 0x03))}, 1);
        put_bytes(stsd, &(u8){(u8)(0xf8 | (w->bit_depth_luma_minus8 & 0x07))}, 1);
        put_bytes(stsd, &(u8){(u8)(0xf8 | (w->bit_depth_chroma_minus8 & 0x07))}, 1);
        put_bytes(stsd, "\0", 1);
    }
    box_end(stsd, avcc);
}

/* 写 stsd box。stsd 描述 sample 的编码格式，这里是 avc1，也就是 H.264/AVC。 */
static void append_stsd(byte_buffer_t *stbl, const mp4_writer_t *w)
{
    
    size_t stsd = box_begin(stbl, "stsd");
    size_t avc1;
    be32(stbl, 0);
    be32(stbl, 1);
    avc1 = box_begin(stbl, "avc1");
    put_zeros(stbl, 6);
    be16(stbl, 1);
    put_zeros(stbl, 16);
    be16(stbl, w->width);
    be16(stbl, w->height);
    be32(stbl, 0x00480000);
    be32(stbl, 0x00480000);
    be32(stbl, 0);
    be16(stbl, 1);
    put_zeros(stbl, 32);
    be16(stbl, 0x0018);
    be16(stbl, 0xffff);
    append_avcc(stbl, w);
    box_end(stbl, avc1);
    box_end(stbl, stsd);
}

/*
 * 写 stts box。stts 描述每帧持续多久。
 * 慢动作问题通常就出在这里：如果 60fps 错写成 25fps，总时长就会被拉长。
 */
static void append_stts(byte_buffer_t *stbl, const mp4_writer_t *w)
{
    
    size_t box = box_begin(stbl, "stts");
    be32(stbl, 0);
    be32(stbl, 1);
    be32(stbl, (u32)w->samples.count);
    be32(stbl, w->sample_delta);
    box_end(stbl, box);
}

/* 写 stss box。stss 记录哪些 sample 是关键帧，方便播放器 seek。 */
static void append_stss(byte_buffer_t *stbl, const mp4_writer_t *w)
{
    
    size_t i;
    u32 sync_count = 0;
    size_t box = box_begin(stbl, "stss");

    for (i = 0; i < w->samples.count; i++) {
        if (w->samples.items[i].is_sync) {
            sync_count++;
        }
    }

    be32(stbl, 0);
    be32(stbl, sync_count);
    for (i = 0; i < w->samples.count; i++) {
        if (w->samples.items[i].is_sync) {
            be32(stbl, (u32)i + 1);
        }
    }
    box_end(stbl, box);
}

/* 写 stsc box。这里采用简单策略：每个 chunk 中只有一个 sample。 */
static void append_stsc(byte_buffer_t *stbl)
{
    
    size_t box = box_begin(stbl, "stsc");
    be32(stbl, 0);
    be32(stbl, 1);
    be32(stbl, 1);
    be32(stbl, 1);
    be32(stbl, 1);
    box_end(stbl, box);
}

/* 写 stsz box。stsz 记录每个 sample 的字节大小。 */
static void append_stsz(byte_buffer_t *stbl, const mp4_writer_t *w)
{
    
    size_t i;
    size_t box = box_begin(stbl, "stsz");
    be32(stbl, 0);
    be32(stbl, 0);
    be32(stbl, (u32)w->samples.count);
    for (i = 0; i < w->samples.count; i++) {
        be32(stbl, w->samples.items[i].size);
    }
    box_end(stbl, box);
}

/* 写 stco box。stco 记录每个 chunk/sample 在文件中的偏移。 */
static void append_stco(byte_buffer_t *stbl, const mp4_writer_t *w)
{
    
    size_t i;
    size_t box = box_begin(stbl, "stco");
    be32(stbl, 0);
    be32(stbl, (u32)w->samples.count);
    for (i = 0; i < w->samples.count; i++) {
        be32(stbl, w->samples.items[i].offset);
    }
    box_end(stbl, box);
}

/* 写 stbl box。sample table 是 MP4 播放最关键的索引区，包含格式、时间、大小、偏移等表。 */
static void append_stbl(byte_buffer_t *minf, const mp4_writer_t *w)
{
    
    size_t stbl = box_begin(minf, "stbl");
    append_stsd(minf, w);
    append_stts(minf, w);
    append_stss(minf, w);
    append_stsc(minf);
    append_stsz(minf, w);
    append_stco(minf, w);
    box_end(minf, stbl);
}

/* 写 minf box。media information 包含视频媒体头、数据引用和 sample table。 */
static void append_minf(byte_buffer_t *mdia, const mp4_writer_t *w)
{
    
    size_t minf = box_begin(mdia, "minf");
    append_vmhd(mdia);
    append_dinf(mdia);
    append_stbl(mdia, w);
    box_end(mdia, minf);
}

/* 写 mdia box。mdia 汇总一个 track 的媒体信息。 */
static void append_mdia(byte_buffer_t *trak, const mp4_writer_t *w, u32 duration)
{
    
    size_t mdia = box_begin(trak, "mdia");
    append_mdhd(trak, w->timescale, duration);
    append_hdlr(trak);
    append_minf(trak, w);
    box_end(trak, mdia);
}

/*
 * 构造 moov box。
 * moov 是 MP4 的“说明书”：播放器通过它知道宽高、帧率、SPS/PPS、每帧大小和每帧偏移。
 */
static h264_mp4_status_t build_moov(const mp4_writer_t *w, byte_buffer_t *moov)
{
    
    u64 media_duration64 = (u64)w->samples.count * (u64)w->sample_delta;
    u64 edit_offset64 = (u64)w->reorder_frames * (u64)w->sample_delta;
    u64 movie_duration64;
    u32 media_duration;
    u32 movie_duration;
    u32 edit_offset;
    size_t moov_box;
    size_t trak;

    if (media_duration64 > 0xffffffffULL || w->samples.count > 0xffffffffUL) {
        return H264_MP4_ERR_UNSUPPORTED;
    }
    if (edit_offset64 >= media_duration64) {
        edit_offset64 = 0;
    }
    movie_duration64 = media_duration64 - edit_offset64;
    media_duration = (u32)media_duration64;
    movie_duration = (u32)movie_duration64;
    edit_offset = (u32)edit_offset64;

    moov_box = box_begin(moov, "moov");
    append_mvhd(moov, w->timescale, movie_duration);
    trak = box_begin(moov, "trak");
    append_tkhd(moov, 1, movie_duration, w->width, w->height);
    append_edts(moov, movie_duration, edit_offset);
    append_mdia(moov, w, media_duration);
    box_end(moov, trak);
    box_end(moov, moov_box);

    if (moov->failed) {
        return H264_MP4_ERR_OUT_OF_MEMORY;
    }
    if (moov->size > H264_MP4_MAX_BOX_SIZE) {
        return H264_MP4_ERR_UNSUPPORTED;
    }
    return H264_MP4_OK;
}

/* 当前帧写完后，把它的 offset/size/is_sync 记录进 sample 表。 */
static h264_mp4_status_t flush_pending_sample(mp4_writer_t *w,
                                              pending_sample_t *sample)
{
    if (!w || !sample) {
        return H264_MP4_ERR_INVALID_ARG;
    }
    if (!sample->has_data) {
        return H264_MP4_OK;
    }
    if (!table_add(&w->samples, sample->offset, sample->size,
                   sample->is_sync)) {
        return H264_MP4_ERR_OUT_OF_MEMORY;
    }
    memset(sample, 0, sizeof(*sample));
    return H264_MP4_OK;
}

/*
 * 把一个 H.264 NALU 写入 mdat。
 * Annex-B 使用起始码分隔 NALU；MP4 使用 4 字节长度 + NALU 数据。
 * 这里完成的就是 Annex-B NALU 到 MP4 length-prefixed NALU 的转换。
 */
static h264_mp4_status_t append_nal_to_mdat(mp4_writer_t *w,
                                            pending_sample_t *sample,
                                            const u8 *nal,
                                            size_t nal_size)
{
    long pos;
    u32 written;

    if (!w || !w->out || !sample || !nal) {
        return H264_MP4_ERR_INVALID_ARG;
    }
    if (nal_size > 0xffffffffUL - 4UL) {
        return H264_MP4_ERR_UNSUPPORTED;
    }
    written = 4U + (u32)nal_size;
    if ((u64)sample->size + written > 0xffffffffULL ||
        (u64)w->mdat_payload_size + written > 0xffffffffULL - 8ULL) {
        return H264_MP4_ERR_UNSUPPORTED;
    }

    if (!sample->has_data) {
        pos = file_tell(w->out);
        if (pos < 0 || (u64)pos > 0xffffffffULL) {
            return H264_MP4_ERR_UNSUPPORTED;
        }
        sample->offset = (u32)pos;
        sample->has_data = 1;
    }

    if (!file_write_be32(w->out, (u32)nal_size)) {
        return H264_MP4_ERR_WRITE;
    }
    if (nal_size > 0 && fwrite(nal, 1, nal_size, w->out) != nal_size) {
        return H264_MP4_ERR_WRITE;
    }

    sample->size += written;
    w->mdat_payload_size += written;
    return H264_MP4_OK;
}

/*
 * 流式扫描 H.264 文件并写入 mdat。
 * 这个函数不会把整个输入文件读入内存，也不会缓存整帧数据。
 * 它只保存当前 NALU、SPS/PPS 和 sample 索引表。
 */
static h264_mp4_status_t process_stream_file(mp4_writer_t *w,
                                             const char *h264_path)
{
    h264_nalu_reader_t reader;
    byte_buffer_t nalbuf;
    pending_sample_t sample;
    int sample_has_vcl = 0;
    h264_mp4_status_t st;

    if (!w || !h264_path) {
        return H264_MP4_ERR_INVALID_ARG;
    }

    memset(&reader, 0, sizeof(reader));
    memset(&nalbuf, 0, sizeof(nalbuf));
    memset(&sample, 0, sizeof(sample));

    st = reader_open(&reader, h264_path);
    if (st != H264_MP4_OK) {
        goto done;
    }

    for (;;) {
        int got_nal = 0;
        u8 nal_type;

        st = reader_next_nalu(&reader, &nalbuf, &got_nal);
        if (st != H264_MP4_OK) {
            goto done;
        }
        if (!got_nal) {
            break;
        }
        if (nalbuf.size == 0) {
            continue;
        }

        nal_type = nalbuf.data[0] & 0x1f;

        if (nal_type == 7) {
            if (w->sps.size == 0) {
                h264_sps_info_t sps_info;
                if (nalbuf.size > H264_MP4_MAX_PARAM_SET ||
                    !buffer_append(&w->sps, nalbuf.data, nalbuf.size)) {
                    st = H264_MP4_ERR_OUT_OF_MEMORY;
                    goto done;
                }
                if (!parse_sps_info(nalbuf.data, nalbuf.size, &sps_info)) {
                    st = H264_MP4_ERR_UNSUPPORTED;
                    goto done;
                }
                if (w->width == 0) {
                    w->width = sps_info.width;
                }
                if (w->height == 0) {
                    w->height = sps_info.height;
                }
                if (w->fps_num == 0 && sps_info.fps_num != 0) {
                    w->fps_num = sps_info.fps_num;
                    w->fps_den = sps_info.fps_den;
                }
                w->reorder_frames = sps_info.reorder_frames;
                w->chroma_format_idc = sps_info.chroma_format_idc;
                w->bit_depth_luma_minus8 = sps_info.bit_depth_luma_minus8;
                w->bit_depth_chroma_minus8 = sps_info.bit_depth_chroma_minus8;
            }
            continue;
        }

        if (nal_type == 8) {
            if (w->pps.size == 0) {
                if (nalbuf.size > H264_MP4_MAX_PARAM_SET ||
                    !buffer_append(&w->pps, nalbuf.data, nalbuf.size)) {
                    st = H264_MP4_ERR_OUT_OF_MEMORY;
                    goto done;
                }
            }
            continue;
        }

        if (nal_type == 9) {
            continue;
        }

        if (nal_type == 1 || nal_type == 5) {
            u32 first_mb = read_first_mb_in_slice(nalbuf.data, nalbuf.size);
            if (sample_has_vcl && first_mb == 0) {
                st = flush_pending_sample(w, &sample);
                if (st != H264_MP4_OK) {
                    goto done;
                }
                sample_has_vcl = 0;
            }
            sample_has_vcl = 1;
            w->wrote_vcl = 1;
            if (nal_type == 5) {
                sample.is_sync = 1;
            }
        }

        st = append_nal_to_mdat(w, &sample, nalbuf.data, nalbuf.size);
        if (st != H264_MP4_OK) {
            goto done;
        }
    }

    st = flush_pending_sample(w, &sample);

done:
    reader_close(&reader);
    buffer_free(&nalbuf);
    return st;
}

/*
 * 对外 probe 接口。
 * 它只读取必要信息：SPS、PPS、至少一个视频帧。
 * 这样既能确认码流可封装，又不会为了统计完整帧数而读完整个大文件。
 */
h264_mp4_status_t h264_mp4_probe_file(const char *h264_path,
                                      h264_mp4_info_t *info)
{
    h264_nalu_reader_t reader;
    byte_buffer_t nalbuf;
    int sample_has_vcl = 0;
    h264_mp4_status_t st;

    if (!h264_path || !info) {
        return H264_MP4_ERR_INVALID_ARG;
    }

    memset(info, 0, sizeof(*info));
    memset(&reader, 0, sizeof(reader));
    memset(&nalbuf, 0, sizeof(nalbuf));

    st = reader_open(&reader, h264_path);
    if (st != H264_MP4_OK) {
        return st;
    }

    for (;;) {
        int got_nal = 0;
        u8 nal_type;

        st = reader_next_nalu(&reader, &nalbuf, &got_nal);
        if (st != H264_MP4_OK) {
            goto done;
        }
        if (!got_nal) {
            break;
        }
        if (nalbuf.size == 0) {
            continue;
        }

        nal_type = nalbuf.data[0] & 0x1f;

        if (nal_type == 7 && !info->has_sps) {
            h264_sps_info_t sps_info;
            if (!parse_sps_info(nalbuf.data, nalbuf.size, &sps_info)) {
                st = H264_MP4_ERR_UNSUPPORTED;
                goto done;
            }
            info->has_sps = 1;
            info->width = sps_info.width;
            info->height = sps_info.height;
            info->fps_num = sps_info.fps_num;
            info->fps_den = sps_info.fps_den;
            info->profile_idc = sps_info.profile_idc;
            info->level_idc = sps_info.level_idc;
            continue;
        }

        if (nal_type == 8) {
            info->has_pps = 1;
            continue;
        }

        if (nal_type == 1 || nal_type == 5) {
            u32 first_mb = read_first_mb_in_slice(nalbuf.data, nalbuf.size);
            if (!sample_has_vcl || first_mb == 0) {
                info->frame_count++;
            }
            sample_has_vcl = 1;
        } else if (nal_type == 9) {
            sample_has_vcl = 0;
        }

        if (info->has_sps && info->has_pps && info->frame_count > 0) {
            break;
        }
    }

done:
    reader_close(&reader);
    buffer_free(&nalbuf);
    if (st != H264_MP4_OK) {
        return st;
    }
    if (!info->has_sps) {
        return H264_MP4_ERR_NO_SPS;
    }
    if (!info->has_pps) {
        return H264_MP4_ERR_NO_PPS;
    }
    if (info->frame_count == 0) {
        return H264_MP4_ERR_NO_SAMPLES;
    }
    return H264_MP4_OK;
}

/*
 * 对外 mux 接口。完整封装流程：
 * 1. 创建 MP4 输出文件。
 * 2. 写 ftyp。
 * 3. 写 mdat 头，size 先占位。
 * 4. 流式读取 H.264 NALU，把视频数据写入 mdat。
 * 5. 回填 mdat size。
 * 6. 根据 sample 表构造 moov。
 * 7. 写 moov，MP4 完成。
 */
h264_mp4_status_t h264_mp4_mux_file(const char *h264_path,
                                    const char *mp4_path,
                                    const h264_mp4_config_t *config)
{
    
    byte_buffer_t moov;
    mp4_writer_t w;
    h264_mp4_status_t st;
    unsigned int cfg_fps_num;
    unsigned int cfg_fps_den;
    u64 delta64;
    long end_pos;
    u32 mdat_size;

    if (!h264_path || !mp4_path) {
        return H264_MP4_ERR_INVALID_ARG;
    }

    cfg_fps_num = config ? config->fps_num : 0U;
    cfg_fps_den = config ? config->fps_den : 0U;
    
    if ((cfg_fps_num == 0 && cfg_fps_den != 0) ||
        (cfg_fps_num != 0 && cfg_fps_den == 0)) {
        return H264_MP4_ERR_INVALID_ARG;
    }

    memset(&moov, 0, sizeof(moov));
    memset(&w, 0, sizeof(w));

    w.width = config ? config->width : 0;
    w.height = config ? config->height : 0;
    w.fps_num = cfg_fps_num;
    w.fps_den = cfg_fps_den;
    w.timescale = config && config->timescale ? config->timescale
                                              : H264_MP4_DEFAULT_TIMESCALE;

    //创建一个MP4文件用于写入封装后的数据
    w.out = fopen(mp4_path, "wb+b");
    if (!w.out) {
        st = H264_MP4_ERR_OPEN_OUTPUT;
        goto done;
    }

    //写入ftyp box，声明这是一个MP4文件
    st = write_ftyp(w.out);
    if (st != H264_MP4_OK) {
        goto done;
    }

    end_pos = file_tell(w.out);
    if (end_pos < 0 || (u64)end_pos > 0xffffffffULL) {
        st = H264_MP4_ERR_UNSUPPORTED;
        goto done;
    }
    w.mdat_start = (u32)end_pos;
    
    //写入mdat box头，size先占位，等视频数据写完后再回填正确的size
    if (!file_write_be32(w.out, 0) || fwrite("mdat", 1, 4, w.out) != 4) {
        st = H264_MP4_ERR_WRITE;
        goto done;
    }

    //流式读取H.264文件，处理NALU并写入mdat
    st = process_stream_file(&w, h264_path);
    if (st != H264_MP4_OK) {
        goto done;
    }
    if (w.sps.size == 0) {
        st = H264_MP4_ERR_NO_SPS;
        goto done;
    }
    if (w.pps.size == 0) {
        st = H264_MP4_ERR_NO_PPS;
        goto done;
    }
    if (!w.wrote_vcl || w.samples.count == 0) {
        st = H264_MP4_ERR_NO_SAMPLES;
        goto done;
    }
    if (w.width == 0 || w.height == 0) {
        st = H264_MP4_ERR_UNSUPPORTED;
        goto done;
    }
    if (w.width > 0xffffU || w.height > 0xffffU) {
        st = H264_MP4_ERR_UNSUPPORTED;
        goto done;
    }
    if (w.fps_num == 0 || w.fps_den == 0) {
        
        st = H264_MP4_ERR_NO_TIMING;
        goto done;
    }
    delta64 = ((u64)w.timescale * w.fps_den + w.fps_num / 2U) / w.fps_num;
    if (delta64 == 0 || delta64 > 0xffffffffULL) {
        st = H264_MP4_ERR_INVALID_ARG;
        goto done;
    }
    w.sample_delta = (u32)delta64;

    mdat_size = w.mdat_payload_size + 8U;
    end_pos = file_tell(w.out);
    if (end_pos < 0) {
        st = H264_MP4_ERR_WRITE;
        goto done;
    }
    
    if (!file_seek(w.out, (long)w.mdat_start) ||
        !file_write_be32(w.out, mdat_size) ||
        !file_seek(w.out, end_pos)) {
        st = H264_MP4_ERR_WRITE;
        goto done;
    }

    
    st = build_moov(&w, &moov);
    if (st != H264_MP4_OK) {
        goto done;
    }
    if (fwrite(moov.data, 1, moov.size, w.out) != moov.size) {
        st = H264_MP4_ERR_WRITE;
        goto done;
    }

done:
    if (w.out) {
        fclose(w.out);
    }
    buffer_free(&moov);
    buffer_free(&w.sps);
    buffer_free(&w.pps);
    table_free(&w.samples);
    return st;
}

/* 把错误码转换成可打印字符串，方便示例程序和上层工程显示错误原因。 */
const char *h264_mp4_status_string(h264_mp4_status_t status)
{
    
    switch (status) {
    case H264_MP4_OK: return "ok";
    case H264_MP4_ERR_INVALID_ARG: return "invalid argument";
    case H264_MP4_ERR_OPEN_INPUT: return "failed to open input file";
    case H264_MP4_ERR_OPEN_OUTPUT: return "failed to open output file";
    case H264_MP4_ERR_READ: return "failed to read input file";
    case H264_MP4_ERR_WRITE: return "failed to write output file";
    case H264_MP4_ERR_NO_SPS: return "missing H.264 SPS";
    case H264_MP4_ERR_NO_PPS: return "missing H.264 PPS";
    case H264_MP4_ERR_NO_SAMPLES: return "missing H.264 video samples";
    case H264_MP4_ERR_UNSUPPORTED: return "unsupported stream or file too large";
    case H264_MP4_ERR_OUT_OF_MEMORY: return "out of memory";
    case H264_MP4_ERR_NO_TIMING: return "missing H.264 SPS VUI timing; pass fps explicitly";
    default: return "unknown error";
    }
}
