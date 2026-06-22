#ifndef RTP_H264_H
#define RTP_H264_H

#include <stddef.h>
#include <stdint.h>

#include "platform_net.h"

typedef struct RtpH264Session {
    UdpEndpoint *rtp_endpoint;
    uint32_t ssrc;
    uint16_t sequence;
    uint32_t timestamp;
    uint8_t payload_type;
    size_t max_payload_size;
    uint32_t packet_count;
    uint32_t octet_count;
} RtpH264Session;

void rtp_h264_session_init(RtpH264Session *session,
                           UdpEndpoint *rtp_endpoint,
                           uint32_t ssrc,
                           uint16_t first_sequence,
                           uint32_t first_timestamp,
                           uint8_t payload_type,
                           size_t max_payload_size);

int rtp_h264_send_nal(RtpH264Session *session,
                      const uint8_t *nal,
                      size_t nal_size,
                      int marker);

void rtp_h264_add_timestamp(RtpH264Session *session, uint32_t timestamp_step);

#endif

