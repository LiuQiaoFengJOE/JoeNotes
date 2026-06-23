#include "rtcp.h"

#include <stdio.h>
#include <string.h>

/*
 * RTCP 和 RTP 一样，包里的多字节整数都用网络字节序。
 * 所以这里统一提供写入/读取 big-endian 整数的小工具。
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

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

int rtcp_send_sender_report(const UdpEndpoint *rtcp_endpoint,
                            const RtcpSenderState *state,
                            uint32_t rtp_timestamp)
{
    uint8_t packet[28];
    uint32_t ntp_sec;
    uint32_t ntp_frac;

    if (rtcp_endpoint == NULL || state == NULL) {
        return -1;
    }

    /*
     * SR 里同时放 NTP 时间和 RTP timestamp。
     * 这样接收端就知道“媒体时间”和“真实墙钟时间”的对应关系。
     */
    platform_ntp_now(&ntp_sec, &ntp_frac);

    memset(packet, 0, sizeof(packet));

    /*
     * RTCP Sender Report 固定部分：
     *
     * byte 0: V=2, P=0, RC=0
     *   RC 是 report count。这个 demo 的 SR 后面不带 report block，所以 RC=0。
     *
     * byte 1: PT=200，表示 Sender Report。
     *
     * length 字段单位是 32-bit word，并且“不包含第一个 32-bit header”。
     * 这个包总长 28 字节 = 7 个 word，所以 length = 7 - 1 = 6。
     */
    packet[0] = 0x80;
    packet[1] = 200;
    write_be16(&packet[2], 6);
    write_be32(&packet[4], state->ssrc);
    write_be32(&packet[8], ntp_sec);
    write_be32(&packet[12], ntp_frac);
    write_be32(&packet[16], rtp_timestamp);
    write_be32(&packet[20], state->packet_count);
    write_be32(&packet[24], state->octet_count);

    return udp_endpoint_send(rtcp_endpoint, packet, sizeof(packet));
}

int rtcp_parse_sender_report(const uint8_t *packet,
                             size_t packet_size,
                             RtcpSenderReport *report)
{
    uint8_t version;
    uint8_t packet_type;
    uint16_t length_words_minus_one;
    size_t expected_size;

    if (packet == NULL || report == NULL || packet_size < 28) {
        return -1;
    }

    version = (uint8_t)(packet[0] >> 6);
    packet_type = packet[1];
    length_words_minus_one = read_be16(&packet[2]);

    if (version != 2 || packet_type != 200) {
        return -1;
    }

    /*
     * 根据 RTCP length 计算真实包长。
     * 加这个检查，是为了避免收到残缺 UDP 包时还继续读越界字段。
     */
    expected_size = ((size_t)length_words_minus_one + 1u) * 4u;
    if (packet_size < expected_size || expected_size < 28) {
        return -1;
    }

    memset(report, 0, sizeof(*report));
    report->sender_ssrc = read_be32(&packet[4]);
    report->ntp_sec = read_be32(&packet[8]);
    report->ntp_frac = read_be32(&packet[12]);
    report->rtp_timestamp = read_be32(&packet[16]);
    report->packet_count = read_be32(&packet[20]);
    report->octet_count = read_be32(&packet[24]);
    return 0;
}

int rtcp_send_receiver_report(const UdpEndpoint *rtcp_endpoint,
                              const RtcpReceiverState *state)
{
    uint8_t packet[32];
    uint32_t expected_packets;
    uint32_t lost_packets;
    uint8_t fraction_lost;

    if (rtcp_endpoint == NULL || state == NULL) {
        return -1;
    }

    /*
     * RTP sequence 不一定从 0 开始。
     * 本 demo 的发送端从 1000 开始，所以理论应收包数是：
     *   highest_sequence - base_sequence + 1
     *
     * 完整工程还要处理 16-bit sequence 回绕，这里先简化。
     */
    if (state->highest_sequence >= state->base_sequence) {
        expected_packets = state->highest_sequence - state->base_sequence + 1u;
    } else {
        expected_packets = state->packets_received;
    }
    lost_packets = expected_packets > state->packets_received
                 ? expected_packets - state->packets_received
                 : 0u;

    /*
     * fraction lost 是 8-bit 定点数：
     *   丢包比例 * 256
     *
     * 例如丢包率 25%，fraction_lost 大约是 64。
     * 这里为了教学用累计值估算，真实实现通常按统计周期计算。
     */
    if (expected_packets == 0) {
        fraction_lost = 0;
    } else {
        fraction_lost = (uint8_t)((lost_packets * 256u) / expected_packets);
    }

    memset(packet, 0, sizeof(packet));

    /*
     * RTCP Receiver Report:
     *
     * byte 0: V=2, P=0, RC=1
     *   RC=1 表示后面跟 1 个 report block。
     *
     * byte 1: PT=201，表示 Receiver Report。
     *
     * 总长 32 字节 = 8 个 word，所以 length = 8 - 1 = 7。
     */
    packet[0] = 0x81;
    packet[1] = 201;
    write_be16(&packet[2], 7);

    /* 先写 RR 发送者自己的 SSRC，也就是 receiver 的 SSRC。 */
    write_be32(&packet[4], state->receiver_ssrc);

    /*
     * report block 表示：
     * “我正在汇报自己接收 sender_ssrc 这个源时看到的情况”。
     */
    write_be32(&packet[8], state->sender_ssrc);
    packet[12] = fraction_lost;
    packet[13] = (uint8_t)((lost_packets >> 16) & 0xff);
    packet[14] = (uint8_t)((lost_packets >> 8) & 0xff);
    packet[15] = (uint8_t)(lost_packets & 0xff);
    write_be32(&packet[16], state->highest_sequence);
    write_be32(&packet[20], state->jitter);

    /*
     * LSR/DLSR 可用于计算 RTT：
     * - LSR：最近一次收到的 SR 时间戳摘要
     * - DLSR：收到 SR 到发送 RR 的延迟
     *
     * 本 demo 暂不计算 RTT，所以填 0，但保留字段位置。
     */
    write_be32(&packet[24], 0);
    write_be32(&packet[28], 0);

    return udp_endpoint_send(rtcp_endpoint, packet, sizeof(packet));
}

int rtcp_send_app_message(const UdpEndpoint *rtcp_endpoint,
                          uint32_t ssrc,
                          const char app_name[4],
                          const char *text)
{
    uint8_t packet[64];
    size_t text_len;
    size_t app_data_len;
    size_t packet_size;
    size_t i;

    if (rtcp_endpoint == NULL || app_name == NULL || text == NULL) {
        return -1;
    }

    text_len = strlen(text);
    if (text_len > 47u) {
        text_len = 47u;
    }

    /*
     * RTCP APP 包格式：
     *   4 bytes RTCP common header
     *   4 bytes SSRC
     *   4 bytes name，例如 "PAIR"
     *   N bytes application data
     *
     * RTCP 总长度必须是 32-bit 对齐，所以 text 后面要补 0。
     */
    app_data_len = text_len + 1u;
    while ((12u + app_data_len) % 4u != 0u) {
        app_data_len++;
    }
    packet_size = 12u + app_data_len;

    memset(packet, 0, sizeof(packet));
    packet[0] = 0x80;
    packet[1] = 204;
    write_be16(&packet[2], (uint16_t)(packet_size / 4u - 1u));
    write_be32(&packet[4], ssrc);
    for (i = 0; i < 4u; ++i) {
        packet[8u + i] = (uint8_t)app_name[i];
    }
    memcpy(packet + 12u, text, text_len);

    return udp_endpoint_send(rtcp_endpoint, packet, packet_size);
}

int rtcp_parse_app_message(const uint8_t *packet,
                           size_t packet_size,
                           char app_name[5],
                           char *text,
                           size_t text_capacity,
                           uint32_t *ssrc)
{
    uint8_t version;
    uint16_t length_words_minus_one;
    size_t expected_size;
    size_t text_len;

    if (packet == NULL || packet_size < 12u || app_name == NULL ||
        text == NULL || text_capacity == 0u) {
        return -1;
    }

    version = (uint8_t)(packet[0] >> 6);
    if (version != 2 || packet[1] != 204) {
        return -1;
    }

    length_words_minus_one = read_be16(&packet[2]);
    expected_size = ((size_t)length_words_minus_one + 1u) * 4u;
    if (expected_size > packet_size || expected_size < 12u) {
        return -1;
    }

    if (ssrc != NULL) {
        *ssrc = read_be32(&packet[4]);
    }

    memcpy(app_name, packet + 8u, 4u);
    app_name[4] = '\0';

    text_len = expected_size - 12u;
    if (text_len >= text_capacity) {
        text_len = text_capacity - 1u;
    }
    memcpy(text, packet + 12u, text_len);
    text[text_len] = '\0';
    return 0;
}

void rtcp_print_packet_type(const uint8_t *packet, size_t packet_size, const char *prefix)
{
    uint8_t packet_type;

    if (packet == NULL || packet_size < 2) {
        return;
    }

    packet_type = packet[1];
    if (packet_type == 200) {
        printf("%s RTCP Sender Report (SR), size=%u\n",
               prefix, (unsigned)packet_size);
    } else if (packet_type == 201) {
        printf("%s RTCP Receiver Report (RR), size=%u\n",
               prefix, (unsigned)packet_size);
    } else if (packet_type == 202) {
        printf("%s RTCP SDES, size=%u\n", prefix, (unsigned)packet_size);
    } else if (packet_type == 203) {
        printf("%s RTCP BYE, size=%u\n", prefix, (unsigned)packet_size);
    } else if (packet_type == 204) {
        char app_name[5];
        char text[48];
        uint32_t ssrc = 0;
        if (rtcp_parse_app_message(packet, packet_size, app_name, text, sizeof(text), &ssrc) == 0) {
            printf("%s RTCP APP name=%s text=%s ssrc=0x%08x size=%u\n",
                   prefix,
                   app_name,
                   text,
                   (unsigned)ssrc,
                   (unsigned)packet_size);
        } else {
            printf("%s RTCP APP, size=%u\n", prefix, (unsigned)packet_size);
        }
    } else {
        printf("%s RTCP PT=%u, size=%u\n",
               prefix, (unsigned)packet_type, (unsigned)packet_size);
    }
}
