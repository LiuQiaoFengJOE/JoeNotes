#ifndef H264_ANNEXB_H
#define H264_ANNEXB_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum H264ReadResult {
    H264_READ_OK = 0,
    H264_READ_EOF = 1,
    H264_READ_NAL_TOO_LARGE = 2,
    H264_READ_IO_ERROR = 3
} H264ReadResult;

/*
 * 从 Annex-B H264 文件中读取下一个 NALU。
 *
 * Annex-B 的 NALU 前面有起始码：
 *   00 00 01
 * 或：
 *   00 00 00 01
 *
 * 返回的 nal_buf 不包含起始码，只包含 NAL header + RBSP/EBSP 数据。
 */
H264ReadResult h264_annexb_read_next_nal(FILE *fp,
                                         uint8_t *nal_buf,
                                         size_t nal_buf_capacity,
                                         size_t *nal_size);

const char *h264_nal_type_name(uint8_t nal_type);
int h264_nal_is_vcl(uint8_t nal_type);

#endif

