#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// lwipopts.h — lwIP configuration for ABC80 Pico / CYW43 threadsafe_background
//
// Rules imposed by the SDK:
//   - MEM_LIBC_MALLOC must be 0 (malloc is not safe in IRQ context)
//   - LWIP_NETIF_TX_SINGLE_PBUF must be 1 (CYW43 cannot scatter-gather TX)
//   - Checksums are computed by lwIP in software (CYW43 has no offload path)

// ── No OS — lwIP driven by CYW43 threadsafe_background interrupts ────────────
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// ── Memory — fixed pools (MEM_LIBC_MALLOC forbidden with threadsafe_bg) ───────
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    8192  // main lwIP heap (TCP PCBs, etc.)
#define MEMP_NUM_TCP_PCB            4
#define MEMP_NUM_TCP_PCB_LISTEN     0
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_SYS_TIMEOUT        16    // DHCP + TCP + CYW43 internal timers
#define PBUF_POOL_SIZE              16

// ── TCP ──────────────────────────────────────────────────────────────────────
#define LWIP_TCP                    1
#define TCP_MSS                     1460
#define TCP_WND                     (4 * TCP_MSS)
#define TCP_SND_BUF                 (4 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define TCP_TTL                     255

// ── Protocols required by CYW43 driver ───────────────────────────────────────
#define LWIP_UDP                    1
#define LWIP_DHCP                   1
#define LWIP_DNS                    1   // needed by CYW43 internally
#define LWIP_ICMP                   1
#define LWIP_IGMP                   1
#define LWIP_RAW                    1

// ── Netif callbacks — required by CYW43 ──────────────────────────────────────
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1

// ── CRITICAL: CYW43 cannot handle scatter-gather transmit ────────────────────
#define LWIP_NETIF_TX_SINGLE_PBUF   1

// ── Speed up DHCP (no ARP collision check on isolated AP) ────────────────────
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

// ── Stats / debug ─────────────────────────────────────────────────────────────
#define LWIP_STATS                  0
#define LWIP_DEBUG                  0

#endif // _LWIPOPTS_H
