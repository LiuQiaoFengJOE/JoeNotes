#include "rtp_h264.h"

#include <stdio.h>
#include <string.h>

#include "demo_config.h"

/*
 * RTP/RTCP 协议里的多字节整数都使用网络字节序，也就是 big-endian。
 * Windows/x86 常见 CPU 是 little-endian，所以写包时不能直接 memcpy 整数。
 */
static void write_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xff);
}

static void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)((v >> 16) & 0xff);
    p[2] = (uint8_t)((v >> 8) & 0xff);
    p[3] = (uint8_t)(v & 0xff);
}

static void rtp_write_header(RtpH264Session *session,
                             uint8_t *packet,
                             int marker)
{
    /*
     * RTP 固定头一共 12 字节：
     *
     * byte 0:
     *   V  = 2，RTP 版本固定为 2
     *   P  = 0，不使用 padding
     *   X  = 0，不使用扩展头
     *   CC = 0，没有 CSRC 列表
     *
     * 0x80 的二进制是 1000 0000，正好表示 V=2，其余位为 0。
     */
    packet[0] = 0x80;

    /*
     * byte 1:
     *   bit 7    = marker
     *   bit 6..0 = payload type
     *
     * H264 常使用动态 payload type，比如 96。marker 对视频通常表示
     * “一个访问单元/一帧的最后一个 RTP 包”。
     */
    packet[1] = (uint8_t)(session->payload_type & 0x7f);
    if (marker) {
        packet[1] |= 0x80;
    }

    /*
     * sequence 每发一个 RTP 包加 1，用来帮助接收端发现丢包或乱序。
     * timestamp 表示媒体时间，同一帧的多个 RTP 包 timestamp 相同。
     * SSRC 用来区分同一个会话里的不同发送源。
     */
    write_be16(&packet[2], session->sequence);
    write_be32(&packet[4], session->timestamp);
    write_be32(&packet[8], session->ssrc);
}

static int rtp_send_packet(RtpH264Session *session,
                           uint8_t *packet,
                           size_t packet_size,
                           size_t payload_size)
{
    if (udp_endpoint_send(session->rtp_endpoint, packet, packet_size) != 0) {
        return -1;
    }

    /*
     * 只有真正 send 成功后才推进统计值。
     * RTCP Sender Report 会使用 packet_count 和 octet_count 汇报发送情况。
     */
    session->sequence++;
    session->packet_count++;
    session->octet_count += (uint32_t)payload_size;
    return 0;
}

void rtp_h264_session_init(RtpH264Session *session,
                           UdpEndpoint *rtp_endpoint,
                           uint32_t ssrc,
                           uint16_t first_sequence,
                           uint32_t first_timestamp,
                           uint8_t payload_type,
                           size_t max_payload_size)
{
    memset(session, 0, sizeof(*session));
    session->rtp_endpoint = rtp_endpoint;
    session->ssrc = ssrc;
    session->sequence = first_sequence;
    session->timestamp = first_timestamp;
    session->payload_type = payload_type;
    session->max_payload_size = max_payload_size;
}

int rtp_h264_send_nal(RtpH264Session *session,
                      const uint8_t *nal,
                      size_t nal_size,
                      int marker)
{
    uint8_t packet[DEMO_RTP_MTU];
    uint8_t nal_header;
    uint8_t nal_f;
    uint8_t nal_nri;
    uint8_t nal_type;
    size_t offset;
    size_t fu_payload_capacity;

    if (session == NULL || nal == NULL || nal_size == 0) {
        return -1;
    }

    /*
     * FU-A 分片至少需要 2 字节 FU 头，再加上至少 1 字节数据。
     * 如果 max_payload_size 小于 3，就连一个合法分片都放不下。
     */
    if (session->max_payload_size > DEMO_RTP_MAX_PAYLOAD_SIZE || session->max_payload_size < 3) {
        fprintf(stderr, "invalid RTP payload size\n");
        return -1;
    }

    /*
     * 情况 1：NALU 足够小，直接塞进一个 RTP 包。
     *
     * UDP payload:
     *   RTP header + H264 NALU
     *
     * 为什么可以直接放？
     * RFC 6184 规定 H264 over RTP 支持 Single NAL Unit 模式。
     * 这时 RTP payload 的第一个字节就是原始 H264 NAL header。
     */
    if (nal_size <= session->max_payload_size) {
        rtp_write_header(session, packet, marker);
        memcpy(packet + DEMO_RTP_HEADER_SIZE, nal, nal_size);
        return rtp_send_packet(session,
                               packet,
                               DEMO_RTP_HEADER_SIZE + nal_size,
                               nal_size);
    }

    /*
     * 情况 2：NALU 太大，需要拆成 FU-A 分片。
     *
     * 为什么要分片？
     * 直接发送很大的 UDP 包会触发 IP 分片，网络上任意一个 IP 分片丢失，
     * 整个 UDP 包都不可用。RTP 层主动分片更容易控制包大小和丢包影响范围。
     *
     * 原始 NAL header:
     *   F | NRI | Type
     *
     * FU-A payload 前两个字节：
     *   FU indicator: F | NRI | 28
     *   FU header:    S | E | R | 原始 Type
     *
     * 注意：原始 NAL header 不再作为数据直接发送，而是被拆进这两个字节里。
     */
    nal_header = nal[0];
    nal_f = (uint8_t)(nal_header & 0x80);
    nal_nri = (uint8_t)(nal_header & 0x60);
    nal_type = (uint8_t)(nal_header & 0x1f);

    /* offset 从 1 开始，因为 nal[0] 是原始 NAL header，FU-A 里不直接复制它。 */
    offset = 1;
    fu_payload_capacity = session->max_payload_size - 2;

    while (offset < nal_size) {
        size_t remain = nal_size - offset;
        size_t fragment_size = remain > fu_payload_capacity ? fu_payload_capacity : remain;
        int start = (offset == 1);
        int end = (offset + fragment_size == nal_size);
        int packet_marker = end ? marker : 0;

        /*
         * 同一个 NALU 的所有 FU-A 分片使用同一个 RTP timestamp。
         * sequence 仍然每个 RTP 包递增。
         */
        rtp_write_header(session, packet, packet_marker);

        /*
         * FU indicator：
         *   F 和 NRI 沿用原始 NAL header
         *   Type 改成 28，表示这是 FU-A
         */
        packet[DEMO_RTP_HEADER_SIZE] = (uint8_t)(nal_f | nal_nri | 28);

        /*
         * FU header：
         *   S=1 表示第一个分片
         *   E=1 表示最后一个分片
         *   Type 保存原始 NAL type，接收端靠它恢复原始 NAL header
         */
        packet[DEMO_RTP_HEADER_SIZE + 1] = nal_type;
        if (start) {
            packet[DEMO_RTP_HEADER_SIZE + 1] |= 0x80;
        }
        if (end) {
            packet[DEMO_RTP_HEADER_SIZE + 1] |= 0x40;
        }

        memcpy(packet + DEMO_RTP_HEADER_SIZE + 2, nal + offset, fragment_size);

        if (rtp_send_packet(session,
                            packet,
                            DEMO_RTP_HEADER_SIZE + 2 + fragment_size,
                            2 + fragment_size) != 0) {
            return -1;
        }

        offset += fragment_size;
    }

    return 0;
}

void rtp_h264_add_timestamp(RtpH264Session *session, uint32_t timestamp_step)
{
    if (session != NULL) {
        session->timestamp += timestamp_step;
    }
}
