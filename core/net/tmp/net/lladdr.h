/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Header file for the link-layer address representation
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Simon Duquennoy <simonduq@sics.se>
 */

#ifndef __LLADDR_H__
#define __LLADDR_H__

#include "contiki-conf.h"

/** \brief 16 bit 802.15.4 address */
typedef union lladdr_802154_shortaddr {
  uint8_t u8[2];
  uint8_t addr[2];
} lladdr_802154_shortaddr;
/** \brief 64 bit 802.15.4 address */
typedef union lladdr_802154_longaddr {
  uint8_t u8[8];
  uint8_t addr[8];
} lladdr_802154_longaddr;

/** \brief 802.11 address */
typedef union lladdr_80211_addr {
  uint8_t u8[6];
  uint8_t addr[6];
} lladdr_80211_addr;

/** \brief 802.3 address */
typedef union lladdr_eth_addr {
  uint8_t u8[6];
  uint8_t addr[6];
} lladdr_eth_addr;

#if LLADDR_CONF_802154
/** \brief 802.15.4 address */
typedef lladdr_802154_longaddr lladdr_t;
#define LL_802154_SHORTADDR_LEN 2
#define LL_802154_LONGADDR_LEN  8
#define LLADDR_LEN LL_802154_LONGADDR_LEN
#else /*LLADDR_CONF_802154*/
#if LLADDR_CONF_80211
/** \brief 802.11 address */
typedef ll_80211_addr lladdr_t;
#define LLADDR_LEN 6
#else /*LLADDR_CONF_80211*/
/** \brief Ethernet address */
typedef lladdr_eth_addr lladdr_t;
#define LLADDR_LEN 6
#endif /*LLADDR_CONF_80211*/
#endif /*LLADDR_CONF_802154*/

/**
 * \brief      Copy a link-layer address
 * \param dest The destination
 * \param from The source
 *
 *             This function copies a link-layer address from one location
 *             to another.
 *
 */
void lladdr_copy(lladdr_t *dest, const lladdr_t *from);

/**
 * \brief      Compare two link-layer addresses
 * \param addr1 The first address
 * \param addr2 The second address
 * \return     Non-zero if the addresses are the same, zero if they are different
 *
 *             This function compares two Rime addresses and returns
 *             the result of the comparison. The function acts like
 *             the '==' operator and returns non-zero if the addresses
 *             are the same, and zero if the addresses are different.
 *
 */
int lladdr_cmp(const lladdr_t *addr1, const lladdr_t *addr2);


/**
 * \brief      Set the address of the current node
 * \param addr The address
 *
 *             This function sets the Rime address of the node.
 *
 */
void lladdr_set_node_addr(lladdr_t *addr);

/**
 * \brief      The link-layer address of the node
 *
 *             This variable contains the Rime address of the
 *             node. This variable should not be changed directly;
 *             rather, the lladdr_set_node_addr() function should be
 *             used.
 *
 */
extern lladdr_t node_lladdr;

/**
 * \brief      The null link-layer address
 *
 *             This variable contains the null Rime address. The null
 *             address is used in route tables to indicate that the
 *             table entry is unused. Nodes with no configured address
 *             has the null address. Nodes with their node address set
 *             to the null address will have problems communicating
 *             with other nodes.
 *
 */
extern const lladdr_t lladdr_null;

#endif /* __LLADDR_H__ */
/** @} */
/** @} */
