/**
 * @file
 * Ethernet Interface Skeleton
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */

#include "lwip/opt.h"
#include "netif/tapif.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ppp/pppoe.h"

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <unistd.h>
#include <net/ethernet.h>
#include <linux/if_ether.h>

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 't'
#define IFNAME2 'h'
#define IFNAME3 '1'

#define DEFAULT_IF  "eth1"

#define ETH_BUF_SIZ   1518



/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
struct ethernetif 
{

  struct eth_addr ethaddr;
  /* Add whatever per-interface state that is needed here. */

  /** Interface name e.g eth0, eth1 .... etc **/
  char ifName[IFNAMSIZ];


  int send_sockfd;  /* Sock fd used for sending a packet to raw socket */ 
  struct ifreq if_idx;
  struct ifreq if_mac;
  struct sockaddr_ll socket_address;

  int recv_sockfd;  /* Sock fd used for reading a packet from raw socket */
  struct ifreq ifopts;  /* set promiscuous mode */
  int sockopt;

};

/* Forward declarations. */
static void  ethernetif_input(struct netif *netif);

#if !NO_SYS
static void  ethernetif_thread(void *arg);
#endif

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void
low_level_ethernetif_init(struct netif *netif)
{
  struct ethernetif *ethernetif = (struct ethernetif *)netif->state;

  LWIP_ASSERT("ethernetif != NULL", (ethernetif != NULL));


  /***  Open and Initialize RAW Ethernet Socket ***/
  strcpy(ethernetif->ifName, DEFAULT_IF);

  /* Open RAW socket to send on */
  if ((ethernetif->send_sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) 
  {
    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: UNABLE TO CREATE A RAW SOCKET!!! \n"));    
    perror("create socket error! ");
    exit(1);
  }

  /* Get the index of the interface to send on */
  memset(&ethernetif->if_idx, 0, sizeof(struct ifreq));
  strncpy(ethernetif->if_idx.ifr_name, ethernetif->ifName, IFNAMSIZ-1);
  if (ioctl(ethernetif->send_sockfd, SIOCGIFINDEX, &ethernetif->if_idx) < 0)
  {
      LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: SIOCGIFINDEX \n"));        
      perror("SIOCGIFINDEX");
      exit(1);
  }

  /* Index of the network device */
  ethernetif->socket_address.sll_ifindex = ethernetif->if_idx.ifr_ifindex;

  /* Get the MAC address of the interface to send on */
  memset(&ethernetif->if_mac, 0, sizeof(struct ifreq));
  strncpy(ethernetif->if_mac.ifr_name, ethernetif->ifName, IFNAMSIZ-1);
  if (ioctl(ethernetif->send_sockfd, SIOCGIFHWADDR, &ethernetif->if_mac) < 0)
  {
      LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: SIOCGIFHWADDR \n"));    
      perror("SIOCGIFHWADDR");  
      exit(1);
  }

  /* Ethernet header */
  ethernetif->ethaddr.addr[0] = ((uint8_t *)&ethernetif->if_mac.ifr_hwaddr.sa_data)[0];
  ethernetif->ethaddr.addr[1] = ((uint8_t *)&ethernetif->if_mac.ifr_hwaddr.sa_data)[1];
  ethernetif->ethaddr.addr[2] = ((uint8_t *)&ethernetif->if_mac.ifr_hwaddr.sa_data)[2];
  ethernetif->ethaddr.addr[3] = ((uint8_t *)&ethernetif->if_mac.ifr_hwaddr.sa_data)[3];
  ethernetif->ethaddr.addr[4] = ((uint8_t *)&ethernetif->if_mac.ifr_hwaddr.sa_data)[4];
  ethernetif->ethaddr.addr[5] = ((uint8_t *)&ethernetif->if_mac.ifr_hwaddr.sa_data)[5];

  /* set MAC hardware address length */
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  /* set MAC hardware address */
  netif->hwaddr[0] = ethernetif->ethaddr.addr[0];
  netif->hwaddr[1] = ethernetif->ethaddr.addr[1];  
  netif->hwaddr[2] = ethernetif->ethaddr.addr[2];
  netif->hwaddr[3] = ethernetif->ethaddr.addr[3];
  netif->hwaddr[4] = ethernetif->ethaddr.addr[4];
  netif->hwaddr[5] = ethernetif->ethaddr.addr[5];

  /* maximum transfer unit */
  netif->mtu = 1500;

  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_IPV6 && LWIP_IPV6_MLD
  /*
   * For hardware/netifs that implement MAC filtering.
   * All-nodes link-local is handled by default, so we must let the hardware know
   * to allow multicast packets in.
   * Should set mld_mac_filter previously. */
  if (netif->mld_mac_filter != NULL) 
  {
    ip6_addr_t ip6_allnodes_ll;
    ip6_addr_set_allnodes_linklocal(&ip6_allnodes_ll);
    netif->mld_mac_filter(netif, &ip6_allnodes_ll, NETIF_ADD_MAC_FILTER);
  }
#endif /* LWIP_IPV6 && LWIP_IPV6_MLD */

  /* Do whatever else is needed to initialize interface. */

  /* Create a thread to receive ethernet packets */
#if !NO_SYS
  sys_thread_new("ethernetif_thread", ethernetif_thread, netif, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
#else

  /** Create receiving socket **/
  /* Open PF_PACKET socket, listening for EtherType ETHER_TYPE */
  if ((ethernetif->recv_sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) 
  {
    perror("listener: socket"); 
    exit(EXIT_FAILURE);
  }

  /* Set interface to promiscuous mode - do we need to do this every time? */
  strncpy(ethernetif->ifopts.ifr_name, ethernetif->ifName, IFNAMSIZ-1);
  ioctl(ethernetif->recv_sockfd, SIOCGIFFLAGS, &ethernetif->ifopts);

  ethernetif->ifopts.ifr_flags |= IFF_PROMISC;
  ioctl(ethernetif->recv_sockfd, SIOCSIFFLAGS, &ethernetif->ifopts);

  /* Allow the socket to be reused - incase connection is closed prematurely */
  if (setsockopt(ethernetif->recv_sockfd, SOL_SOCKET, SO_REUSEADDR, &ethernetif->sockopt, sizeof (int)) == -1) 
  {
    perror("setsockopt");
    close(ethernetif->recv_sockfd);
    exit(EXIT_FAILURE);
  }

  /* Bind to device */
  if (setsockopt(ethernetif->recv_sockfd, SOL_SOCKET, SO_BINDTODEVICE, ethernetif->ifName, IFNAMSIZ-1) == -1)  
  {
    perror("SO_BINDTODEVICE");
    close(ethernetif->recv_sockfd);
    exit(EXIT_FAILURE);
  }

  printf("listener: Waiting to recvfrom...\n");

#endif /* !NO_SYS */

}


/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t
low_level_ethernetif_output(struct netif *netif, struct pbuf *p)
{
  struct ethernetif *ethernetif = (struct ethernetif *)netif->state;
  char buf[ETH_BUF_SIZ]; /* max packet size including VLAN excluding CRC */
  ssize_t written;

  LWIP_ASSERT("netif != NULL", (netif != NULL));
  LWIP_ASSERT("ethernetif != NULL", (ethernetif != NULL));

#if ETH_PAD_SIZE
  pbuf_remove_header(p, ETH_PAD_SIZE); /* drop the padding word */
#endif

  if (p->tot_len > sizeof(buf)) 
  {
    MIB2_STATS_NETIF_INC(netif, ifoutdiscards);
    perror("ethernetif: packet too large");
    return ERR_IF;
  }

  /* initiate transfer(); */
  pbuf_copy_partial(p, buf, p->tot_len, 0);

  /* signal that packet should be sent(); */
  /* Destination MAC */
  ethernetif->socket_address.sll_addr[0] = buf[0]; 
  ethernetif->socket_address.sll_addr[1] = buf[1]; 
  ethernetif->socket_address.sll_addr[2] = buf[2]; 
  ethernetif->socket_address.sll_addr[3] = buf[3]; 
  ethernetif->socket_address.sll_addr[4] = buf[4]; 
  ethernetif->socket_address.sll_addr[5] = buf[5]; 

  written = sendto(ethernetif->send_sockfd, buf, p->tot_len, 0, (struct sockaddr*)&ethernetif->socket_address, sizeof(struct sockaddr_ll));
  if (written < p->tot_len) 
  {
    MIB2_STATS_NETIF_INC(netif, ifoutdiscards);
    perror("ethernetif: sendto");
    return ERR_IF;
  } 
  else 
  {
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, (u32_t)written);

    if (((u8_t *)p->payload)[0] & 1) 
    {
      /* broadcast or multicast packet*/
      MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);
    } 
    else 
    {
      /* unicast packet */
      MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);
    }

    /* increase ifoutdiscards or ifouterrors on error */

#if ETH_PAD_SIZE
  pbuf_add_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

  LINK_STATS_INC(link.xmit);

    return ERR_OK;
  }


  return ERR_OK;
}


/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *
low_level_ethernetif_input(struct netif *netif)
{
  struct ethernetif *ethernetif = (struct ethernetif *)netif->state;
  struct pbuf *p;
  u16_t len;
  u8_t buf[ETH_BUF_SIZ];

  LWIP_ASSERT("netif != NULL", (netif != NULL));
  LWIP_ASSERT("ethernetif != NULL", (ethernetif != NULL));

  /* Obtain the size of the packet and put it into the "len" variable. */
  len = recvfrom(ethernetif->recv_sockfd, buf, ETH_BUF_SIZ, 0, NULL, NULL);

#if ETH_PAD_SIZE
  len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

  /* We allocate a pbuf chain of pbufs from the pool. */
  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

  if (p != NULL) {

#if ETH_PAD_SIZE
    pbuf_remove_header(p, ETH_PAD_SIZE); /* drop the padding word */
#endif

    pbuf_take(p, buf, len);
    
    MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->tot_len);

    if (((u8_t *)p->payload)[0] & 1) 
    {
      /* broadcast or multicast packet*/
      MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
    } 
    else 
    {
      /* unicast packet*/
      MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
    }

#if ETH_PAD_SIZE
    pbuf_add_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

    LINK_STATS_INC(link.recv);
  } 
  else 
  {
    
    MIB2_STATS_NETIF_INC(netif, ifindiscards);
    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: could not allocate pbuf\n"));

  }


  return p;
}


/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
static void
ethernetif_input(struct netif *netif)
{
  struct ethernetif *ethernetif;
  /* struct eth_hdr *ethhdr; */
  struct pbuf *p;

  LWIP_ASSERT("netif != NULL", (netif != NULL));

  ethernetif = (struct ethernetif *)netif->state;
  LWIP_ASSERT("ethernetif != NULL", (ethernetif != NULL));

  /* move received packet into a new pbuf */
  p = low_level_ethernetif_input(netif);

  /* if no packet could be read, silently ignore this */
  if (p != NULL) 
  {  
    /* pass all packets to ethernet_input, which decides what packets it supports */
    if (netif->input(p, netif) != ERR_OK) 
    {
      /* LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error \n")); */
      pbuf_free(p);
      p = NULL;
    }
  }

}



/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
ethernetif_init(struct netif *netif)
{
  struct ethernetif *ethernetif;

  LWIP_ASSERT("netif != NULL", (netif != NULL));

  ethernetif = (struct ethernetif *)mem_malloc(sizeof(struct ethernetif));
  if (ethernetif == NULL) 
  {
    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
    return ERR_MEM;
  }

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  /* MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS); */

  netif->state = ethernetif;

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  netif->name[2] = IFNAME2;
  netif->name[3] = IFNAME3;  
  
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */

#if LWIP_IPV4
  netif->output = etharp_output;
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */

  netif->linkoutput = low_level_ethernetif_output;

  ethernetif->ethaddr.addr[0] = (uint8_t)netif->hwaddr[0];
  ethernetif->ethaddr.addr[1] = (uint8_t)netif->hwaddr[1];
  ethernetif->ethaddr.addr[2] = (uint8_t)netif->hwaddr[2];
  ethernetif->ethaddr.addr[3] = (uint8_t)netif->hwaddr[3];
  ethernetif->ethaddr.addr[4] = (uint8_t)netif->hwaddr[4];
  ethernetif->ethaddr.addr[5] = (uint8_t)netif->hwaddr[5];


  /* initialize the hardware */
  low_level_ethernetif_init(netif);

  return ERR_OK;
}


/*-----------------------------------------------------------------------------------*/
void
ethernetif_poll(struct netif *netif)
{
  ethernetif_input(netif);
}

#if NO_SYS

#if 0
int
ethernetif_select(struct netif *netif)
{
  /** NOT SURE WHAT TO DO HERE! **/
  return -1;
}
#endif

#else /* NO_SYS */

static void
ethernetif_thread(void *arg)
{

  struct netif *netif;
  struct ethernetif *ethernetif;

  netif = (struct netif *)arg;
  LWIP_ASSERT("netif != NULL", (netif != NULL));

  ethernetif = (struct ethernetif *)netif->state;
  LWIP_ASSERT("ethernetif != NULL", (ethernetif != NULL));


  /** Create receiving socket **/
  /* Open PF_PACKET socket, listening for EtherType ETHER_TYPE */
  if ((ethernetif->recv_sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) 
  {
    perror("listener: socket"); 
    exit(EXIT_FAILURE);
  }

  /* Set interface to promiscuous mode - do we need to do this every time? */
  strncpy(ethernetif->ifopts.ifr_name, ethernetif->ifName, IFNAMSIZ-1);
  ioctl(ethernetif->recv_sockfd, SIOCGIFFLAGS, &ethernetif->ifopts);

  ethernetif->ifopts.ifr_flags |= IFF_PROMISC;
  ioctl(ethernetif->recv_sockfd, SIOCSIFFLAGS, &ethernetif->ifopts);

  /* Allow the socket to be reused - incase connection is closed prematurely */
  if (setsockopt(ethernetif->recv_sockfd, SOL_SOCKET, SO_REUSEADDR, &ethernetif->sockopt, sizeof (int)) == -1) 
  {
    perror("setsockopt");
    close(ethernetif->recv_sockfd);
    exit(EXIT_FAILURE);
  }

  /* Bind to device */
  if (setsockopt(ethernetif->recv_sockfd, SOL_SOCKET, SO_BINDTODEVICE, ethernetif->ifName, IFNAMSIZ-1) == -1)  
  {
    perror("SO_BINDTODEVICE");
    close(ethernetif->recv_sockfd);
    exit(EXIT_FAILURE);
  }

  printf("listener: Waiting to recvfrom...\n");

  
  while(1) 
  {
    /** Read packets from RAW socket **/
    ethernetif_input(netif);
  
  }

}

#endif
