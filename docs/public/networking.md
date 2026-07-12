# Experimental Networking {#networking}

RADix-OS Crimson 0.1.0 includes an experimental networking path for the x86 VM
target. The goal of this surface is to prove the kernel interfaces for network
devices, packet receive, POSIX-inspired datagram sockets, and the first TCP
stream socket lifecycle before adding a complete wire-level TCP/IP stack or
target-specific production drivers.

## Current Stack

The network device API exposes link information, frame transmit, frame receive,
and polling. The x86 VM target registers a legacy virtio-net device as
`/dev/net0` when QEMU exposes one. The VM backend now programs separate legacy
virtio RX and TX rings and emits smoke markers when the rings are owned by the
guest.

The socket layer currently supports:

- `RAD_AF_INET`
- `RAD_SOCK_DGRAM`
- `RAD_IPPROTO_UDP`
- `socket`, `bind`, `sendto`, `recvfrom`
- `RAD_SOCK_STREAM`
- `RAD_IPPROTO_TCP`
- `listen`, `accept`, `connect`, `send`, `recv`, `shutdown`
- fd lifecycle through `close`, `dup`, `dup2`, `fcntl`, fork inheritance, and
  close-on-exec handling

The TCP support in this pass is a kernel socket model and local stream
foundation. It verifies listener setup, client connect, accept, connected peer
state, stream byte delivery, and shutdown behavior. It is not yet a full
wire-level TCP implementation with SYN/ACK sequencing, retransmit timers,
congestion control, route lookup, or external host connections.

The default VM IPv4 profile is static and QEMU-oriented:

- guest: `10.0.2.15`
- gateway/host side: `10.0.2.2`
- MTU: `1500`

## Current Limits

This page is intentionally marked experimental. UDP datagrams, local TCP stream
sockets, and virtio-net queue setup are verified through host tests and x86 VM
smoke markers, but the network stack is still minimal. DHCP, DNS, ICMP, full
wire-level TCP, route tables, firewall rules, packet sockets, and non-x86 NIC
drivers remain future work.

## Verification

Host tests cover network device registration, link info, transmit, receive, UDP
datagram socket delivery, and the local TCP listener/connect/accept/stream
socket lifecycle.

The x86 Slint VM smoke gate checks:

- `RADIX_VIRTIO_NET_FOUND`
- `RADIX_NET_DEVICE_REGISTERED`
- `RADIX_VIRTIO_NET_TX_RING_OK`
- `RADIX_VIRTIO_NET_RX_RING_OK`
- `RADIX_ETH_TX_OK`
- `RADIX_ARP_OK`
- `RADIX_IPV4_OK`
- `RADIX_UDP_OK`
- `RADIX_UDP_RX_OK`
- `RADIX_SOCKET_DGRAM_OK`
- `RADIX_TCP_SOCKET_OK`
- `RADIX_TCP_CONNECT_OK`
- `RADIX_TCP_LISTEN_ACCEPT_OK`
- `RADIX_TCP_STREAM_IO_OK`
- `RADIX_TCP_SHUTDOWN_OK`
