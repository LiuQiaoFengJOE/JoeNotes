#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console_helper.h"
#include "demo_config.h"
#include "h264_annexb.h"
#include "platform_net.h"
#include "rtcp.h"
#include "rtp_h264.h"

static void print_usage(const char *program)
{
    printf("Usage:\n");
    printf("  %s [input.h264] [dest_ip] [rtp_port] [receiver_rtcp_port] [fps] [local_rtcp_port]\n\n",
           program);
    printf("Default:\n");
    printf("  input              = %s\n", DEMO_DEFAULT_INPUT_FILE);
    printf("  dest_ip            = %s\n", DEMO_DEFAULT_DEST_IP);
    printf("  rtp_port           = %d\n", DEMO_DEFAULT_RTP_PORT);
    printf("  receiver_rtcp_port = %d\n", DEMO_DEFAULT_RTCP_PORT);
    printf("  fps                = %d\n", DEMO_DEFAULT_FPS);
    printf("  local_rtcp_port    = %d\n\n", DEMO_DEFAULT_SENDER_RTCP_PORT);
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

static void sender_interactive_config(char *input_file,
                                      size_t input_file_size,
                                      char *dest_ip,
                                      size_t dest_ip_size,
                                      uint16_t *rtp_port,
                                      uint16_t *rtcp_port,
                                      uint16_t *local_rtcp_port,
                                      uint32_t *fps)
{
    printf("H264 RTP sender interactive terminal\n");
    printf("This program reads a local .h264 file and sends it to receiver by RTP.\n");
    printf("Press ENTER to use the default value shown in brackets.\n");

    console_print_step(1, "Input local sender settings");
    console_read_line_default("Input H264 file in sender folder",
                              DEMO_DEFAULT_INPUT_FILE,
                              input_file,
                              input_file_size);
    console_read_line_default("Receiver IP address",
                              DEMO_DEFAULT_DEST_IP,
                              dest_ip,
                              dest_ip_size);
    *rtp_port = (uint16_t)console_read_u32_default("Receiver RTP port",
                                                   DEMO_DEFAULT_RTP_PORT);
    *rtcp_port = (uint16_t)console_read_u32_default("Receiver RTCP port",
                                                    DEMO_DEFAULT_RTCP_PORT);
    *local_rtcp_port = (uint16_t)console_read_u32_default("Sender local RTCP port",
                                                          DEMO_DEFAULT_SENDER_RTCP_PORT);
    *fps = console_read_u32_default("Simulated video fps", DEMO_DEFAULT_FPS);
    if (*fps == 0u) {
        *fps = DEMO_DEFAULT_FPS;
    }
}

static int sender_pair_with_receiver(UdpEndpoint *rtcp_endpoint, uint32_t sender_ssrc)
{
    uint64_t start_ms;
    uint64_t last_hello_ms = 0;
    uint8_t packet[256];

    /*
     * 这个 demo 额外加了一个 APP 握手，目的是让新手看清楚“RTCP 也能带自定义控制消息”。
     * 真正工程里通常会用 RTSP/SDP、信令服务器或更完整的会话管理。
     */
    console_print_step(3, "Pair with receiver by RTCP APP");
    printf("Sender will send RTCP APP PAIR/HELLO.\n");
    printf("Receiver should answer RTCP APP PAIR/WELCOME.\n");
    printf("This is a learning handshake, not RTSP and not TCP connect.\n\n");

    start_ms = platform_time_ms();
    while (platform_time_ms() - start_ms < 10000u) {
        size_t received_size = 0;
        UdpPacketSource source;
        int ret;

        /*
         * 每隔 1 秒重发一次 HELLO。
         * 为什么要重发？
         * 因为 UDP 不保证对方一定收到，学习阶段用“重复尝试”最直观。
         */
        if (platform_time_ms() - last_hello_ms >= 1000u) {
            if (rtcp_send_app_message(rtcp_endpoint, sender_ssrc, "PAIR", "HELLO") != 0) {
                fprintf(stderr, "failed to send RTCP APP HELLO\n");
                return -1;
            }
            printf("send RTCP APP: PAIR HELLO\n");
            last_hello_ms = platform_time_ms();
        }

        ret = udp_endpoint_recv_from(rtcp_endpoint,
                                     packet,
                                     sizeof(packet),
                                     &received_size,
                                     &source,
                                     200);
        if (ret == 0) {
            char app_name[5];
            char text[48];
            uint32_t peer_ssrc = 0;

            rtcp_print_packet_type(packet, received_size, "recv");
            if (rtcp_parse_app_message(packet,
                                       received_size,
                                       app_name,
                                       text,
                                       sizeof(text),
                                       &peer_ssrc) == 0 &&
                strcmp(app_name, "PAIR") == 0 &&
                strcmp(text, "WELCOME") == 0) {
                printf("paired OK, receiver_ssrc=0x%08x\n", (unsigned)peer_ssrc);
                return 0;
            }
        } else if (ret < 0) {
            return -1;
        }
    }

    printf("pair timeout: receiver did not answer WELCOME in 10 seconds\n");
    return -1;
}

int main(int argc, char **argv)
{
    char input_file_buf[256] = DEMO_DEFAULT_INPUT_FILE;
    char dest_ip_buf[64] = DEMO_DEFAULT_DEST_IP;
    const char *input_file = input_file_buf;
    const char *dest_ip = dest_ip_buf;
    uint16_t rtp_port = DEMO_DEFAULT_RTP_PORT;
    uint16_t rtcp_port = DEMO_DEFAULT_RTCP_PORT;
    uint16_t local_rtcp_port = DEMO_DEFAULT_SENDER_RTCP_PORT;
    uint32_t fps = DEMO_DEFAULT_FPS;
    uint32_t timestamp_step;
    uint8_t *nal_buf = NULL;
    FILE *fp = NULL;
    UdpEndpoint rtp_endpoint;
    UdpEndpoint rtcp_endpoint;
    RtpH264Session rtp_session;
    uint64_t last_rtcp_ms;
    uint32_t nal_index = 0;
    int exit_code = 1;
    int interactive = (argc == 1);

    /*
     * 交互模式下让新手直接在终端里输入参数。
     * 命令行模式则更适合脚本和批处理。
     */
    if (interactive) {
        sender_interactive_config(input_file_buf,
                                  sizeof(input_file_buf),
                                  dest_ip_buf,
                                  sizeof(dest_ip_buf),
                                  &rtp_port,
                                  &rtcp_port,
                                  &local_rtcp_port,
                                  &fps);
    } else if (argc > 1) {
        input_file = argv[1];
    }
    if (argc > 2) {
        dest_ip = argv[2];
    }
    if (argc > 3) {
        rtp_port = (uint16_t)parse_u32_or_default(argv[3], DEMO_DEFAULT_RTP_PORT);
    }
    if (argc > 4) {
        rtcp_port = (uint16_t)parse_u32_or_default(argv[4], DEMO_DEFAULT_RTCP_PORT);
    }
    if (argc > 5) {
        fps = parse_u32_or_default(argv[5], DEMO_DEFAULT_FPS);
        if (fps == 0) {
            fps = DEMO_DEFAULT_FPS;
        }
    }
    if (argc > 6) {
        local_rtcp_port = (uint16_t)parse_u32_or_default(argv[6],
                                                         DEMO_DEFAULT_SENDER_RTCP_PORT);
    }

    if (argc > 7) {
        print_usage(argv[0]);
        if (interactive) {
            console_pause("Press ENTER to exit...");
        }
        return 1;
    }

    /*
     * RTP timestamp 的步长要和 fps 对应。
     * 视频常用 90000Hz 时钟：25fps 时每帧步长就是 90000 / 25 = 3600。
     */
    timestamp_step = DEMO_RTP_CLOCK_RATE / fps;

    console_print_step(2, "Open input file and UDP sockets");
    printf("H264 to RTP/RTCP sender demo\n");
    printf("input: %s\n", input_file);
    printf("dest : %s RTP=%u receiver_RTCP=%u fps=%u local_RTCP=%u\n",
           dest_ip,
           (unsigned)rtp_port,
           (unsigned)rtcp_port,
           (unsigned)fps,
           (unsigned)local_rtcp_port);
    printf("RTP payload type=%u clock=%uHz timestamp_step=%u\n\n",
           (unsigned)DEMO_RTP_PAYLOAD_TYPE,
           (unsigned)DEMO_RTP_CLOCK_RATE,
           (unsigned)timestamp_step);

    fp = fopen(input_file, "rb");
    if (fp == NULL) {
        fprintf(stderr, "failed to open input file: %s\n", input_file);
        goto cleanup;
    }

    nal_buf = (uint8_t *)malloc(DEMO_MAX_NAL_SIZE);
    if (nal_buf == NULL) {
        fprintf(stderr, "failed to allocate NAL buffer\n");
        goto cleanup;
    }

    if (platform_net_init() != 0) {
        goto cleanup;
    }

    rtp_endpoint.fd = INVALID_SOCKET;
    rtcp_endpoint.fd = INVALID_SOCKET;

    if (udp_endpoint_open(&rtp_endpoint, dest_ip, rtp_port) != 0) {
        goto cleanup_net;
    }

    /*
     * RTCP 要使用同一个 UDP socket 完成“发 SR”和“收 RR”。
     *
     * 为什么？
     * UDP 没有连接，接收端收到 SR 后，会把 RR 回到 SR 的源地址和源端口。
     * 如果发送端发 SR 用临时端口，却在另一个端口等 RR，就收不到回包。
     *
     * 所以这里先 bind 固定本地 RTCP 端口，再设置远端 RTCP 地址。
     */
    if (udp_endpoint_bind(&rtcp_endpoint, local_rtcp_port) != 0) {
        goto cleanup_net;
    }
    if (udp_endpoint_set_remote_ip_port(&rtcp_endpoint, dest_ip, rtcp_port) != 0) {
        goto cleanup_net;
    }

    /*
     * SSRC 是 RTP 会话里的同步源标识。
     * 真正设备里通常随机生成，确保同一会话内唯一。
     * 这里固定一个值，方便 Wireshark 观察。
     */
    rtp_h264_session_init(&rtp_session,
                          &rtp_endpoint,
                          0x12345678u,
                          1000u,
                          0u,
                          DEMO_RTP_PAYLOAD_TYPE,
                          DEMO_RTP_MAX_PAYLOAD_SIZE);

    if (sender_pair_with_receiver(&rtcp_endpoint, rtp_session.ssrc) != 0) {
        goto cleanup_net;
    }

    console_print_step(4, "Send H264 as RTP packets");
    last_rtcp_ms = 0;

    for (;;) {
        H264ReadResult read_result;
        size_t nal_size = 0;
        uint8_t nal_type;
        int is_vcl;
        int marker;

        /*
         * 从 Annex-B 文件里读出一个完整 NALU。
         * 返回的数据不带起始码，后面可以直接塞进 RTP。
         */
        read_result = h264_annexb_read_next_nal(fp, nal_buf, DEMO_MAX_NAL_SIZE, &nal_size);
        if (read_result == H264_READ_EOF) {
            printf("\nend of file\n");
            break;
        }
        if (read_result == H264_READ_NAL_TOO_LARGE) {
            fprintf(stderr, "NALU is larger than DEMO_MAX_NAL_SIZE=%u\n",
                    (unsigned)DEMO_MAX_NAL_SIZE);
            goto cleanup_net;
        }
        if (read_result != H264_READ_OK) {
            fprintf(stderr, "failed to read H264 NALU\n");
            goto cleanup_net;
        }

        nal_index++;
        nal_type = (uint8_t)(nal_buf[0] & 0x1f);
        is_vcl = h264_nal_is_vcl(nal_type);

        /*
         * marker bit 常被用来标记“一帧最后一个 RTP 包”。
         * 这里为了教学简单：
         * - SPS/PPS/SEI/AUD 等非图像 NALU：marker=0
         * - 图像切片 NALU：marker=1
         *
         * 如果一个真实编码器一帧里有多个 slice，严格做法是只给最后一个 slice 置 1。
         */
        marker = is_vcl ? 1 : 0;

        printf("NAL %6u: type=%2u %-14s size=%7u timestamp=%10u\n",
               (unsigned)nal_index,
               (unsigned)nal_type,
               h264_nal_type_name(nal_type),
               (unsigned)nal_size,
               (unsigned)rtp_session.timestamp);

        if (rtp_h264_send_nal(&rtp_session, nal_buf, nal_size, marker) != 0) {
            fprintf(stderr, "failed to send RTP packet\n");
            goto cleanup_net;
        }

        /*
         * 每隔固定时间发一次 RTCP SR。
         * SR 的作用是告诉接收端：
         * 1. 我是谁（SSRC）
         * 2. 现在的墙钟时间是多少（NTP）
         * 3. 当前对应的 RTP timestamp 是多少
         * 4. 我已经发了多少包和多少字节
         */
        if (platform_time_ms() - last_rtcp_ms >= DEMO_RTCP_SR_INTERVAL_MS) {
            RtcpSenderState sr_state;
            sr_state.ssrc = rtp_session.ssrc;
            sr_state.packet_count = rtp_session.packet_count;
            sr_state.octet_count = rtp_session.octet_count;

            if (rtcp_send_sender_report(&rtcp_endpoint, &sr_state, rtp_session.timestamp) != 0) {
                fprintf(stderr, "failed to send RTCP sender report\n");
                goto cleanup_net;
            }
            last_rtcp_ms = platform_time_ms();
            printf("  RTCP SR sent: packets=%u octets=%u\n",
                   (unsigned)sr_state.packet_count,
                   (unsigned)sr_state.octet_count);
        }

        /*
         * 发送端也顺手监听自己的 RTCP 端口。
         * 正常 RTP 会话里：
         * - sender 周期性发 SR
         * - receiver 周期性发 RR
         *
         * 这里不做完整 RR 解析，只打印包类型，让你先看到“回路是通的”。
         */
        {
            uint8_t rtcp_packet[1500];
            size_t rtcp_size = 0;
            UdpPacketSource rtcp_source;
            int rtcp_ret = udp_endpoint_recv_from(&rtcp_endpoint,
                                                  rtcp_packet,
                                                  sizeof(rtcp_packet),
                                                  &rtcp_size,
                                                  &rtcp_source,
                                                  0);
            if (rtcp_ret == 0) {
                rtcp_print_packet_type(rtcp_packet, rtcp_size, "recv");
            } else if (rtcp_ret < 0) {
                goto cleanup_net;
            }
        }

        /*
         * 真实流媒体里，timestamp 通常和真实帧率对齐；
         * 这里用 sleep 模拟一个比较稳定的发送节奏。
         */
        if (is_vcl) {
            rtp_h264_add_timestamp(&rtp_session, timestamp_step);
            platform_sleep_ms(1000u / fps);
        }
    }

    printf("\nsummary: RTP packets=%u RTP payload octets=%u\n",
           (unsigned)rtp_session.packet_count,
           (unsigned)rtp_session.octet_count);
    exit_code = 0;

cleanup_net:
    udp_endpoint_close(&rtp_endpoint);
    udp_endpoint_close(&rtcp_endpoint);
    platform_net_deinit();

cleanup:
    if (fp != NULL) {
        fclose(fp);
    }
    free(nal_buf);
    if (interactive) {
        console_pause("Press ENTER to exit...");
    }
    return exit_code;
}
