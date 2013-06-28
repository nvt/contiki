/*
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

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/uip.h"
#include "net/rpl/rpl.h"

#include "net/netstack.h"
#include "dev/button-sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>




/////////////////////////////////////////////////////////////////////////////////////////////////////

///** \name Neighbor tables: register and loop through table elements */
///** @{ */
//int nbr_table_register(nbr_table_t *table);
//item_t *nbr_table_head(nbr_table_t *table);
//item_t *nbr_table_next(nbr_table_t *table, item_t *item);
///** @} */
//
///** \name Neighbor tables: add and get data */
///** @{ */
//item_t *nbr_table_add_lladdr(nbr_table_t *table, const rimeaddr_t *lladdr);
//item_t *nbr_table_get_from_lladdr(nbr_table_t *table, const rimeaddr_t *lladdr);
///** @} */
//
///** \name Neighbor tables: set flags (unused, locked, unlocked) */
///** @{ */
//int nbr_table_remove(nbr_table_t *table, item_t *item);
//int nbr_table_lock(nbr_table_t *table, item_t *item);
//int nbr_table_unlock(nbr_table_t *table, item_t *item);
///** @} */
//
///** \name Neighbor tables: address manipulation */
///** @{ */
//rimeaddr_t *nbr_table_get_lladdr(nbr_table_t *table, item_t *item);

#include "net/neighbor-table.h"

typedef struct {
  int count;
} nbr_unit_t;

void callback1(nbr_unit_t *item);
void callback2(nbr_unit_t *item);

NEIGHBOR_TABLE(nbr_unit_t, nbr_test, callback1);
NEIGHBOR_TABLE(nbr_unit_t, nbr_test2, callback2);

const rimeaddr_t rimeaddr1 = { { 0, 0, 0, 0, 0, 0, 0, 1 } };
const rimeaddr_t rimeaddr2 = { { 0, 0, 0, 0, 0, 0, 0, 2 } };
const rimeaddr_t rimeaddr3 = { { 0, 0, 0, 0, 0, 0, 0, 3 } };
const rimeaddr_t rimeaddr4 = { { 0, 0, 0, 0, 0, 0, 0, 4 } };
const rimeaddr_t rimeaddr5 = { { 0, 0, 0, 0, 0, 0, 0, 5 } };
const rimeaddr_t rimeaddr6 = { { 0, 0, 0, 0, 0, 0, 0, 6 } };
const rimeaddr_t rimeaddr7 = { { 0, 0, 0, 0, 0, 0, 0, 7 } };
const rimeaddr_t rimeaddr8 = { { 0, 0, 0, 0, 0, 0, 0, 8 } };
const rimeaddr_t rimeaddr9 = { { 0, 0, 0, 0, 0, 0, 0, 9 } };
const rimeaddr_t rimeaddr10 = { { 0, 0, 0, 0, 0, 0, 0, 10 } };
const rimeaddr_t rimeaddr11 = { { 0, 0, 0, 0, 0, 0, 0, 11 } };
const rimeaddr_t rimeaddr12 = { { 0, 0, 0, 0, 0, 0, 0, 12 } };

void
callback1(nbr_unit_t *item) {
  printf("Callback [1]: %u\n", item->count);
  nbr_table_remove(nbr_test, item);
}

void
callback2(nbr_unit_t *item) {
  printf("Callback [2]: %u\n", item->count);
}

void
neighbor_table_list() {
  nbr_unit_t *item = nbr_table_head(nbr_test);
  printf("Neighbor table list:\n");
  while(item) {
    printf("%u\n", item->count);
    item = nbr_table_next(nbr_test, item);
  }
  printf(":end\n");
}

void
unit_test_neighbor_table() {
  int count = 1;
  nbr_unit_t *item;
  printf("Unit test: neighbor table\n");
  printf("Table size: %u\n", NEIGHBOR_TABLE_MAX_NEIGHBORS);
  printf("Start\n");
  nbr_table_register(nbr_test);
  nbr_table_register(nbr_test2);
  neighbor_table_list();
  printf("Add 6 (2: double, 1: locked, 3: locked)\n");
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr1);
  nbr_table_lock(nbr_test, item);
  item->count = count++;
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr2);
  item->count = count++;
  item = nbr_table_add_lladdr(nbr_test2, &rimeaddr2);
  item->count = 222;
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr3);
  nbr_table_lock(nbr_test, item);
  item->count = count++;
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr4);
  item->count = count++;
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr5);
  item->count = count++;
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr6);
  item->count = count++;
  neighbor_table_list();
  printf("Lock 6, Add 7, 8 (7: locked)\n");
  nbr_table_lock(nbr_test, nbr_table_get_from_lladdr(nbr_test, &rimeaddr6));
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr7);
  nbr_table_lock(nbr_test, item);
  item->count = count++;
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr8);
  item->count = count++;
  neighbor_table_list();
  printf("Dec 2\n");
  nbr_table_remove(nbr_test, nbr_table_get_from_lladdr(nbr_test, &rimeaddr2));
  neighbor_table_list();
  printf("Add 9\n");
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr9);
  item->count = count++;
  neighbor_table_list();
  printf("Remove 3, unlock 1\n");
  nbr_table_remove(nbr_test, nbr_table_get_from_lladdr(nbr_test, &rimeaddr3));
  nbr_table_unlock(nbr_test, nbr_table_get_from_lladdr(nbr_test, &rimeaddr1));
  neighbor_table_list();
  printf("Add 10, 11\n");
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr10);
  item->count = count++;
  item = nbr_table_add_lladdr(nbr_test, &rimeaddr11);
  item->count = count++;
  neighbor_table_list();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////








#define SERVER_REPLY 1

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define UDP_EXAMPLE_ID  190

static struct uip_udp_conn *server_conn;

PROCESS(udp_server_process, "UDP server process");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  if(uip_newdata()) {
    uint8_t seq_id = *((uint8_t*)uip_appdata);
    PRINTF("DATA recv %u from %u\n",
        seq_id, UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1]);
#if SERVER_REPLY
    PRINTF("DATA send %u to %u\n",
        seq_id, UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1]);
    uip_ipaddr_copy(&server_conn->ripaddr, &UIP_IP_BUF->srcipaddr);
    uip_udp_packet_send(server_conn, &seq_id, sizeof(seq_id));
    uip_create_unspecified(&server_conn->ripaddr);
#endif
  }
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Server IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
	uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  //unit_test_neighbor_table();
  //return 0;

  uip_ipaddr_t ipaddr;
  struct uip_ds6_addr *root_if;

  PROCESS_BEGIN();

  PROCESS_PAUSE();

  SENSORS_ACTIVATE(button_sensor);

  PRINTF("UDP server started\n");

#if UIP_CONF_ROUTER
/* The choice of server address determines its 6LoPAN header compression.
 * Obviously the choice made here must also be selected in udp-client.c.
 *
 * For correct Wireshark decoding using a sniffer, add the /64 prefix to the 6LowPAN protocol preferences,
 * e.g. set Context 0 to aaaa::.  At present Wireshark copies Context/128 and then overwrites it.
 * (Setting Context 0 to aaaa::1111:2222:3333:4444 will report a 16 bit compressed address of aaaa::1111:22ff:fe33:xxxx)
 * Note Wireshark's IPCMV6 checksum verification depends on the correct uncompressed addresses.
 */
 
#if 0
/* Mode 1 - 64 bits inline */
   uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
#elif 1
/* Mode 2 - 16 bits inline */
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
#else
/* Mode 3 - derived from link local (MAC) address */
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
#endif

  uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
  root_if = uip_ds6_addr_lookup(&ipaddr);
  if(root_if != NULL) {
    rpl_dag_t *dag;
    dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &ipaddr, 64);
    PRINTF("created a new RPL dag\n");
  } else {
    PRINTF("failed to create a new RPL DAG\n");
  }
#endif /* UIP_CONF_ROUTER */
  
  print_local_addresses();

  /* The data sink runs with a 100% duty cycle in order to ensure high 
     packet reception rates. */
  NETSTACK_MAC.off(1);

  server_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);
  if(server_conn == NULL) {
    PRINTF("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }
  udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT));

  PRINTF("Created a server connection with remote address ");
  PRINT6ADDR(&server_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n", UIP_HTONS(server_conn->lport),
         UIP_HTONS(server_conn->rport));

  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
      tcpip_handler();
    } else if (ev == sensors_event && data == &button_sensor) {
      PRINTF("Initiaing global repair\n");
      rpl_repair_root(RPL_DEFAULT_INSTANCE);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
