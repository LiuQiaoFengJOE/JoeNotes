#ifndef PLATFORM_NET_H
#define PLATFORM_NET_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
/*
 * 某些 Windows API 需要在包含 winsock/windows 头文件前先声明目标版本。
 * 这里保守设成 Vista 及以上，方便使用 inet_pton / GetTickCount64 等接口。
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET platform_socket_handle_t;
#else
typedef int platform_socket_handle_t;
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif

typedef struct UdpEndpoint {
    platform_socket_handle_t fd;
    uint32_t remote_ip_be;
    uint16_t remote_port_be;
} UdpEndpoint;

/*
 * UDP 收包时记录“这个包是谁发来的”。
 * 字段名里的 be 表示 big-endian / network byte order，也就是网络字节序。
 */
typedef struct UdpPacketSource {
    uint32_t ip_be;
    uint16_t port_be;
} UdpPacketSource;

int platform_net_init(void);
void platform_net_deinit(void);

int udp_endpoint_open(UdpEndpoint *endpoint, const char *ip, uint16_t port);
int udp_endpoint_bind(UdpEndpoint *endpoint, uint16_t local_port);
void udp_endpoint_set_remote(UdpEndpoint *endpoint, const UdpPacketSource *source);
int udp_endpoint_set_remote_ip_port(UdpEndpoint *endpoint, const char *ip, uint16_t port);
int udp_endpoint_send(const UdpEndpoint *endpoint, const uint8_t *data, size_t size);
int udp_endpoint_recv_from(const UdpEndpoint *endpoint,
                           uint8_t *data,
                           size_t capacity,
                           size_t *received_size,
                           UdpPacketSource *source,
                           uint32_t timeout_ms);
void udp_endpoint_close(UdpEndpoint *endpoint);

uint64_t platform_time_ms(void);
void platform_sleep_ms(uint32_t ms);

/*
 * 获取 NTP 时间戳，供 RTCP Sender Report 使用。
 * NTP 时间 = 1900 年以来的秒数 + 小数秒。
 */
void platform_ntp_now(uint32_t *ntp_sec, uint32_t *ntp_frac);

#endif
