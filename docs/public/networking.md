# Experimental Networking {#networking}

RADix-OS Crimson 0.1.0 includes an experimental IPv4/UDP networking path for
the x86 VM target. The goal of this surface is to prove the kernel interfaces
for network devices, packet receive, and POSIX-inspired datagram sockets before
adding TCP or target-specific production drivers.

## Current Stack

The network device API exposes link information, frame transmit, frame receive,
and polling. The x86 VM target registers a legacy virtio-net device as
`/dev/net0` when QEMU exposes one.

The socket layer currently supports:

- `RAD_AF_INET`
- `RAD_SOCK_DGRAM`
- `RAD_IPPROTO_UDP`
- `socket`, `bind`, `sendto`, `recvfrom`
- fd lifecycle through `close`, `dup`, `dup2`, `fcntl`, fork inheritance, and
  close-on-exec handling

The default VM IPv4 profile is static and QEMU-oriented:

- guest: `10.0.2.15`
- gateway/host side: `10.0.2.2`
- MTU: `1500`

## Current Limits

This page is intentionally marked experimental. The UDP socket path is verified
through host tests and x86 VM smoke markers, but the virtio-net backend is still
minimal. Full RX queue ownership, DHCP, DNS, ICMP, TCP, route tables, firewall
rules, packet sockets, and non-x86 NIC drivers remain future work.

TCP-style syscalls such as `connect`, `listen`, `accept`, and `shutdown`
currently return `RAD_STATUS_NOT_SUPPORTED`.

## Verification

Host tests cover network device registration, link info, transmit, receive, and
UDP datagram socket delivery.

The x86 Slint VM smoke gate checks:

- `RADIX_VIRTIO_NET_FOUND`
- `RADIX_NET_DEVICE_REGISTERED`
- `RADIX_ETH_TX_OK`
- `RADIX_ARP_OK`
- `RADIX_IPV4_OK`
- `RADIX_UDP_OK`
- `RADIX_UDP_RX_OK`
- `RADIX_SOCKET_DGRAM_OK`
