#ifndef DEMO_CONFIG_H
#define DEMO_CONFIG_H

/*
 * 这个文件集中放 demo 的可配置参数。
 *
 * 移植到小内存平台时，可以优先调整这些宏：
 * - DEMO_MAX_NAL_SIZE: 单个 NALU 最大缓存。
 * - DEMO_RTP_MTU: RTP 包最大网络包大小。
 *
 * 真实 IPC 中，编码器通常可以按帧给出 NALU 指针和长度，此时可以绕过
 * 文件读取模块，直接调用 rtp_h264_send_nal()。
 */

#define DEMO_DEFAULT_INPUT_FILE "output.h264"
#define DEMO_DEFAULT_DEST_IP "127.0.0.1"
#define DEMO_DEFAULT_RTP_PORT 5004
#define DEMO_DEFAULT_RTCP_PORT 5005
#define DEMO_DEFAULT_SENDER_RTCP_PORT 5007
#define DEMO_DEFAULT_FPS 25
#define DEMO_DEFAULT_RECEIVE_SECONDS 20
#define DEMO_DEFAULT_OUTPUT_FILE "received_output.h264"

/* RTP 一般跑在 UDP 上。为了避免 IP 分片，负载不要超过常见 MTU。 */
#define DEMO_RTP_MTU 1200

/* RTP 固定头 12 字节，所以 H264 RTP payload 最大为 MTU - 12。 */
#define DEMO_RTP_HEADER_SIZE 12
#define DEMO_RTP_MAX_PAYLOAD_SIZE (DEMO_RTP_MTU - DEMO_RTP_HEADER_SIZE)

/*
 * demo 使用固定 NALU 缓冲区，不把整个文件读到内存。
 * 如果你的测试码流有超大 IDR 帧，可以适当调大。
 */
#define DEMO_MAX_NAL_SIZE (512u * 1024u)

/* H264 over RTP 常用动态 payload type。SDP 里通常写成 a=rtpmap:96 H264/90000。 */
#define DEMO_RTP_PAYLOAD_TYPE 96
#define DEMO_RTP_CLOCK_RATE 90000u

/* 每隔多少毫秒发一个 RTCP Sender Report。 */
#define DEMO_RTCP_SR_INTERVAL_MS 5000u

#endif
