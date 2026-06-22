#include "rtp_h264.h"

#include <stdio.h>
#include <string.h>

#include "demo_config.h"

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
    packet[0] = 0x80; /* V=2, P=0, X=0, CC=0 */
    packet[1] = (uint8_t)(session->payload_type & 0x7f);
    if (marker) {
        packet[1] |= 0x80;
    }
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

    if (session->max_payload_size > DEMO_RTP_MAX_PAYLOAD_SIZE || session->max_payload_size < 3) {
        fprintf(stderr, "invalid RTP payload size\n");
        return -1;
    }

    /*
     * 情况 1：NALU 足够小，直接放入一个 RTP 包。
     *
     * RTP payload:
     *   +---------------+
     *   | H264 NALU ... |
     *   +---------------+
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
     * 情况 2：NALU 太大，需要 FU-A 分片。
     *
     * 原始 NAL header:
     *   F | NRI | Type
     *
     * FU-A RTP payload:
     *   FU indicator: F | NRI | 28
     *   FU header:    S | E | R | 原始 Type
     *   后面跟原始 NALU 去掉第一个 NAL header 后的数据片段。
     */
    nal_header = nal[0];
    nal_f = (uint8_t)(nal_header & 0x80);
    nal_nri = (uint8_t)(nal_header & 0x60);
    nal_type = (uint8_t)(nal_header & 0x1f);

    offset = 1;
    fu_payload_capacity = session->max_payload_size - 2;

    while (offset < nal_size) {
        size_t remain = nal_size - offset;
        size_t fragment_size = remain > fu_payload_capacity ? fu_payload_capacity : remain;
        int start = (offset == 1);
        int end = (offset + fragment_size == nal_size);
        int packet_marker = end ? marker : 0;

        rtp_write_header(session, packet, packet_marker);

        packet[DEMO_RTP_HEADER_SIZE] = (uint8_t)(nal_f | nal_nri | 28);
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

