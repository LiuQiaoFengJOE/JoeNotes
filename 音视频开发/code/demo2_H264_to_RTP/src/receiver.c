#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console_helper.h"
#include "demo_config.h"
#include "platform_net.h"
#include "rtcp.h"

typedef struct RtpPacketView {
    uint8_t marker;
    uint8_t payload_type;
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
    const uint8_t *payload;
    size_t payload_size;
} RtpPacketView;

typedef struct H264Depacketizer {
    FILE *out;
    int fu_started;
    uint32_t nal_count;
    uint32_t fu_packet_count;
    uint32_t stap_a_count;
} H264Depacketizer;

typedef struct ReceiverStats {
    uint32_t sender_ssrc;
    uint32_t receiver_ssrc;
    uint32_t base_sequence;
    uint32_t packets_received;
    uint32_t highest_sequence;
    int have_sequence;
} ReceiverStats;

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

static uint32_t parse_u32_or_default(const char *text, uint32_t fallback)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL) {
        return fallback;
    }

    value = strtoul(text, &end, 10);
    if (end == text || *end != '\0') {
        return fallback;
    }
    return (uint32_t)value;
}

static void print_usage(const char *program)
{
    printf("Usage:\n");
    printf("  %s [output.h264] [listen_rtp_port] [listen_rtcp_port] [receive_seconds]\n\n",
           program);
    printf("Default:\n");
    printf("  output_file = %s\n", DEMO_DEFAULT_OUTPUT_FILE);
    printf("  rtp_port    = %d\n", DEMO_DEFAULT_RTP_PORT);
    printf("  rtcp_port   = %d\n", DEMO_DEFAULT_RTCP_PORT);
    printf("  seconds     = %d\n\n", DEMO_DEFAULT_RECEIVE_SECONDS);
}

static void receiver_interactive_config(char *output_file,
                                        size_t output_file_size,
                                        uint16_t *rtp_port,
                                        uint16_t *rtcp_port,
                                        uint32_t *receive_seconds)
{
    printf("H264 RTP receiver interactive terminal\n");
    printf("This program receives RTP packets and rebuilds a .h264 file.\n");
    printf("Press ENTER to use the default value shown in brackets.\n");

    console_print_step(1, "Input local receiver settings");
    console_read_line_default("Output H264 file in receiver folder",
                              DEMO_DEFAULT_OUTPUT_FILE,
                              output_file,
                              output_file_size);
    *rtp_port = (uint16_t)console_read_u32_default("Receiver local RTP port",
                                                   DEMO_DEFAULT_RTP_PORT);
    *rtcp_port = (uint16_t)console_read_u32_default("Receiver local RTCP port",
                                                    DEMO_DEFAULT_RTCP_PORT);
    *receive_seconds = console_read_u32_default("Receive seconds",
                                                DEMO_DEFAULT_RECEIVE_SECONDS);
    if (*receive_seconds == 0u) {
        *receive_seconds = DEMO_DEFAULT_RECEIVE_SECONDS;
    }
}

static int receiver_wait_for_pair(UdpEndpoint *rtcp_listener, uint32_t receiver_ssrc)
{
    uint8_t packet[256];

    console_print_step(3, "Wait for sender pairing");
    printf("Waiting for RTCP APP PAIR/HELLO from sender...\n");
    printf("After receiving HELLO, receiver will answer PAIR/WELCOME.\n\n");

    for (;;) {
        size_t received_size = 0;
        UdpPacketSource source;
        int ret;

        ret = udp_endpoint_recv_from(rtcp_listener,
                                     packet,
                                     sizeof(packet),
                                     &received_size,
                                     &source,
                                     1000);
        if (ret == 1) {
            printf("still waiting for sender HELLO...\n");
            continue;
        }
        if (ret < 0) {
            return -1;
        }

        rtcp_print_packet_type(packet, received_size, "recv");

        {
            char app_name[5];
            char text[48];
            uint32_t sender_ssrc = 0;

            if (rtcp_parse_app_message(packet,
                                       received_size,
                                       app_name,
                                       text,
                                       sizeof(text),
                                       &sender_ssrc) == 0 &&
                strcmp(app_name, "PAIR") == 0 &&
                strcmp(text, "HELLO") == 0) {
                udp_endpoint_set_remote(rtcp_listener, &source);
                printf("sender says HELLO, sender_ssrc=0x%08x\n", (unsigned)sender_ssrc);

                if (rtcp_send_app_message(rtcp_listener,
                                          receiver_ssrc,
                                          "PAIR",
                                          "WELCOME") != 0) {
                    fprintf(stderr, "failed to send RTCP APP WELCOME\n");
                    return -1;
                }
                printf("send RTCP APP: PAIR WELCOME\n");
                printf("paired OK, receiver is ready to receive RTP video\n");
                return 0;
            }
        }
    }
}

static int write_annexb_nal(FILE *out, const uint8_t *nal, size_t nal_size)
{
    static const uint8_t start_code[4] = {0x00, 0x00, 0x00, 0x01};

    if (out == NULL || nal == NULL || nal_size == 0) {
        return -1;
    }

    /*
     * RTP 里传的是“不带起始码”的 H264 NALU。
     * .h264 Annex-B 文件里需要恢复起始码，播放器和分析工具才容易识别。
     */
    if (fwrite(start_code, 1, sizeof(start_code), out) != sizeof(start_code)) {
        return -1;
    }
    if (fwrite(nal, 1, nal_size, out) != nal_size) {
        return -1;
    }
    return 0;
}

static int parse_rtp_packet(const uint8_t *packet,
                            size_t packet_size,
                            RtpPacketView *view)
{
    uint8_t version;
    uint8_t csrc_count;
    uint8_t has_extension;
    size_t header_size;

    if (packet == NULL || view == NULL || packet_size < 12) {
        return -1;
    }

    memset(view, 0, sizeof(*view));

    /*
     * RTP 第 1 个字节：
     *   bit 7..6: V，版本，必须是 2
     *   bit 5   : P，padding，demo 不使用
     *   bit 4   : X，扩展头标志
     *   bit 3..0: CC，CSRC 个数
     */
    version = (uint8_t)(packet[0] >> 6);
    has_extension = (uint8_t)((packet[0] >> 4) & 0x01);
    csrc_count = (uint8_t)(packet[0] & 0x0f);
    if (version != 2) {
        return -1;
    }

    header_size = 12u + (size_t)csrc_count * 4u;
    if (packet_size < header_size) {
        return -1;
    }

    /*
     * 如果存在 RTP extension header，格式是：
     *   16-bit profile id
     *   16-bit extension length，以 32-bit word 为单位
     * demo 的发送端不会生成扩展头；这里解析跳过，是为了让接收器更完整。
     */
    if (has_extension) {
        uint16_t extension_words;
        if (packet_size < header_size + 4u) {
            return -1;
        }
        extension_words = read_be16(packet + header_size + 2u);
        header_size += 4u + (size_t)extension_words * 4u;
        if (packet_size < header_size) {
            return -1;
        }
    }

    /*
     * RTP 第 2 个字节：
     *   bit 7   : M，marker，视频里常表示一帧的最后一个 RTP 包
     *   bit 6..0: PT，payload type。H264 常用动态类型 96。
     */
    view->marker = (uint8_t)(packet[1] >> 7);
    view->payload_type = (uint8_t)(packet[1] & 0x7f);
    view->sequence = read_be16(packet + 2);
    view->timestamp = read_be32(packet + 4);
    view->ssrc = read_be32(packet + 8);
    view->payload = packet + header_size;
    view->payload_size = packet_size - header_size;

    return view->payload_size == 0 ? -1 : 0;
}

static int depacketize_h264_payload(H264Depacketizer *dep,
                                    const RtpPacketView *rtp)
{
    uint8_t nal_type;

    if (dep == NULL || rtp == NULL || rtp->payload == NULL || rtp->payload_size == 0) {
        return -1;
    }

    /*
     * H264 over RTP 的 payload 第一个字节仍然可以看作 NAL header。
     * Type 决定它是哪种 RTP/H264 承载方式：
     *   1..23: Single NAL Unit，整个 NALU 就在这个 RTP 包里
     *   24   : STAP-A，一个 RTP 包里打包多个小 NALU
     *   28   : FU-A，一个大 NALU 被拆成多个 RTP 包
     */
    nal_type = (uint8_t)(rtp->payload[0] & 0x1f);

    if (nal_type >= 1 && nal_type <= 23) {
        if (write_annexb_nal(dep->out, rtp->payload, rtp->payload_size) != 0) {
            return -1;
        }
        dep->nal_count++;
        dep->fu_started = 0;
        printf("RTP seq=%5u ts=%10u M=%u -> Single NAL type=%2u size=%u\n",
               (unsigned)rtp->sequence,
               (unsigned)rtp->timestamp,
               (unsigned)rtp->marker,
               (unsigned)nal_type,
               (unsigned)rtp->payload_size);
        return 0;
    }

    if (nal_type == 24) {
        size_t offset = 1;

        /*
         * STAP-A 格式：
         *   1 byte  STAP-A header，type=24
         *   2 bytes NALU size
         *   N bytes NALU data
         *   2 bytes NALU size
         *   N bytes NALU data
         *   ...
         *
         * SPS/PPS 有时会被打到同一个 STAP-A 里。
         */
        while (offset + 2u <= rtp->payload_size) {
            uint16_t nal_size = read_be16(rtp->payload + offset);
            offset += 2u;
            if (nal_size == 0 || offset + nal_size > rtp->payload_size) {
                return -1;
            }
            if (write_annexb_nal(dep->out, rtp->payload + offset, nal_size) != 0) {
                return -1;
            }
            dep->nal_count++;
            offset += nal_size;
        }

        dep->stap_a_count++;
        dep->fu_started = 0;
        printf("RTP seq=%5u ts=%10u M=%u -> STAP-A packet\n",
               (unsigned)rtp->sequence,
               (unsigned)rtp->timestamp,
               (unsigned)rtp->marker);
        return 0;
    }

    if (nal_type == 28) {
        uint8_t fu_indicator;
        uint8_t fu_header;
        uint8_t start_bit;
        uint8_t end_bit;
        uint8_t original_type;
        uint8_t reconstructed_nal_header;
        const uint8_t *fragment_data;
        size_t fragment_size;
        static const uint8_t start_code[4] = {0x00, 0x00, 0x00, 0x01};

        if (rtp->payload_size < 3) {
            return -1;
        }

        fu_indicator = rtp->payload[0];
        fu_header = rtp->payload[1];
        start_bit = (uint8_t)((fu_header >> 7) & 0x01);
        end_bit = (uint8_t)((fu_header >> 6) & 0x01);
        original_type = (uint8_t)(fu_header & 0x1f);
        fragment_data = rtp->payload + 2;
        fragment_size = rtp->payload_size - 2u;

        /*
         * FU-A 会把原始 NAL header 拆开保存：
         *   FU indicator 保存 F 和 NRI，但 Type 改成 28。
         *   FU header 保存 start/end 标志和原始 Type。
         *
         * 重组时要恢复原始 NAL header：
         *   F/NRI 来自 FU indicator
         *   Type 来自 FU header
         */
        reconstructed_nal_header = (uint8_t)((fu_indicator & 0xe0) | original_type);

        if (start_bit) {
            if (fwrite(start_code, 1, sizeof(start_code), dep->out) != sizeof(start_code)) {
                return -1;
            }
            if (fwrite(&reconstructed_nal_header, 1, 1, dep->out) != 1) {
                return -1;
            }
            dep->fu_started = 1;
        } else if (!dep->fu_started) {
            /*
             * 如果中间分片先到了，说明前面的 start 分片丢了。
             * 没有 NAL header 就无法恢复完整 NALU，所以丢弃这一片。
             */
            printf("RTP seq=%5u -> FU-A middle fragment ignored, start fragment missing\n",
                   (unsigned)rtp->sequence);
            return 0;
        }

        if (fwrite(fragment_data, 1, fragment_size, dep->out) != fragment_size) {
            return -1;
        }

        dep->fu_packet_count++;
        if (end_bit) {
            dep->fu_started = 0;
            dep->nal_count++;
        }

        printf("RTP seq=%5u ts=%10u M=%u -> FU-A type=%2u S=%u E=%u fragment=%u\n",
               (unsigned)rtp->sequence,
               (unsigned)rtp->timestamp,
               (unsigned)rtp->marker,
               (unsigned)original_type,
               (unsigned)start_bit,
               (unsigned)end_bit,
               (unsigned)fragment_size);
        return 0;
    }

    printf("RTP seq=%5u -> unsupported H264 RTP payload type=%u\n",
           (unsigned)rtp->sequence,
           (unsigned)nal_type);
    return 0;
}

static void update_receiver_stats(ReceiverStats *stats, const RtpPacketView *rtp)
{
    if (stats == NULL || rtp == NULL) {
        return;
    }

    stats->sender_ssrc = rtp->ssrc;
    stats->packets_received++;

    /*
     * RTCP RR 里的 highest sequence number 真实规范要求“扩展序号”：
     * 需要处理 16-bit sequence 回绕。
     * demo 为了让新手先看懂主流程，先假设短时间测试不会回绕。
     */
    if (!stats->have_sequence) {
        stats->base_sequence = rtp->sequence;
        stats->highest_sequence = rtp->sequence;
        stats->have_sequence = 1;
    } else if (rtp->sequence > stats->highest_sequence) {
        stats->highest_sequence = rtp->sequence;
    }
}

int main(int argc, char **argv)
{
    char output_file_buf[256] = DEMO_DEFAULT_OUTPUT_FILE;
    const char *output_file = output_file_buf;
    uint16_t rtp_port = DEMO_DEFAULT_RTP_PORT;
    uint16_t rtcp_port = DEMO_DEFAULT_RTCP_PORT;
    uint32_t receive_seconds = DEMO_DEFAULT_RECEIVE_SECONDS;
    FILE *out = NULL;
    UdpEndpoint rtp_listener;
    UdpEndpoint rtcp_listener;
    H264Depacketizer dep;
    ReceiverStats stats;
    uint8_t packet[DEMO_RTP_MTU + 256u];
    uint64_t start_ms;
    uint64_t last_rr_ms = 0;
    int exit_code = 1;
    int interactive = (argc == 1);

    if (interactive) {
        receiver_interactive_config(output_file_buf,
                                    sizeof(output_file_buf),
                                    &rtp_port,
                                    &rtcp_port,
                                    &receive_seconds);
    } else if (argc > 1) {
        output_file = argv[1];
    }
    if (argc > 2) {
        rtp_port = (uint16_t)parse_u32_or_default(argv[2], DEMO_DEFAULT_RTP_PORT);
    }
    if (argc > 3) {
        rtcp_port = (uint16_t)parse_u32_or_default(argv[3], DEMO_DEFAULT_RTCP_PORT);
    }
    if (argc > 4) {
        receive_seconds = parse_u32_or_default(argv[4], DEMO_DEFAULT_RECEIVE_SECONDS);
    }
    if (argc > 5) {
        print_usage(argv[0]);
        if (interactive) {
            console_pause("Press ENTER to exit...");
        }
        return 1;
    }

    console_print_step(2, "Open output file and listen UDP ports");
    printf("RTP/RTCP H264 receiver demo\n");
    printf("listen RTP : UDP %u\n", (unsigned)rtp_port);
    printf("listen RTCP: UDP %u\n", (unsigned)rtcp_port);
    printf("write file : %s\n", output_file);
    printf("duration   : %u seconds, use Ctrl+C to stop earlier\n\n",
           (unsigned)receive_seconds);

    out = fopen(output_file, "wb");
    if (out == NULL) {
        fprintf(stderr, "failed to open output file: %s\n", output_file);
        goto cleanup;
    }

    if (platform_net_init() != 0) {
        goto cleanup;
    }

    rtp_listener.fd = INVALID_SOCKET;
    rtcp_listener.fd = INVALID_SOCKET;

    if (udp_endpoint_bind(&rtp_listener, rtp_port) != 0) {
        goto cleanup_net;
    }
    if (udp_endpoint_bind(&rtcp_listener, rtcp_port) != 0) {
        goto cleanup_net;
    }

    memset(&dep, 0, sizeof(dep));
    dep.out = out;

    memset(&stats, 0, sizeof(stats));
    stats.receiver_ssrc = 0x87654321u;

    if (receiver_wait_for_pair(&rtcp_listener, stats.receiver_ssrc) != 0) {
        goto cleanup_net;
    }

    console_print_step(4, "Receive RTP and rebuild H264 file");
    start_ms = platform_time_ms();

    while (platform_time_ms() - start_ms < (uint64_t)receive_seconds * 1000u) {
        size_t received_size = 0;
        UdpPacketSource source;
        int ret;

        /*
         * 先等 RTP。这里用 20ms 超时，是为了在没有视频包时也能定期检查 RTCP。
         * 真正产品里通常用 select/poll 同时监听多个 socket。
         */
        ret = udp_endpoint_recv_from(&rtp_listener,
                                     packet,
                                     sizeof(packet),
                                     &received_size,
                                     &source,
                                     20);
        if (ret == 0) {
            RtpPacketView rtp;
            if (parse_rtp_packet(packet, received_size, &rtp) == 0) {
                update_receiver_stats(&stats, &rtp);
                if (depacketize_h264_payload(&dep, &rtp) != 0) {
                    fprintf(stderr, "failed to depacketize H264 RTP payload\n");
                    goto cleanup_net;
                }
            } else {
                printf("received invalid RTP packet, size=%u\n", (unsigned)received_size);
            }
        } else if (ret < 0) {
            goto cleanup_net;
        }

        /*
         * 再用非阻塞方式检查 RTCP。
         * 收到发送端 SR 后，把 RTCP remote 设置成 SR 的来源地址，然后回 RR。
         */
        ret = udp_endpoint_recv_from(&rtcp_listener,
                                     packet,
                                     sizeof(packet),
                                     &received_size,
                                     &source,
                                     0);
        if (ret == 0) {
            RtcpSenderReport sr;
            udp_endpoint_set_remote(&rtcp_listener, &source);
            rtcp_print_packet_type(packet, received_size, "recv");

            if (rtcp_parse_sender_report(packet, received_size, &sr) == 0) {
                RtcpReceiverState rr;
                memset(&rr, 0, sizeof(rr));
                rr.receiver_ssrc = stats.receiver_ssrc;
                rr.sender_ssrc = sr.sender_ssrc;
                rr.base_sequence = stats.base_sequence;
                rr.highest_sequence = stats.highest_sequence;
                rr.packets_received = stats.packets_received;
                rr.packets_lost = 0;
                rr.jitter = 0;

                printf("  SR detail: sender_ssrc=0x%08x rtp_ts=%u packets=%u octets=%u\n",
                       (unsigned)sr.sender_ssrc,
                       (unsigned)sr.rtp_timestamp,
                       (unsigned)sr.packet_count,
                       (unsigned)sr.octet_count);

                if (rtcp_send_receiver_report(&rtcp_listener, &rr) != 0) {
                    fprintf(stderr, "failed to send RTCP RR\n");
                    goto cleanup_net;
                }
                last_rr_ms = platform_time_ms();
                printf("send RTCP Receiver Report (RR): received=%u highest_seq=%u\n",
                       (unsigned)stats.packets_received,
                       (unsigned)stats.highest_sequence);
            }
        } else if (ret < 0) {
            goto cleanup_net;
        }

        /*
         * 如果发送端还没发 SR，接收端不知道 RTCP 对端地址，不能主动发 RR。
         * 一旦收到过 SR，就可以每 5 秒回一次 RR。
         */
        if (rtcp_listener.remote_port_be != 0 &&
            platform_time_ms() - last_rr_ms >= DEMO_RTCP_SR_INTERVAL_MS) {
            RtcpReceiverState rr;
            memset(&rr, 0, sizeof(rr));
            rr.receiver_ssrc = stats.receiver_ssrc;
            rr.sender_ssrc = stats.sender_ssrc;
            rr.base_sequence = stats.base_sequence;
            rr.highest_sequence = stats.highest_sequence;
            rr.packets_received = stats.packets_received;

            if (rtcp_send_receiver_report(&rtcp_listener, &rr) != 0) {
                fprintf(stderr, "failed to send periodic RTCP RR\n");
                goto cleanup_net;
            }
            last_rr_ms = platform_time_ms();
            printf("send periodic RTCP Receiver Report (RR): received=%u highest_seq=%u\n",
                   (unsigned)stats.packets_received,
                   (unsigned)stats.highest_sequence);
        }
    }

    fflush(out);
    printf("\nreceiver summary:\n");
    printf("  RTP packets received : %u\n", (unsigned)stats.packets_received);
    printf("  H264 NALUs written   : %u\n", (unsigned)dep.nal_count);
    printf("  FU-A packets handled : %u\n", (unsigned)dep.fu_packet_count);
    printf("  STAP-A packets       : %u\n", (unsigned)dep.stap_a_count);
    printf("  output file          : %s\n", output_file);
    exit_code = 0;

cleanup_net:
    udp_endpoint_close(&rtp_listener);
    udp_endpoint_close(&rtcp_listener);
    platform_net_deinit();

cleanup:
    if (out != NULL) {
        fclose(out);
    }
    if (interactive) {
        console_pause("Press ENTER to exit...");
    }
    return exit_code;
}
