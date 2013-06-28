/**
 * \addtogroup uip6
 * @{
 */
/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
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
 *         Logic for Directed Acyclic Graphs in RPL.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 */


#include "contiki.h"
#include "net/ipv6/rpl/rpl-private.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-nd6.h"
#include "net/neighbor-table.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "sys/ctimer.h"

#include <limits.h>
#include <string.h>

#define DEBUG DEBUG_NONE
#include "net/ipv6/uip-debug.h"

/*---------------------------------------------------------------------------*/
extern rpl_of_t RPL_OF;
static rpl_of_t * const objective_functions[] = {&RPL_OF};

/*---------------------------------------------------------------------------*/
/* RPL definitions. */

#ifndef RPL_CONF_GROUNDED
#define RPL_GROUNDED                    0
#else
#define RPL_GROUNDED                    RPL_CONF_GROUNDED
#endif /* !RPL_CONF_GROUNDED */

/*---------------------------------------------------------------------------*/
/* Per-neighbor RPL information */
NEIGHBOR_TABLE(rpl_neighbor_t, rpl_neighbors, rpl_remove_neighbor);
/*---------------------------------------------------------------------------*/
/* Allocate instance table. */
rpl_instance_t instance_table[RPL_MAX_INSTANCES];
rpl_instance_t *default_instance;

/*---------------------------------------------------------------------------*/
uip_ipaddr_t *
rpl_get_neighbor_ipaddr(rpl_neighbor_t *nbr)
{
  lladdr_t *lladdr = nbr_table_get_lladdr(rpl_neighbors, nbr);
  return uip_ds6_nbr_ipaddr_from_lladdr(lladdr);
}
/*---------------------------------------------------------------------------*/
/* Greater-than function for the lollipop counter.                      */
/*---------------------------------------------------------------------------*/
static int
lollipop_greater_than(int a, int b)
{
  /* Check if we are comparing an initial value with an old value */
  if(a > RPL_LOLLIPOP_CIRCULAR_REGION && b <= RPL_LOLLIPOP_CIRCULAR_REGION) {
    return (RPL_LOLLIPOP_MAX_VALUE + 1 + b - a) > RPL_LOLLIPOP_SEQUENCE_WINDOWS;
  }
  /* Otherwise check if a > b and comparable => ok, or
     if they have wrapped and are still comparable */
  return (a > b && (a - b) < RPL_LOLLIPOP_SEQUENCE_WINDOWS) ||
      (a < b && (b - a) > (RPL_LOLLIPOP_CIRCULAR_REGION + 1-
          RPL_LOLLIPOP_SEQUENCE_WINDOWS));
}
/*---------------------------------------------------------------------------*/
/* Remove DAG neighbors with a rank that is at least the same as minimum_rank. */
static void
remove_neighbors(rpl_dag_t *dag, rpl_rank_t minimum_rank)
{
  rpl_neighbor_t *nbr;

  PRINTF("RPL: Removing neighbors (minimum rank %u)\n",
      minimum_rank);

  nbr = nbr_table_head(rpl_neighbors);
  while(nbr != NULL) {
    if(dag == nbr->dag && nbr->rank >= minimum_rank) {
      rpl_remove_neighbor(nbr);
    }
    nbr = nbr_table_next(rpl_neighbors, nbr);
  }

}
/*---------------------------------------------------------------------------*/
static void
nullify_neighbors(rpl_dag_t *dag, rpl_rank_t minimum_rank)
{
  rpl_neighbor_t *nbr;

  PRINTF("RPL: Removing neighbors (minimum rank %u)\n",
      minimum_rank);

  nbr = nbr_table_head(rpl_neighbors);
  while(nbr != NULL) {
    if(dag == nbr->dag && nbr->rank >= minimum_rank) {
      rpl_nullify_neighbor(nbr);
    }
    nbr = nbr_table_next(rpl_neighbors, nbr);
  }
}
/*---------------------------------------------------------------------------*/
static int
should_send_dao(rpl_instance_t *instance, rpl_dio_t *dio, rpl_neighbor_t *nbr)
{
  /* if MOP is set to no downward routes no DAO should be sent */
  if(instance->mop == RPL_MOP_NO_DOWNWARD_ROUTES) {
    return 0;
  }
  /* check if the new DTSN is more recent */
  return nbr == instance->current_dag->preferred_parent &&
      (lollipop_greater_than(dio->dtsn, nbr->dtsn));
}
/*---------------------------------------------------------------------------*/
static int
acceptable_rank(rpl_dag_t *dag, rpl_rank_t rank)
{
  return rank != INFINITE_RANK &&
      ((dag->instance->max_rankinc == 0) ||
          DAG_RANK(rank, dag->instance) <= DAG_RANK(dag->min_rank + dag->instance->max_rankinc, dag->instance));
}
/*---------------------------------------------------------------------------*/
static rpl_dag_t *
get_dag(uint8_t instance_id, uip_ipaddr_t *dag_id)
{
  rpl_instance_t *instance;
  rpl_dag_t *dag;
  int i;

  instance = rpl_get_instance(instance_id);
  if(instance == NULL) {
    return NULL;
  }

  for(i = 0; i < RPL_MAX_DAG_PER_INSTANCE; ++i) {
    dag = &instance->dag_table[i];
    if(dag->used && uip_ipaddr_cmp(&dag->dag_id, dag_id)) {
      return dag;
    }
  }

  return NULL;
}
/*---------------------------------------------------------------------------*/
rpl_dag_t *
rpl_set_root(uint8_t instance_id, uip_ipaddr_t *dag_id)
{
  rpl_dag_t *dag;
  rpl_instance_t *instance;
  uint8_t version;

  version = RPL_LOLLIPOP_INIT;
  dag = get_dag(instance_id, dag_id);
  if(dag != NULL) {
    version = dag->version;
    RPL_LOLLIPOP_INCREMENT(version);
    PRINTF("RPL: Dropping a joined DAG when setting this node as root");
    if(dag == dag->instance->current_dag) {
      dag->instance->current_dag = NULL;
    }
    rpl_free_dag(dag);
  }

  dag = rpl_alloc_dag(instance_id, dag_id);
  if(dag == NULL) {
    PRINTF("RPL: Failed to allocate a DAG\n");
    return NULL;
  }

  instance = dag->instance;

  dag->version = version;
  dag->joined = 1;
  dag->grounded = RPL_GROUNDED;
  instance->mop = RPL_MOP_DEFAULT;
  instance->of = &RPL_OF;
  dag->preferred_parent = NULL;

  memcpy(&dag->dag_id, dag_id, sizeof(dag->dag_id));

  instance->dio_intdoubl = RPL_DIO_INTERVAL_DOUBLINGS;
  instance->dio_intmin = RPL_DIO_INTERVAL_MIN;
  /* The current interval must differ from the minimum interval in order to
     trigger a DIO timer reset. */
  instance->dio_intcurrent = RPL_DIO_INTERVAL_MIN +
      RPL_DIO_INTERVAL_DOUBLINGS;
  instance->dio_redundancy = RPL_DIO_REDUNDANCY;
  instance->max_rankinc = RPL_MAX_RANKINC;
  instance->min_hoprankinc = RPL_MIN_HOPRANKINC;
  instance->default_lifetime = RPL_DEFAULT_LIFETIME;
  instance->lifetime_unit = RPL_DEFAULT_LIFETIME_UNIT;

  dag->rank = ROOT_RANK(instance);

  if(instance->current_dag != dag && instance->current_dag != NULL) {
    /* Remove routes installed by DAOs. */
    rpl_remove_routes(instance->current_dag);

    instance->current_dag->joined = 0;
  }

  instance->current_dag = dag;
  instance->dtsn_out = RPL_LOLLIPOP_INIT;
  instance->of->update_metric_container(instance);
  default_instance = instance;

  PRINTF("RPL: Node set to be a DAG root with DAG ID ");
  PRINT6ADDR(&dag->dag_id);
  PRINTF("\n");

  ANNOTATE("#A root=%u\n", dag->dag_id.u8[sizeof(dag->dag_id) - 1]);

  rpl_reset_dio_timer(instance);

  return dag;
}
/*---------------------------------------------------------------------------*/
int
rpl_repair_root(uint8_t instance_id)
{
  rpl_instance_t *instance;

  instance = rpl_get_instance(instance_id);
  if(instance == NULL ||
      instance->current_dag->rank != ROOT_RANK(instance)) {
    return 0;
  }

  RPL_LOLLIPOP_INCREMENT(instance->current_dag->version);
  RPL_LOLLIPOP_INCREMENT(instance->dtsn_out);
  rpl_reset_dio_timer(instance);
  return 1;
}
/*---------------------------------------------------------------------------*/
static void
set_ip_from_prefix(uip_ipaddr_t *ipaddr, rpl_prefix_t *prefix)
{
  memset(ipaddr, 0, sizeof(uip_ipaddr_t));
  memcpy(ipaddr, &prefix->prefix, (prefix->length + 7) / 8);
  uip_ds6_set_addr_iid(ipaddr, &lladdr);
}
/*---------------------------------------------------------------------------*/
static void
check_prefix(rpl_prefix_t *last_prefix, rpl_prefix_t *new_prefix)
{
  uip_ipaddr_t ipaddr;
  uip_ds6_addr_t *rep;

  if(last_prefix != NULL && new_prefix != NULL &&
      last_prefix->length == new_prefix->length &&
      uip_ipaddr_prefixcmp(&last_prefix->prefix, &new_prefix->prefix, new_prefix->length) &&
      last_prefix->flags == new_prefix->flags) {
    /* Nothing has changed. */
    return;
  }

  if(last_prefix != NULL) {
    set_ip_from_prefix(&ipaddr, last_prefix);
    rep = uip_ds6_addr_lookup(&ipaddr);
    if(rep != NULL) {
      PRINTF("RPL: removing global IP address ");
      PRINT6ADDR(&ipaddr);
      PRINTF("\n");
      uip_ds6_addr_rm(rep);
    }
  }

  if(new_prefix != NULL) {
    set_ip_from_prefix(&ipaddr, new_prefix);
    if(uip_ds6_addr_lookup(&ipaddr) == NULL) {
      PRINTF("RPL: adding global IP address ");
      PRINT6ADDR(&ipaddr);
      PRINTF("\n");
      uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
    }
  }
}
/*---------------------------------------------------------------------------*/
int
rpl_set_prefix(rpl_dag_t *dag, uip_ipaddr_t *prefix, unsigned len)
{
  if(len > 128) {
    return 0;
  }

  memset(&dag->prefix_info.prefix, 0, sizeof(dag->prefix_info.prefix));
  memcpy(&dag->prefix_info.prefix, prefix, (len + 7) / 8);
  dag->prefix_info.length = len;
  dag->prefix_info.flags = UIP_ND6_RA_FLAG_AUTONOMOUS;
  PRINTF("RPL: Prefix set - will announce this in DIOs\n");
  /* Autoconfigure an address if this node does not already have an address
     with this prefix. */
  check_prefix(NULL, &dag->prefix_info);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
rpl_set_default_route(rpl_instance_t *instance, uip_ipaddr_t *from)
{
  if(instance->def_route != NULL) {
    PRINTF("RPL: Removing default route through ");
    PRINT6ADDR(&instance->def_route->ipaddr);
    PRINTF("\n");
    uip_ds6_defrt_rm(instance->def_route);
    instance->def_route = NULL;
  }

  if(from != NULL) {
    PRINTF("RPL: Adding default route through ");
    PRINT6ADDR(from);
    PRINTF("\n");
    instance->def_route = uip_ds6_defrt_add(from,
        RPL_LIFETIME(instance,
            instance->default_lifetime));
    if(instance->def_route == NULL) {
      return 0;
    }
  } else {
    PRINTF("RPL: Removing default route\n");
    if(instance->def_route != NULL) {
      uip_ds6_defrt_rm(instance->def_route);
    } else {
      PRINTF("RPL: Not actually removing default route, since instance had no default route\n");
    }
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
rpl_instance_t *
rpl_alloc_instance(uint8_t instance_id)
{
  rpl_instance_t *instance, *end;

  for(instance = &instance_table[0], end = instance + RPL_MAX_INSTANCES;
      instance < end; ++instance) {
    if(instance->used == 0) {
      memset(instance, 0, sizeof(*instance));
      instance->instance_id = instance_id;
      instance->def_route = NULL;
      instance->used = 1;
      return instance;
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
rpl_dag_t *
rpl_alloc_dag(uint8_t instance_id, uip_ipaddr_t *dag_id)
{
  rpl_dag_t *dag, *end;
  rpl_instance_t *instance;

  instance = rpl_get_instance(instance_id);
  if(instance == NULL) {
    instance = rpl_alloc_instance(instance_id);
    if(instance == NULL) {
      RPL_STAT(rpl_stats.mem_overflows++);
      return NULL;
    }
  }

  for(dag = &instance->dag_table[0], end = dag + RPL_MAX_DAG_PER_INSTANCE; dag < end; ++dag) {
    if(!dag->used) {
      memset(dag, 0, sizeof(*dag));
      dag->used = 1;
      dag->rank = INFINITE_RANK;
      dag->min_rank = INFINITE_RANK;
      dag->instance = instance;
      return dag;
    }
  }

  RPL_STAT(rpl_stats.mem_overflows++);
  rpl_free_instance(instance);
  return NULL;
}
/*---------------------------------------------------------------------------*/
void
rpl_set_default_instance(rpl_instance_t *instance)
{
  default_instance = instance;
}
/*---------------------------------------------------------------------------*/
void
rpl_free_instance(rpl_instance_t *instance)
{
  rpl_dag_t *dag;
  rpl_dag_t *end;

  PRINTF("RPL: Leaving the instance %u\n", instance->instance_id);

  /* Remove any DAG inside this instance */
  for(dag = &instance->dag_table[0], end = dag + RPL_MAX_DAG_PER_INSTANCE; dag < end; ++dag) {
    if(dag->used) {
      rpl_free_dag(dag);
    }
  }

  rpl_set_default_route(instance, NULL);

  ctimer_stop(&instance->dio_timer);
  ctimer_stop(&instance->dao_timer);

  if(default_instance == instance) {
    default_instance = NULL;
  }

  instance->used = 0;
}
/*---------------------------------------------------------------------------*/
void
rpl_free_dag(rpl_dag_t *dag)
{
  if(dag->joined) {
    PRINTF("RPL: Leaving the DAG ");
    PRINT6ADDR(&dag->dag_id);
    PRINTF("\n");
    dag->joined = 0;

    /* Remove routes installed by DAOs. */
    rpl_remove_routes(dag);

    /* Remove autoconfigured address */
    if((dag->prefix_info.flags & UIP_ND6_RA_FLAG_AUTONOMOUS)) {
      check_prefix(&dag->prefix_info, NULL);
    }

    remove_neighbors(dag, 0);
  }
  dag->used = 0;
}
/*---------------------------------------------------------------------------*/
rpl_neighbor_t *
rpl_add_neighbor(rpl_dag_t *dag, rpl_dio_t *dio, uip_ipaddr_t *addr)
{
  rpl_neighbor_t *nbr = NULL;
  /* Is the neighbor known by ds6? Drop this request if not.
   * Typically, the neighbor is added upon receiving a DIO. */
  lladdr_t *lladdr = uip_ds6_nbr_lladdr_from_ipaddr(addr);

  if(lladdr != NULL) {
    /* Add neighbor in rpl_neighbors */
    nbr = nbr_table_add_lladdr(rpl_neighbors, lladdr);
    nbr->dag = dag;
    nbr->rank = dio->rank;
    nbr->dtsn = dio->dtsn;
    nbr->link_metric = RPL_INIT_LINK_METRIC;
#if RPL_DAG_MC != RPL_DAG_MC_NONE
    memcpy(&nbr->mc, &dio->mc, sizeof(nbr->mc));
#endif /* RPL_DAG_MC != RPL_DAG_MC_NONE */
  }

  return nbr;
}

/*---------------------------------------------------------------------------*/
static rpl_neighbor_t *
find_neighbor_any_dag_any_instance(uip_ipaddr_t *addr)
{
  uip_ds6_nbr_t *ds6_nbr = uip_ds6_nbr_lookup(addr);
  lladdr_t *lladdr = uip_ds6_nbr_get_ll(ds6_nbr);
  return nbr_table_get_from_lladdr(rpl_neighbors, lladdr);
}

/*---------------------------------------------------------------------------*/
rpl_neighbor_t *
rpl_find_neighbor(rpl_dag_t *dag, uip_ipaddr_t *addr)
{
  rpl_neighbor_t *nbr = find_neighbor_any_dag_any_instance(addr);
  if(nbr->dag == dag) {
    return nbr;
  } else {
    return NULL;
  }
}

/*---------------------------------------------------------------------------*/
static rpl_dag_t *
find_neighbor_dag(rpl_instance_t *instance, uip_ipaddr_t *addr)
{
  rpl_neighbor_t *nbr = find_neighbor_any_dag_any_instance(addr);
  if(nbr != NULL) {
    return nbr->dag;
  } else {
    return NULL;
  }
}
/*---------------------------------------------------------------------------*/
rpl_neighbor_t *
rpl_find_neighbor_any_dag(rpl_instance_t *instance, uip_ipaddr_t *addr)
{
  rpl_neighbor_t *nbr = find_neighbor_any_dag_any_instance(addr);
  if(nbr && nbr->dag && nbr->dag->instance == instance) {
    return nbr;
  } else {
    return NULL;
  }
}
/*---------------------------------------------------------------------------*/
rpl_dag_t *
rpl_select_dag(rpl_instance_t *instance, rpl_neighbor_t *nbr)
{
  rpl_neighbor_t *last_parent;
  rpl_dag_t *dag, *end, *best_dag;
  rpl_rank_t old_rank;

  old_rank = instance->current_dag->rank;
  last_parent = instance->current_dag->preferred_parent;

  best_dag = instance->current_dag;
  if(best_dag->rank != ROOT_RANK(instance)) {
    if(rpl_select_parent(nbr->dag) != NULL) {
      if(nbr->dag != best_dag) {
        best_dag = instance->of->best_dag(best_dag, nbr->dag);
      }
    } else if(nbr->dag == best_dag) {
      best_dag = NULL;
      for(dag = &instance->dag_table[0], end = dag + RPL_MAX_DAG_PER_INSTANCE; dag < end; ++dag) {
        if(dag->used && dag->preferred_parent != NULL && dag->preferred_parent->rank != INFINITE_RANK) {
          if(best_dag == NULL) {
            best_dag = dag;
          } else {
            best_dag = instance->of->best_dag(best_dag, dag);
          }
        }
      }
    }
  }

  if(best_dag == NULL) {
    /* No parent found: the calling function handle this problem. */
    return NULL;
  }

  if(instance->current_dag != best_dag) {
    /* Remove routes installed by DAOs. */
    rpl_remove_routes(instance->current_dag);

    PRINTF("RPL: New preferred DAG: ");
    PRINT6ADDR(&best_dag->dag_id);
    PRINTF("\n");

    if(best_dag->prefix_info.flags & UIP_ND6_RA_FLAG_AUTONOMOUS) {
      check_prefix(&instance->current_dag->prefix_info, &best_dag->prefix_info);
    } else if(instance->current_dag->prefix_info.flags & UIP_ND6_RA_FLAG_AUTONOMOUS) {
      check_prefix(&instance->current_dag->prefix_info, NULL);
    }

    best_dag->joined = 1;
    instance->current_dag->joined = 0;
    instance->current_dag = best_dag;
  }

  instance->of->update_metric_container(instance);
  /* Update the DAG rank. */
  best_dag->rank = instance->of->calculate_rank(best_dag->preferred_parent, 0);
  if(best_dag->rank < best_dag->min_rank) {
    best_dag->min_rank = best_dag->rank;
  } else if(!acceptable_rank(best_dag, best_dag->rank)) {
    PRINTF("RPL: New rank unacceptable!\n");
    instance->current_dag->preferred_parent = NULL;
    if(instance->mop != RPL_MOP_NO_DOWNWARD_ROUTES && last_parent != NULL) {
      /* Send a No-Path DAO to the removed preferred parent. */
      dao_output(last_parent, RPL_ZERO_LIFETIME);
    }
    return NULL;
  }

  if(best_dag->preferred_parent != last_parent) {
    rpl_set_default_route(instance, rpl_get_neighbor_ipaddr(best_dag->preferred_parent));
    PRINTF("RPL: Changed preferred parent, rank changed from %u to %u\n",
        (unsigned)old_rank, best_dag->rank);
    RPL_STAT(rpl_stats.parent_switch++);
    if(instance->mop != RPL_MOP_NO_DOWNWARD_ROUTES) {
      if(last_parent != NULL) {
        /* Send a No-Path DAO to the removed preferred parent. */
        dao_output(last_parent, RPL_ZERO_LIFETIME);
      }
      /* The DAO parent set changed - schedule a DAO transmission. */
      RPL_LOLLIPOP_INCREMENT(instance->dtsn_out);
      rpl_schedule_dao(instance);
    }
    rpl_reset_dio_timer(instance);
  } else if(best_dag->rank != old_rank) {
    PRINTF("RPL: Preferred parent update, rank changed from %u to %u\n",
        (unsigned)old_rank, best_dag->rank);
  }
  return best_dag;
}
/*---------------------------------------------------------------------------*/
rpl_neighbor_t *
rpl_select_parent(rpl_dag_t *dag)
{
  rpl_neighbor_t *nbr, *best;

  best = NULL;

  nbr = nbr_table_head(rpl_neighbors);
  while(nbr != NULL) {
    if(nbr->rank == INFINITE_RANK) {
      /* ignore this neighbor */
    } else if(best == NULL) {
      best = nbr;
    } else {
      best = dag->instance->of->best_parent(best, nbr);
    }
    nbr = nbr_table_next(rpl_neighbors, nbr);
  }

  if(best != NULL) {
    dag->preferred_parent = best;
  }

  return best;
}
/*---------------------------------------------------------------------------*/
void
rpl_remove_neighbor(rpl_neighbor_t *neighbor)
{
  PRINTF("RPL: Removing neighbor ");
  PRINT6ADDR(rpl_get_neighbor_ipaddr(neighbor));
  PRINTF("\n");

  rpl_nullify_neighbor(neighbor);

  nbr_table_remove(rpl_neighbors, neighbor);
}
/*---------------------------------------------------------------------------*/
void
rpl_nullify_neighbor(rpl_neighbor_t *neighbor)
{
  rpl_dag_t *dag = neighbor->dag;
  if(neighbor == dag->preferred_parent) {
    dag->preferred_parent = NULL;
    dag->rank = INFINITE_RANK;
    if(dag->joined) {
      if(dag->instance->def_route != NULL) {
        PRINTF("RPL: Removing default route ");
        PRINT6ADDR(rpl_get_neighbor_ipaddr(neighbor));
        PRINTF("\n");
        uip_ds6_defrt_rm(dag->instance->def_route);
        dag->instance->def_route = NULL;
      }
      dao_output(neighbor, RPL_ZERO_LIFETIME);
    }
  }

  PRINTF("RPL: Nullifying neighbor ");
  PRINT6ADDR(rpl_get_neighbor_ipaddr(neighbor));
  PRINTF("\n");
}
/*---------------------------------------------------------------------------*/
void
rpl_move_neighbor(rpl_dag_t *dag_src, rpl_dag_t *dag_dst, rpl_neighbor_t *neighbor)
{
  if(neighbor == dag_src->preferred_parent) {
    dag_src->preferred_parent = NULL;
    dag_src->rank = INFINITE_RANK;
    if(dag_src->joined && dag_src->instance->def_route != NULL) {
      PRINTF("RPL: Removing default route ");
      PRINT6ADDR(rpl_get_neighbor_ipaddr(neighbor));
      PRINTF("\n");
      PRINTF("rpl_move_neighbor\n");
      uip_ds6_defrt_rm(dag_src->instance->def_route);
      dag_src->instance->def_route = NULL;
    }
  } else if(dag_src->joined) {
    /* Remove uIPv6 routes that have this parent as the next hop. */
    rpl_remove_routes_by_nexthop(rpl_get_neighbor_ipaddr(neighbor), dag_src);
  }

  PRINTF("RPL: Moving neighbor ");
  PRINT6ADDR(rpl_get_neighbor_ipaddr(neighbor));
  PRINTF("\n");

  neighbor->dag = dag_dst;
}
/*---------------------------------------------------------------------------*/
rpl_dag_t *
rpl_get_any_dag(void)
{
  int i;

  for(i = 0; i < RPL_MAX_INSTANCES; ++i) {
    if(instance_table[i].used && instance_table[i].current_dag->joined) {
      return instance_table[i].current_dag;
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
rpl_instance_t *
rpl_get_instance(uint8_t instance_id)
{
  int i;

  for(i = 0; i < RPL_MAX_INSTANCES; ++i) {
    if(instance_table[i].used && instance_table[i].instance_id == instance_id) {
      return &instance_table[i];
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
rpl_of_t *
rpl_find_of(rpl_ocp_t ocp)
{
  unsigned int i;

  for(i = 0;
      i < sizeof(objective_functions) / sizeof(objective_functions[0]);
      i++) {
    if(objective_functions[i]->ocp == ocp) {
      return objective_functions[i];
    }
  }

  return NULL;
}
/*---------------------------------------------------------------------------*/
void
rpl_join_instance(uip_ipaddr_t *from, rpl_dio_t *dio)
{
  rpl_instance_t *instance;
  rpl_dag_t *dag;
  rpl_neighbor_t *nbr;
  rpl_of_t *of;

  dag = rpl_alloc_dag(dio->instance_id, &dio->dag_id);
  if(dag == NULL) {
    PRINTF("RPL: Failed to allocate a DAG object!\n");
    return;
  }

  instance = dag->instance;

  nbr = rpl_add_neighbor(dag, dio, from);
  PRINTF("RPL: Adding ");
  PRINT6ADDR(from);
  PRINTF(" as a neighbor: ");
  if(nbr == NULL) {
    PRINTF("failed\n");
    instance->used = 0;
    return;
  }
  nbr->dtsn = dio->dtsn;
  PRINTF("succeeded\n");

  /* Determine the objective function by using the
     objective code point of the DIO. */
  of = rpl_find_of(dio->ocp);
  if(of == NULL) {
    PRINTF("RPL: DIO for DAG instance %u does not specify a supported OF\n",
        dio->instance_id);
    rpl_remove_neighbor(nbr);
    instance->used = 0;
    return;
  }

  /* Autoconfigure an address if this node does not already have an address
     with this prefix. */
  if(dio->prefix_info.flags & UIP_ND6_RA_FLAG_AUTONOMOUS) {
    check_prefix(NULL, &dio->prefix_info);
  }

  dag->joined = 1;
  dag->preference = dio->preference;
  dag->grounded = dio->grounded;
  dag->version = dio->version;

  instance->of = of;
  instance->mop = dio->mop;
  instance->current_dag = dag;
  instance->dtsn_out = RPL_LOLLIPOP_INIT;

  instance->max_rankinc = dio->dag_max_rankinc;
  instance->min_hoprankinc = dio->dag_min_hoprankinc;
  instance->dio_intdoubl = dio->dag_intdoubl;
  instance->dio_intmin = dio->dag_intmin;
  instance->dio_intcurrent = instance->dio_intmin + instance->dio_intdoubl;
  instance->dio_redundancy = dio->dag_redund;
  instance->default_lifetime = dio->default_lifetime;
  instance->lifetime_unit = dio->lifetime_unit;

  memcpy(&dag->dag_id, &dio->dag_id, sizeof(dio->dag_id));

  /* Copy prefix information from the DIO into the DAG object. */
  memcpy(&dag->prefix_info, &dio->prefix_info, sizeof(rpl_prefix_t));

  dag->preferred_parent = nbr;
  instance->of->update_metric_container(instance);
  dag->rank = instance->of->calculate_rank(nbr, 0);
  /* So far this is the lowest rank we are aware of. */
  dag->min_rank = dag->rank;

  if(default_instance == NULL) {
    default_instance = instance;
  }

  PRINTF("RPL: Joined DAG with instance ID %u, rank %hu, DAG ID ",
      dio->instance_id, dag->rank);
  PRINT6ADDR(&dag->dag_id);
  PRINTF("\n");

  ANNOTATE("#A join=%u\n", dag->dag_id.u8[sizeof(dag->dag_id) - 1]);

  rpl_reset_dio_timer(instance);
  rpl_set_default_route(instance, from);

  if(instance->mop != RPL_MOP_NO_DOWNWARD_ROUTES) {
    rpl_schedule_dao(instance);
  } else {
    PRINTF("RPL: The DIO does not meet the prerequisites for sending a DAO\n");
  }
}

/*---------------------------------------------------------------------------*/
void
rpl_add_dag(uip_ipaddr_t *from, rpl_dio_t *dio)
{
  rpl_instance_t *instance;
  rpl_dag_t *dag, *previous_dag;
  rpl_neighbor_t *nbr;
  rpl_of_t *of;

  dag = rpl_alloc_dag(dio->instance_id, &dio->dag_id);
  if(dag == NULL) {
    PRINTF("RPL: Failed to allocate a DAG object!\n");
    return;
  }

  instance = dag->instance;

  previous_dag = find_neighbor_dag(instance, from);
  if(previous_dag == NULL) {
    PRINTF("RPL: Adding ");
    PRINT6ADDR(from);
    PRINTF(" as a neighbor: ");
    nbr = rpl_add_neighbor(dag, dio, from);
    if(nbr == NULL) {
      PRINTF("failed\n");
      dag->used = 0;
      return;
    }
    PRINTF("succeeded\n");
  } else {
    nbr = rpl_find_neighbor(previous_dag, from);
    if(nbr != NULL) {
      rpl_move_neighbor(previous_dag, dag, nbr);
    }
  }

  /* Determine the objective function by using the
     objective code point of the DIO. */
  of = rpl_find_of(dio->ocp);
  if(of != instance->of ||
      instance->mop != dio->mop ||
      instance->max_rankinc != dio->dag_max_rankinc ||
      instance->min_hoprankinc != dio->dag_min_hoprankinc ||
      instance->dio_intdoubl != dio->dag_intdoubl ||
      instance->dio_intmin != dio->dag_intmin ||
      instance->dio_redundancy != dio->dag_redund ||
      instance->default_lifetime != dio->default_lifetime ||
      instance->lifetime_unit != dio->lifetime_unit) {
    PRINTF("RPL: DIO for DAG instance %u uncompatible with previos DIO\n",
        dio->instance_id);
    rpl_remove_neighbor(nbr);
    dag->used = 0;
    return;
  }

  dag->used = 1;
  dag->grounded = dio->grounded;
  dag->preference = dio->preference;
  dag->version = dio->version;

  memcpy(&dag->dag_id, &dio->dag_id, sizeof(dio->dag_id));

  /* copy prefix information into the dag */
  memcpy(&dag->prefix_info, &dio->prefix_info, sizeof(rpl_prefix_t));

  dag->preferred_parent = nbr;
  dag->rank = instance->of->calculate_rank(nbr, 0);
  dag->min_rank = dag->rank; /* So far this is the lowest rank we know of. */

  PRINTF("RPL: Joined DAG with instance ID %u, rank %hu, DAG ID ",
      dio->instance_id, dag->rank);
  PRINT6ADDR(&dag->dag_id);
  PRINTF("\n");

  ANNOTATE("#A join=%u\n", dag->dag_id.u8[sizeof(dag->dag_id) - 1]);

  rpl_process_neighbor_event(instance, nbr);
  nbr->dtsn = dio->dtsn;
}

/*---------------------------------------------------------------------------*/
static void
global_repair(uip_ipaddr_t *from, rpl_dag_t *dag, rpl_dio_t *dio)
{
  rpl_neighbor_t *nbr;

  remove_neighbors(dag, 0);
  dag->version = dio->version;
  dag->instance->of->reset(dag);
  dag->min_rank = INFINITE_RANK;
  RPL_LOLLIPOP_INCREMENT(dag->instance->dtsn_out);

  nbr = rpl_add_neighbor(dag, dio, from);
  if(nbr == NULL) {
    PRINTF("RPL: Failed to add a neighbor during the global repair\n");
    dag->rank = INFINITE_RANK;
  } else {
    dag->rank = dag->instance->of->calculate_rank(nbr, 0);
    dag->min_rank = dag->rank;
    rpl_process_neighbor_event(dag->instance, nbr);
  }

  PRINTF("RPL: Participating in a global repair (version=%u, rank=%hu)\n",
      dag->version, dag->rank);

  RPL_STAT(rpl_stats.global_repairs++);
}
/*---------------------------------------------------------------------------*/
void
rpl_local_repair(rpl_instance_t *instance)
{
  int i;

  PRINTF("RPL: Starting a local instance repair\n");
  for(i = 0; i < RPL_MAX_DAG_PER_INSTANCE; i++) {
    if(instance->dag_table[i].used) {
      instance->dag_table[i].rank = INFINITE_RANK;
      nullify_neighbors(&instance->dag_table[i], 0);
    }
  }

  rpl_reset_dio_timer(instance);

  RPL_STAT(rpl_stats.local_repairs++);
}
/*---------------------------------------------------------------------------*/
void
rpl_recalculate_ranks(void)
{
  rpl_neighbor_t *nbr;

  /*
   * We recalculate ranks when we receive feedback from the system rather
   * than RPL protocol messages. This periodical recalculation is called
   * from a timer in order to keep the stack depth reasonably low.
   */
  nbr = nbr_table_head(rpl_neighbors);
  while(nbr != NULL) {
    if(nbr->dag != NULL && nbr->dag->instance && nbr->updated) {
      nbr->updated = 0;
      if(!rpl_process_neighbor_event(nbr->dag->instance, nbr)) {
        PRINTF("RPL: A neighbor was dropped\n");
      }
    }
    nbr = nbr_table_next(rpl_neighbors, nbr);
  }
}
/*---------------------------------------------------------------------------*/
int
rpl_process_neighbor_event(rpl_instance_t *instance, rpl_neighbor_t *nbr)
{
  int return_value = 1;
#if DEBUG
  rpl_rank_t old_rank = instance->current_dag->rank;
#endif

  if(!acceptable_rank(nbr->dag, nbr->rank)) {
    /* The candidate parent is no longer valid: the rank increase resulting
       from the choice of it as a parent would be too high. */
    PRINTF("RPL: Unacceptable rank %u\n", (unsigned)nbr->rank);
    rpl_nullify_neighbor(nbr);
    if(nbr != instance->current_dag->preferred_parent) {
      return 0;
    } else {
      return_value = 0;
    }
  }

  if(rpl_select_dag(instance, nbr) == NULL) {
    /* No suitable neighbor; trigger a local repair. */
    PRINTF("RPL: No neighbor found in any DAG\n");
    rpl_local_repair(instance);
    return 0;
  }

#if DEBUG
  if(DAG_RANK(old_rank, instance) != DAG_RANK(instance->current_dag->rank, instance)) {
    PRINTF("RPL: Moving in the instance from rank %hu to %hu\n",
        DAG_RANK(old_rank, instance), DAG_RANK(instance->current_dag->rank, instance));
    if(instance->current_dag->rank != INFINITE_RANK) {
      PRINTF("RPL: The preferred parent is ");
      PRINT6ADDR(rpl_get_neighbor_ipaddr(instance->current_dag->preferred_parent));
      PRINTF(" (rank %u)\n",
          (unsigned)DAG_RANK(instance->current_dag->preferred_parent->rank, instance));
    } else {
      PRINTF("RPL: We don't have any parent");
    }
  }
#endif /* DEBUG */

  return return_value;
}
/*---------------------------------------------------------------------------*/
void
rpl_process_dio(uip_ipaddr_t *from, rpl_dio_t *dio)
{
  rpl_instance_t *instance;
  rpl_dag_t *dag, *previous_dag;
  rpl_neighbor_t *nbr;

  if(dio->mop != RPL_MOP_DEFAULT) {
    PRINTF("RPL: Ignoring a DIO with an unsupported MOP: %d\n", dio->mop);
    return;
  }

  dag = get_dag(dio->instance_id, &dio->dag_id);
  instance = rpl_get_instance(dio->instance_id);

  if(dag != NULL && instance != NULL) {
    if(lollipop_greater_than(dio->version, dag->version)) {
      if(dag->rank == ROOT_RANK(instance)) {
        PRINTF("RPL: Root received inconsistent DIO version number\n");
        dag->version = dio->version;
        RPL_LOLLIPOP_INCREMENT(dag->version);
        rpl_reset_dio_timer(instance);
      } else {
        global_repair(from, dag, dio);
      }
      return;
    }

    if(lollipop_greater_than(dag->version, dio->version)) {
      /* The DIO sender is on an older version of the DAG. */
      PRINTF("RPL: old version received => inconsistency detected\n");
      if(dag->joined) {
        rpl_reset_dio_timer(instance);
        return;
      }
    }
  }

  if(dio->rank == INFINITE_RANK) {
    PRINTF("RPL: Ignoring DIO from node with infinite rank: ");
    PRINT6ADDR(from);
    PRINTF("\n");
    return;
  }

  if(instance == NULL) {
    PRINTF("RPL: New instance detected: Joining...\n");
    rpl_join_instance(from, dio);
    return;
  }

  if(dag == NULL) {
    PRINTF("RPL: Adding new DAG to known instance.\n");
    rpl_add_dag(from, dio);
    return;
  }


  if(dio->rank < ROOT_RANK(instance)) {
    PRINTF("RPL: Ignoring DIO with too low rank: %u\n",
        (unsigned)dio->rank);
    return;
  } else if(dio->rank == INFINITE_RANK && dag->joined) {
    rpl_reset_dio_timer(instance);
  }

  if(dag->rank == ROOT_RANK(instance)) {
    if(dio->rank != INFINITE_RANK) {
      instance->dio_counter++;
    }
    return;
  }

  /*
   * At this point, we know that this DIO pertains to a DAG that
   * we are already part of. We consider the sender of the DIO to be
   * a candidate parent, and let rpl_process_neighbor_event decide
   * whether to keep it in the set.
   */

  nbr = rpl_find_neighbor(dag, from);
  if(nbr == NULL) {
    previous_dag = find_neighbor_dag(instance, from);
    if(previous_dag == NULL) {
      /* Add the DIO sender as a candidate parent. */
      nbr = rpl_add_neighbor(dag, dio, from);
      if(nbr == NULL) {
        PRINTF("RPL: Failed to add a new neighbor (");
        PRINT6ADDR(from);
        PRINTF(")\n");
        return;
      }
      PRINTF("RPL: New candidate parent with rank %u: ", (unsigned)nbr->rank);
      PRINT6ADDR(from);
      PRINTF("\n");
    } else {
      nbr = rpl_find_neighbor(previous_dag, from);
      if(nbr != NULL) {
        rpl_move_neighbor(previous_dag, dag, nbr);
      }
    }
  } else {
    if(nbr->rank == dio->rank) {
      PRINTF("RPL: Received consistent DIO\n");
      if(dag->joined) {
        instance->dio_counter++;
      }
    } else {
      nbr->rank=dio->rank;
    }
  }

  PRINTF("RPL: preferred DAG ");
  PRINT6ADDR(&instance->current_dag->dag_id);
  PRINTF(", rank %u, min_rank %u, ",
      instance->current_dag->rank, instance->current_dag->min_rank);
  PRINTF("parent rank %u, link metric %u, instance etx %u\n",
      nbr->rank, nbr->link_metric, instance->mc.obj.etx);

  /* We have allocated a candidate parent; process the DIO further. */

#if RPL_DAG_MC != RPL_DAG_MC_NONE
  memcpy(&nbr->mc, &dio->mc, sizeof(nbr->mc));
#endif /* RPL_DAG_MC != RPL_DAG_MC_NONE */
  if(rpl_process_neighbor_event(instance, nbr) == 0) {
    PRINTF("RPL: The candidate parent is rejected\n");
    return;
  }

  /* We don't use route control, so we can have only one official parent. */
  if(dag->joined && nbr == dag->preferred_parent) {
    if(should_send_dao(instance, dio, nbr)) {
      RPL_LOLLIPOP_INCREMENT(instance->dtsn_out);
      rpl_schedule_dao(instance);
    }
    /* We received a new DIO from our preferred parent.
     * Call uip_ds6_defrt_add to set a fresh value for the lifetime counter */
    uip_ds6_defrt_add(from, RPL_LIFETIME(instance, instance->default_lifetime));
  }
  nbr->dtsn = dio->dtsn;
}
/*---------------------------------------------------------------------------*/
