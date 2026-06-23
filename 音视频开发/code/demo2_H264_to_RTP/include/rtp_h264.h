#ifndef RTP_H264_H
#define RTP_H264_H

#include <stddef.h>
#include <stdint.h>

#include "platform_net.h"

typedef struct RtpH264Session {
    /* 发送 RTP 时用的 UDP 端点，保存 socket 和对端地址。 */
    UdpEndpoint *rtp_endpoint;
    /* 同一条 RTP 会话的同步源标识。 */
    uint32_t ssrc;
    /* RTP 序号，每发一个包加 1。 */
    uint16_t sequence;
    /* RTP 时间戳，用来描述视频帧的播放时间。 */
    uint32_t timestamp;
    /* H264 的 payload type，demo 里固定用 96。 */
    uint8_t payload_type;
    /* 单个 RTP 包允许携带的最大 payload。 */
    size_t max_payload_size;
    /* 统计：已经发送了多少个 RTP 包。 */
    uint32_t packet_count;
    /* 统计：已经发送了多少个 RTP payload 字节。 */
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

/* 发送完一帧后，按固定步长推进 RTP timestamp。 */
void rtp_h264_add_timestamp(RtpH264Session *session, uint32_t timestamp_step);

#endif
