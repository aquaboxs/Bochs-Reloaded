/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef SLIRP_H
#define SLIRP_H

#define SLIRP_VERSION_STRING "4.7.0"

#ifdef __CYGWIN__
#define __USE_W32_SOCKETS
#define _WIN32
#endif

#include "config.h"

#ifdef _WIN32

/* as defined in sdkddkver.h */
#ifdef _WIN32_WINNT
#if _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
#endif
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7 */
#endif
/* reduces the number of implicitly included headers */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <sys/timeb.h>
#include <iphlpapi.h>

#else
#define O_BINARY 0
#endif

#ifndef _WIN32
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#endif

#ifdef __APPLE__
#include <sys/filio.h>
#endif

#include "compat.h"

#include "debug.h"
#include "util.h"

#include "libslirp.h"
#include "ip.h"
#include "ip6.h"
#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "udp.h"
#include "ip_icmp.h"
#include "ip6_icmp.h"
#include "mbuf.h"
#include "sbuf.h"
#include "socket.h"
#include "if.h"
#include "main.h"
#include "misc.h"

#include "bootp.h"
#include "tftp.h"

#define ARPOP_REQUEST 1         /* ARP request */
#define ARPOP_REPLY   2         /* ARP reply   */

struct ethhdr {
    unsigned char  h_dest[ETH_ALEN];   /* destination eth addr */
    unsigned char  h_source[ETH_ALEN]; /* source ether addr    */
    unsigned short h_proto;            /* packet type ID field */
};

SLIRP_PACKED_BEGIN
struct slirp_arphdr {
    unsigned short ar_hrd;      /* format of hardware address */
    unsigned short ar_pro;      /* format of protocol address */
    unsigned char  ar_hln;      /* length of hardware address */
    unsigned char  ar_pln;      /* length of protocol address */
    unsigned short ar_op;       /* ARP opcode (command)       */

    /*
     *  Ethernet looks like this : This bit is variable sized however...
     */
    uint8_t  ar_sha[ETH_ALEN]; /* sender hardware address */
    uint32_t ar_sip;           /* sender IP address       */
    uint8_t  ar_tha[ETH_ALEN]; /* target hardware address */
    uint32_t ar_tip;           /* target IP address       */
} SLIRP_PACKED_END;

#define ARP_TABLE_SIZE 16

typedef struct ArpTable {
    struct slirp_arphdr table[ARP_TABLE_SIZE];
    int next_victim;
} ArpTable;

/* Add a new ARP entry for the given addresses */
void arp_table_add(Slirp *slirp, uint32_t ip_addr, 
                   const uint8_t ethaddr[ETH_ALEN]);

/* Look for an ARP entry for the given IP address */
bool arp_table_search(Slirp *slirp, uint32_t ip_addr,
                      uint8_t out_ethaddr[ETH_ALEN]);

/* Slirp configuration, specified by the application */
struct Slirp {
    int cfg_version;

    unsigned time_fasttimo;
    unsigned last_slowtimo;
    bool do_slowtimo;

    bool in_enabled, in6_enabled;

    /* virtual network configuration */
    struct in_addr vnetwork_addr;
    struct in_addr vnetwork_mask;
    struct in_addr vhost_addr;
    struct in6_addr vprefix_addr6;
    uint8_t vprefix_len;
    struct in6_addr vhost_addr6;
    bool disable_dhcp; /* slirp will not reply to any DHCP requests */
    struct in_addr vdhcp_startaddr;
    struct in_addr vnameserver_addr;
    struct in6_addr vnameserver_addr6;

    struct in_addr client_ipaddr;
    char client_hostname[33];

    int restricted;
    struct gfwd_list *guestfwd_list;

    int if_mtu;
    int if_mru;

    bool disable_host_loopback;

    /* mbuf states */
    struct mbuf m_freelist, m_usedlist;
    int mbuf_alloced;

    /* if states */
    struct mbuf if_fastq;   /* fast queue (for interactive data) */
    struct mbuf if_batchq;  /* queue for non-interactive data */
    struct mbuf *next_m;    /* pointer to next mbuf to output */
    bool if_start_busy;     /* avoid if_start recursion */

    /* ip states */
    struct ipq ipq;         /* ip reass. queue */
    uint16_t ip_id;         /* ip packet ctr, for ids */

    /* bootp/dhcp states */
    BOOTPClient bootp_clients[NB_BOOTP_CLIENTS];
    char *bootp_filename;
    size_t vdnssearch_len;
    uint8_t *vdnssearch;
    char *vdomainname;

    /* tcp states */
    struct socket tcb;
    struct socket *tcp_last_so;
    tcp_seq tcp_iss;        /* tcp initial send seq # */
    uint32_t tcp_now;       /* for RFC 1323 timestamps */

    /* udp states */
    struct socket udb;
    struct socket *udp_last_so;

    /* icmp states */
    struct socket icmp;
    struct socket *icmp_last_so;

    /* tftp states */
    char *tftp_prefix;
    struct tftp_session tftp_sessions[TFTP_SESSIONS_MAX];
    char *tftp_server_name;

    ArpTable arp_table;

    bool enable_emu;

    const SlirpCb *cb;
    void *opaque;

    bool disable_dns; /* slirp will not redirect/serve any DNS packet */
};

/*
 * Send one packet from each session.
 * If there are packets on the fastq, they are sent FIFO, before
 * everything else.  Then we choose the first packet from each
 * batchq session (socket) and send it.
 * For example, if there are 3 ftp sessions fighting for bandwidth,
 * one packet will be sent from the first session, then one packet
 * from the second session, then one packet from the third.
 */
void if_start(Slirp *);

/* Get the address of the DNS server on the host side */
int get_dns_addr(struct in_addr *pdns_addr);

#ifndef _WIN32
#include <netdb.h>
#endif

/* Whether we should send TCP keepalive packets */
extern bool slirp_do_keepalive;

#define TCP_MAXIDLE (TCPTV_KEEPCNT * TCPTV_KEEPINTVL)

/* dnssearch.c */
/* Translate from vdnssearch in configuration, into Slirp */
int translate_dnssearch(Slirp *s, const char ** names);

/* cksum.c */
/* Compute the checksum of the mbuf */
int cksum(struct mbuf *m, int len);
/* Compute the checksum of the mbuf which contains an IPv6 packet */
int ip6_cksum(struct mbuf *m);

/* if.c */
/* Called from slirp_new */
void if_init(Slirp *);
/* Queue packet into an output queue (fast or batch), for sending to the guest */
void if_output(struct socket *, struct mbuf *);

/* ip_input.c */
/* Called from slirp_new */
void ip_init(Slirp *);
/* Called from slirp_cleanup */
void ip_cleanup(Slirp *);
/* Process IPv4 packet coming from the guest */
void ip_input(struct mbuf *);
/*
 * IP timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void ip_slowtimo(Slirp *);
/*
 * Strip out IP options, at higher
 * level protocol in the kernel.
 * Second argument is buffer to which options
 * will be moved, and return value is their length.
 * (XXX) should be deleted; last arg currently ignored.
 */
void ip_stripoptions(struct mbuf *, struct mbuf *);

/* ip_output.c */
/* Send IPv4 packet to the guest */
int ip_output(struct socket *, struct mbuf *);

/* tcp_input.c */
/* Process TCP datagram coming from the guest */
void tcp_input(struct mbuf *, int, struct socket *);
/* Determine a reasonable value for maxseg size */
int tcp_mss(struct tcpcb *, unsigned offer);

/* tcp_output.c */
/* Send TCP datagram to the guest */
int tcp_output(struct tcpcb *);
/* Start/restart persistence timer */
void tcp_setpersist(struct tcpcb *);

/* tcp_subr.c */
/* Called from slirp_new */
void tcp_init(Slirp *);
/* Called from slirp_cleanup */
void tcp_cleanup(Slirp *);
/*
 * Create template to be used to send tcp packets on a connection.
 * Call after host entry created, fills
 * in a skeletal tcp/ip header, minimizing the amount of work
 * necessary when the connection is used.
 */
void tcp_template(struct tcpcb *);
/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.
 */
void tcp_respond(struct tcpcb *, struct tcpiphdr *, struct mbuf *, tcp_seq, tcp_seq, int);
/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.
 */
struct tcpcb * tcp_newtcpcb(struct socket *);
/*
 * Close a TCP control block:
 * discard all space held by the tcp
 * discard internet protocol block
 * wake up any sleepers
 */
struct tcpcb *tcp_close(struct tcpcb *);
/* The Internet socket got closed, tell the guest */
void tcp_sockclosed(struct tcpcb *);
/*
 * Connect to a host on the Internet
 * Called by tcp_input
 */
int tcp_fconnect(struct socket *);
/* Accept the connection from the Internet, and connect to the guest */
void tcp_connect(struct socket *);
/* Attach a TCPCB to a socket */
int tcp_attach(struct socket *);
/* * Return TOS according to the ports */
uint8_t tcp_tos(struct socket *);
/*
 * We received a packet from the guest.
 *
 * Emulate programs that try and connect to us
 * This includes ftp (the data connection is
 * initiated by the server) and IRC (DCC CHAT and
 * DCC SEND) for now
 */
int tcp_emu(struct socket *, struct mbuf *);
/* Configure the socket, now that the guest completed accepting the connection */
int tcp_ctl(struct socket *);
/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb *tcp_drop(struct tcpcb *tp, int err);

/* Find the socket for the guest address and port */
struct socket *slirp_find_ctl_socket(Slirp *slirp, struct in_addr guest_addr,
                                     int guest_port);

/* Send a frame to the virtual Ethernet board, i.e. call the application send_packet callback */
void slirp_send_packet_all(Slirp *slirp, const void *buf, size_t len);

/* Create a new timer, i.e. call the application timer_new callback */
void *slirp_timer_new(Slirp *slirp, SlirpTimerId id, void *cb_opaque);

#endif
