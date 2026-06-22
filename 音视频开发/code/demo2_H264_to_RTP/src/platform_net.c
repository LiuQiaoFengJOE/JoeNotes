#include "platform_net.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

int platform_net_init(void)
{
#ifdef _WIN32
    WSADATA wsa_data;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (ret != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", ret);
        return -1;
    }
#endif
    return 0;
}

void platform_net_deinit(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

int udp_endpoint_open(UdpEndpoint *endpoint, const char *ip, uint16_t port)
{
    if (endpoint == NULL || ip == NULL) {
        return -1;
    }

    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (endpoint->fd == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed\n");
        return -1;
    }

    if (udp_endpoint_set_remote_ip_port(endpoint, ip, port) != 0) {
        udp_endpoint_close(endpoint);
        return -1;
    }
    return 0;
}

int udp_endpoint_bind(UdpEndpoint *endpoint, uint16_t local_port)
{
    struct sockaddr_in local_addr;

    if (endpoint == NULL) {
        return -1;
    }

    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (endpoint->fd == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed\n");
        return -1;
    }

    /*
     * 绑定本地端口，接收端必须做这一步。
     *
     * 发送端如果不 bind，系统会自动分配一个临时端口；
     * 接收端如果不 bind，就没有固定端口给别人发包。
     */
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(local_port);

    if (bind(endpoint->fd,
             (const struct sockaddr *)&local_addr,
             (int)sizeof(local_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed, port=%u\n", (unsigned)local_port);
        udp_endpoint_close(endpoint);
        return -1;
    }

    return 0;
}

void udp_endpoint_set_remote(UdpEndpoint *endpoint, const UdpPacketSource *source)
{
    if (endpoint == NULL || source == NULL) {
        return;
    }

    endpoint->remote_ip_be = source->ip_be;
    endpoint->remote_port_be = source->port_be;
}

int udp_endpoint_set_remote_ip_port(UdpEndpoint *endpoint, const char *ip, uint16_t port)
{
    unsigned long addr;

    if (endpoint == NULL || ip == NULL) {
        return -1;
    }

    /*
     * 为了兼容较老的 MinGW，这里使用 inet_addr()。
     * demo 只接收 IPv4 点分十进制地址，例如 127.0.0.1。
     */
    addr = inet_addr(ip);
    if (addr == INADDR_NONE) {
        fprintf(stderr, "invalid IPv4 address: %s\n", ip);
        return -1;
    }

    endpoint->remote_ip_be = addr;
    endpoint->remote_port_be = htons(port);
    return 0;
}

int udp_endpoint_send(const UdpEndpoint *endpoint, const uint8_t *data, size_t size)
{
    struct sockaddr_in remote_addr;
    int sent;

    if (endpoint == NULL || data == NULL || endpoint->fd == INVALID_SOCKET) {
        return -1;
    }

    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = endpoint->remote_ip_be;
    remote_addr.sin_port = endpoint->remote_port_be;

    sent = sendto(endpoint->fd,
                  (const char *)data,
                  (int)size,
                  0,
                  (const struct sockaddr *)&remote_addr,
                  (int)sizeof(remote_addr));
    if (sent == SOCKET_ERROR || sent != (int)size) {
        fprintf(stderr, "sendto() failed\n");
        return -1;
    }

    return 0;
}

int udp_endpoint_recv_from(const UdpEndpoint *endpoint,
                           uint8_t *data,
                           size_t capacity,
                           size_t *received_size,
                           UdpPacketSource *source,
                           uint32_t timeout_ms)
{
    fd_set read_set;
    struct timeval tv;
    struct timeval *tv_ptr = NULL;
    struct sockaddr_in from_addr;
    int from_len = (int)sizeof(from_addr);
    int ready;
    int received;

    if (received_size != NULL) {
        *received_size = 0;
    }

    if (endpoint == NULL || data == NULL || capacity == 0 || endpoint->fd == INVALID_SOCKET) {
        return -1;
    }

    /*
     * select() 用来做“带超时的等待”。
     *
     * timeout_ms = 0  表示立刻检查一次，不阻塞。
     * timeout_ms > 0  表示最多等这么久。
     *
     * Windows 的 select() 第一个参数会被忽略；Linux/Unix 需要 fd+1。
     */
    FD_ZERO(&read_set);
    FD_SET(endpoint->fd, &read_set);
    tv.tv_sec = (long)(timeout_ms / 1000u);
    tv.tv_usec = (long)((timeout_ms % 1000u) * 1000u);
    tv_ptr = &tv;

#ifdef _WIN32
    ready = select(0, &read_set, NULL, NULL, tv_ptr);
#else
    ready = select(endpoint->fd + 1, &read_set, NULL, NULL, tv_ptr);
#endif
    if (ready == 0) {
        return 1; /* timeout */
    }
    if (ready < 0) {
        fprintf(stderr, "select() failed\n");
        return -1;
    }

    memset(&from_addr, 0, sizeof(from_addr));
    received = recvfrom(endpoint->fd,
                        (char *)data,
                        (int)capacity,
                        0,
                        (struct sockaddr *)&from_addr,
                        &from_len);
    if (received == SOCKET_ERROR) {
#ifdef _WIN32
        int error_code = WSAGetLastError();
        if (error_code == WSAECONNRESET) {
            /*
             * Windows UDP 的一个特殊点：
             * 如果我们给某个 UDP 端口发过包，而对方端口随后关闭，
             * Windows 可能在下一次 recvfrom() 返回 WSAECONNRESET。
             *
             * UDP 本身没有连接，这里并不是“TCP 连接断开”。
             * 对本 demo 来说，把它当成一次没有收到数据即可。
             */
            return 1;
        }
#endif
        fprintf(stderr, "recvfrom() failed\n");
        return -1;
    }

    if (received_size != NULL) {
        *received_size = (size_t)received;
    }
    if (source != NULL) {
        source->ip_be = from_addr.sin_addr.s_addr;
        source->port_be = from_addr.sin_port;
    }

    return 0;
}

void udp_endpoint_close(UdpEndpoint *endpoint)
{
    if (endpoint == NULL || endpoint->fd == INVALID_SOCKET) {
        return;
    }

#ifdef _WIN32
    closesocket(endpoint->fd);
#else
    close(endpoint->fd);
#endif
    endpoint->fd = INVALID_SOCKET;
}

uint64_t platform_time_ms(void)
{
#ifdef _WIN32
    /*
     * GetTickCount() 约 49.7 天回绕一次。demo 只用于短时间调试，
     * 这样写能兼容更多 MinGW/Windows SDK。
     */
    return (uint64_t)GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000u + (uint64_t)tv.tv_usec / 1000u;
#endif
}

void platform_sleep_ms(uint32_t ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep((useconds_t)ms * 1000u);
#endif
}

void platform_ntp_now(uint32_t *ntp_sec, uint32_t *ntp_frac)
{
    uint64_t unix_sec;
    uint64_t usec;

#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER value;
    uint64_t intervals_100ns;

    GetSystemTimeAsFileTime(&ft);
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;

    /*
     * FILETIME 从 1601-01-01 开始，单位 100ns。
     * Unix epoch 从 1970-01-01 开始。
     */
    intervals_100ns = value.QuadPart - 116444736000000000ULL;
    unix_sec = intervals_100ns / 10000000ULL;
    usec = (intervals_100ns % 10000000ULL) / 10ULL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unix_sec = (uint64_t)tv.tv_sec;
    usec = (uint64_t)tv.tv_usec;
#endif

    if (ntp_sec != NULL) {
        *ntp_sec = (uint32_t)(unix_sec + 2208988800UL);
    }
    if (ntp_frac != NULL) {
        *ntp_frac = (uint32_t)((usec << 32) / 1000000ULL);
    }
}
