#ifndef DEMO_CONFIG_H
#define DEMO_CONFIG_H

/*
 * 这里集中放 demo 的默认参数。
 *
 * 这些值不是协议规定死的，而是为了“开箱即跑”和学习调试方便。
 * 真正接到 IPC、SDK 或嵌入式平台时，最常调整的就是这些地方。
 */

#define DEMO_DEFAULT_INPUT_FILE "output.h264"
#define DEMO_DEFAULT_DEST_IP "127.0.0.1"
#define DEMO_DEFAULT_RTP_PORT 5004
#define DEMO_DEFAULT_RTCP_PORT 5005
#define DEMO_DEFAULT_SENDER_RTCP_PORT 5007
#define DEMO_DEFAULT_FPS 25
#define DEMO_DEFAULT_RECEIVE_SECONDS 20
#define DEMO_DEFAULT_OUTPUT_FILE "received_output.h264"

/* RTP 一般跑在 UDP 上。为了减少 IP 分片，单个 RTP 包不要太大。 */
#define DEMO_RTP_MTU 1200

/* RTP 固定头 12 字节，所以 H264 真正能放进 payload 的长度要减 12。 */
#define DEMO_RTP_HEADER_SIZE 12
#define DEMO_RTP_MAX_PAYLOAD_SIZE (DEMO_RTP_MTU - DEMO_RTP_HEADER_SIZE)

/*
 * demo 使用固定 NAL 缓冲区，不把整个文件一次性读进内存。
 * 如果测试码流里的单个 NALU 很大，可以把这里调大。
 */
#define DEMO_MAX_NAL_SIZE (512u * 1024u)

/* H264 over RTP 常用动态 payload type。 */
#define DEMO_RTP_PAYLOAD_TYPE 96
#define DEMO_RTP_CLOCK_RATE 90000u

/* 多久发一次 RTCP Sender Report。 */
#define DEMO_RTCP_SR_INTERVAL_MS 5000u

#endif
