#ifndef RTCP_H
#define RTCP_H

#include <stddef.h>
#include <stdint.h>

#include "platform_net.h"

typedef struct RtcpSenderState {
    uint32_t ssrc;
    uint32_t packet_count;
    uint32_t octet_count;
} RtcpSenderState;

typedef struct RtcpSenderReport {
    uint32_t sender_ssrc;
    uint32_t ntp_sec;
    uint32_t ntp_frac;
    uint32_t rtp_timestamp;
    uint32_t packet_count;
    uint32_t octet_count;
} RtcpSenderReport;

typedef struct RtcpReceiverState {
    uint32_t receiver_ssrc;
    uint32_t sender_ssrc;
    uint32_t base_sequence;
    uint32_t highest_sequence;
    uint32_t packets_received;
    uint32_t packets_lost;
    uint32_t jitter;
} RtcpReceiverState;

int rtcp_send_sender_report(const UdpEndpoint *rtcp_endpoint,
                            const RtcpSenderState *state,
                            uint32_t rtp_timestamp);

int rtcp_parse_sender_report(const uint8_t *packet,
                             size_t packet_size,
                             RtcpSenderReport *report);

int rtcp_send_receiver_report(const UdpEndpoint *rtcp_endpoint,
                              const RtcpReceiverState *state);

/*
 * RTCP APP packet 是 RTCP 标准中给应用层自定义消息预留的包类型。
 * 这里用它做一个非常简单的“配对握手”：
 *
 *   sender   -- APP "HELLO"   --> receiver
 *   sender   <-- APP "WELCOME" -- receiver
 *
 * 注意：这不是 RTSP，也不是 TCP connect，只是为了学习时让流程更清楚。
 */
int rtcp_send_app_message(const UdpEndpoint *rtcp_endpoint,
                          uint32_t ssrc,
                          const char app_name[4],
                          const char *text);

int rtcp_parse_app_message(const uint8_t *packet,
                           size_t packet_size,
                           char app_name[5],
                           char *text,
                           size_t text_capacity,
                           uint32_t *ssrc);

void rtcp_print_packet_type(const uint8_t *packet, size_t packet_size, const char *prefix);

#endif
