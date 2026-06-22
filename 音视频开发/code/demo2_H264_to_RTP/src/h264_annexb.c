#include "h264_annexb.h"

/*
 * 这是一个教学用 Annex-B 读取器。
 *
 * 为了让逻辑更容易读，它使用 fgetc() 逐字节扫描起始码。
 * PC 调试没有问题；移植到小平台或高码率产品时，可以改成环形缓冲区读取。
 */

static int find_start_code(FILE *fp)
{
    int ch;
    int zero_count = 0;

    while ((ch = fgetc(fp)) != EOF) {
        if (ch == 0x00) {
            zero_count++;
            continue;
        }

        if (ch == 0x01 && zero_count >= 2) {
            return 1;
        }

        zero_count = 0;
    }

    if (ferror(fp)) {
        return -1;
    }

    return 0;
}

H264ReadResult h264_annexb_read_next_nal(FILE *fp,
                                         uint8_t *nal_buf,
                                         size_t nal_buf_capacity,
                                         size_t *nal_size)
{
    int start_found;
    int ch;
    int zero_count = 0;
    size_t size = 0;

    if (fp == NULL || nal_buf == NULL || nal_size == NULL || nal_buf_capacity == 0) {
        return H264_READ_IO_ERROR;
    }

    *nal_size = 0;

    start_found = find_start_code(fp);
    if (start_found < 0) {
        return H264_READ_IO_ERROR;
    }
    if (start_found == 0) {
        return H264_READ_EOF;
    }

    /*
     * 已经越过起始码，现在开始收集 NALU 内容。
     * 当再次遇到 00 00 01 或 00 00 00 01 时，说明下一个 NALU 开始。
     */
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == 0x00) {
            if (size >= nal_buf_capacity) {
                return H264_READ_NAL_TOO_LARGE;
            }
            nal_buf[size++] = 0x00;
            zero_count++;
            continue;
        }

        if (ch == 0x01 && zero_count >= 2) {
            /*
             * 刚刚读到的是下一个起始码的最后一个字节 0x01。
             * 前面的 2 个或更多 0x00 已经被放进 nal_buf，需要从当前 NALU 中删掉。
             *
             * 注意：这里必须把文件指针退回到这个起始码开头。
             * 否则下一次调用会从起始码后面继续找，直接跳过一个 NALU。
             * 这个 demo 面向普通 .h264 文件，所以可以使用 fseek()。
             */
            size -= (size_t)zero_count;
            if (fseek(fp, -(long)(zero_count + 1), SEEK_CUR) != 0) {
                return H264_READ_IO_ERROR;
            }
            *nal_size = size;
            return size == 0 ? h264_annexb_read_next_nal(fp, nal_buf, nal_buf_capacity, nal_size)
                             : H264_READ_OK;
        }

        if (size >= nal_buf_capacity) {
            return H264_READ_NAL_TOO_LARGE;
        }
        nal_buf[size++] = (uint8_t)ch;
        zero_count = 0;
    }

    if (ferror(fp)) {
        return H264_READ_IO_ERROR;
    }

    /*
     * 文件结尾也是一个 NALU 的结束。
     * 末尾填充的 0x00 不属于有效 NALU 时，这里保守地去掉。
     */
    while (size > 0 && nal_buf[size - 1] == 0x00) {
        size--;
    }

    *nal_size = size;
    return size == 0 ? H264_READ_EOF : H264_READ_OK;
}

const char *h264_nal_type_name(uint8_t nal_type)
{
    switch (nal_type) {
    case 1: return "non-IDR slice";
    case 5: return "IDR slice";
    case 6: return "SEI";
    case 7: return "SPS";
    case 8: return "PPS";
    case 9: return "AUD";
    default: return "other";
    }
}

int h264_nal_is_vcl(uint8_t nal_type)
{
    /* VCL NALU 承载真正的视频图像切片。1-5 都属于 VCL。 */
    return nal_type >= 1 && nal_type <= 5;
}
