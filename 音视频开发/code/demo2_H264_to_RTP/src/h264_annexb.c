#include "h264_annexb.h"

/*
 * 这是一个教学用 Annex-B 读取器。
 *
 * 它的目标不是极致性能，而是让你直观看懂：
 * 1. 怎么在文件里找起始码
 * 2. 怎么把一个 NALU 完整切出来
 * 3. 为什么返回的数据不包含起始码
 */

static int find_start_code(FILE *fp)
{
    int ch;
    int zero_count = 0;

    /*
     * Annex-B 起始码一般是：
     *   00 00 01
     * 或
     *   00 00 00 01
     *
     * 这里逐字节扫描，遇到连续 2 个及以上 0，后面跟 1，就认为找到了起始码。
     */
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
     * 找到起始码后，开始收集 NALU 内容。
     * 一旦再次遇到起始码，就说明当前 NALU 到头了。
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
             * 刚读到的是下一个起始码的末尾 0x01。
             * 需要把多写进去的 0x00 从当前 NALU 里回退掉，
             * 并把文件指针退回，让下一次调用还能看到这个起始码。
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
     * 文件结尾也可能正好是最后一个 NALU 的结束。
     * 尾部多出来的 0x00 不算有效数据，保守地去掉。
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
    /* VCL NALU 承载真正的视频图像切片。1..5 都属于 VCL。 */
    return nal_type >= 1 && nal_type <= 5;
}
