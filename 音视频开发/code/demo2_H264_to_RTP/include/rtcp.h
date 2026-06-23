#ifndef RTCP_H
#define RTCP_H

#include <stddef.h>
#include <stdint.h>

#include "platform_net.h"

typedef struct RtcpSenderState {
    /* 发包端自己的同步源标识。 */
    uint32_t ssrc;
    /* 已发送 RTP 包数量。 */
    uint32_t packet_count;
    /* 已发送 RTP payload 字节数。 */
    uint32_t octet_count;
} RtcpSenderState;

typedef struct RtcpSenderReport {
    /* 发送 SR 的源 SSRC。 */
    uint32_t sender_ssrc;
    /* NTP 秒部分，墙钟时间。 */
    uint32_t ntp_sec;
    /* NTP 小数秒部分。 */
    uint32_t ntp_frac;
    /* 与这个 NTP 时间对应的 RTP timestamp。 */
    uint32_t rtp_timestamp;
    /* 发送端累计 RTP 包数。 */
    uint32_t packet_count;
    /* 发送端累计 RTP payload 字节数。 */
    uint32_t octet_count;
} RtcpSenderReport;

typedef struct RtcpReceiverState {
    /* 接收端自己的 SSRC。 */
    uint32_t receiver_ssrc;
    /* 我正在汇报的发送端 SSRC。 */
    uint32_t sender_ssrc;
    /* 收到的第一个序号，用来计算丢包。 */
    uint32_t base_sequence;
    /* 当前看到的最大序号。 */
    uint32_t highest_sequence;
    /* 实际收到的 RTP 包数。 */
    uint32_t packets_received;
    /* 估算丢包数，demo 里只保留字段，不做复杂算法。 */
    uint32_t packets_lost;
    /* 到达间隔抖动，demo 里暂时不计算。 */
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
 * RTCP APP 是标准里预留给应用层自定义消息的包类型。
 *
 * 这个 demo 把它用成一个很简单的“握手”：
 *   sender   -- APP "HELLO"   --> receiver
 *   sender   <-- APP "WELCOME" -- receiver
 *
 * 这不是 RTSP，也不是 TCP 连接，只是为了让学习流程更清楚。
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
