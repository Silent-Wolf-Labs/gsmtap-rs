/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <osmocom/core/application.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/socket.h>

static void require(int ok, const char *operation) {
    if (!ok) { fprintf(stderr, "network reference: %s: %s\n", operation, strerror(errno)); exit(1); }
}
static struct sockaddr_in endpoint(int fd, int peer) {
    struct sockaddr_in addr = {0}; socklen_t len = sizeof(addr);
    int rc = peer ? getpeername(fd, (struct sockaddr *)&addr, &len) : getsockname(fd, (struct sockaddr *)&addr, &len);
    require(rc == 0 && len == sizeof(addr) && addr.sin_family == AF_INET, "endpoint");
    return addr;
}
static int option(int fd, int level, int name) {
    int value = 0; socklen_t len = sizeof(value);
    require(getsockopt(fd, level, name, &value, &len) == 0, "getsockopt"); return value;
}
static void report(const char *name, int fd, int connected, int payload) {
    struct sockaddr_in local = endpoint(fd, 0);
    int flags = fcntl(fd, F_GETFL);
    require(flags >= 0, "fcntl");
    printf("%s\t%d\t%d\t%d\t%d\t%d\t%d\n", name,
        local.sin_addr.s_addr == htonl(INADDR_LOOPBACK) && local.sin_port != 0,
        connected, !!(flags & O_NONBLOCK), option(fd, SOL_SOCKET, SO_REUSEADDR),
        option(fd, IPPROTO_IP, IP_TOS) >> 2, payload);
}
static int create(int stream, unsigned flags, unsigned port) {
    int fd = osmo_sock_init2(AF_INET, stream ? SOCK_STREAM : SOCK_DGRAM,
        stream ? IPPROTO_TCP : IPPROTO_UDP, "127.0.0.1", 0,
        (flags & OSMO_SOCK_F_CONNECT) ? "127.0.0.1" : NULL, port, flags);
    require(fd >= 0, "osmo_sock_init2"); return fd;
}
static void timeout(int fd) {
    struct timeval timeout = {2, 0};
    require(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0, "timeout");
    require(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0, "timeout");
}
int main(void) {
    const struct log_info logging = {0};
    require(osmo_init_logging2(NULL, &logging) == 0, "logging");
    log_set_log_level(osmo_stderr_target, LOGL_FATAL);
    puts("# network-establishment-v1: case bound connected nonblocking reuse_address dscp payload_match");
    int udp = create(0, OSMO_SOCK_F_BIND, 0);
    report("udp_bound", udp, 0, 0);
    close(udp);
    int configured = create(0, OSMO_SOCK_F_BIND | OSMO_SOCK_F_NONBLOCK | OSMO_SOCK_F_UDP_REUSEADDR | OSMO_SOCK_F_DSCP(46), 0);
    report("udp_configured", configured, 0, 0);
    require(osmo_sock_set_dscp(configured, 64) == -EINVAL, "invalid DSCP");
    puts("invalid_dscp\t1");
    close(configured);
    int receiver = create(0, OSMO_SOCK_F_BIND, 0);
    struct sockaddr_in remote = endpoint(receiver, 0);
    int sender = create(0, OSMO_SOCK_F_BIND | OSMO_SOCK_F_CONNECT, ntohs(remote.sin_port));
    struct sockaddr_in peer = endpoint(sender, 1);
    timeout(sender); timeout(receiver);
    require(send(sender, "udp", 3, 0) == 3, "send UDP");
    char bytes[3] = {0};
    require(recv(receiver, bytes, sizeof(bytes), 0) == 3, "receive UDP");
    report("udp_connected", sender, peer.sin_port == remote.sin_port && peer.sin_addr.s_addr == remote.sin_addr.s_addr, !memcmp(bytes, "udp", 3));
    close(sender); close(receiver);
    int listener = create(1, OSMO_SOCK_F_BIND, 0);
    report("tcp_listener", listener, 0, 0);
    remote = endpoint(listener, 0);
    int stream = create(1, OSMO_SOCK_F_BIND | OSMO_SOCK_F_CONNECT, ntohs(remote.sin_port));
    struct pollfd ready = { .fd = listener, .events = POLLIN };
    require(poll(&ready, 1, 2000) == 1, "wait accept");
    int accepted = accept(listener, NULL, NULL);
    require(accepted >= 0, "accept"); timeout(accepted); timeout(stream);
    require(send(stream, "tcp", 3, 0) == 3, "send TCP");
    memset(bytes, 0, sizeof(bytes));
    require(recv(accepted, bytes, sizeof(bytes), MSG_WAITALL) == 3, "receive TCP");
    peer = endpoint(stream, 1);
    report("tcp_connected", stream, peer.sin_port == remote.sin_port && peer.sin_addr.s_addr == remote.sin_addr.s_addr, !memcmp(bytes, "tcp", 3));
    close(accepted); close(stream); close(listener);
    return 0;
}
