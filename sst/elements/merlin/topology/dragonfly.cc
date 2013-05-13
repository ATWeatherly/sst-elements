// Copyright 2009-2013 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2009-2013, Sandia Corporation
// All rights reserved.
// 
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.
//
#include <stdlib.h>

#include <sst_config.h>
#include <sst/core/serialization/element.h>


#include "dragonfly.h"

using namespace SST::Merlin;

#define DPRINTF( fmt, args...) __DBG( DBG_NETWORK, topo_dragonfly, fmt, ## args )

/*
 * Port Layout:
 * [0, params.p)                    // Hosts 0 -> params.p
 * [params.p, params.p+params.a-1)  // Routers within this group
 * [params.p+params.a-1, params.k)  // Other groups
 */

topo_dragonfly::topo_dragonfly(Params &p) :
    Topology()
{
    params.p = (uint32_t)p.find_integer("dragonfly:hosts_per_router");
    params.a = (uint32_t)p.find_integer("dragonfly:routers_per_group");
    params.k = (uint32_t)p.find_integer("num_ports");
    params.h = (uint32_t)p.find_integer("dragonfly:intergroup_per_router");
    params.g = (uint32_t)p.find_integer("dragonfly:num_groups");

    std::string route_algo = p.find_string("dragonfly:algorithm", "minimal");

    if ( !route_algo.compare("valiant") ) {
        if ( params.g <= 2 ) {
            /* 2 or less groups... no point in valiant */
            algorithm = MINIMAL;
        } else {
            algorithm = VALIANT;
        }
    } else {
        algorithm = MINIMAL;
    }

    uint32_t id = p.find_integer("id");
    group_id = id / params.a;
    router_id = id % params.a;
    DPRINTF("%u:%u:  ID: %u   Params:  p = %u  a = %u  k = %u  h = %u  g = %u\n",
            group_id, router_id, id, params.p, params.a, params.k, params.h, params.g);
}


topo_dragonfly::~topo_dragonfly()
{
}


void topo_dragonfly::route(int port, int vc, internal_router_event* ev)
{
    topo_dragonfly_event *td_ev = static_cast<topo_dragonfly_event*>(ev);

    if ( (uint32_t)port >= (params.p + params.a-1) ) {
        /* Came in from another group.  Increment VC */
        td_ev->setVC(vc+1);
    }


    /* Minimal Route */
    uint32_t next_port = 0;
    if ( td_ev->dest.group != group_id ) {
        if ( td_ev->dest.mid_group != group_id ) {
            next_port = port_for_group(td_ev->dest.mid_group);
        } else {
            next_port = port_for_group(td_ev->dest.group);
        }
    } else if ( td_ev->dest.router != router_id ) {
        next_port = port_for_router(td_ev->dest.router);
    } else {
        next_port = td_ev->dest.host;
    }

    DPRINTF("%u:%u, Recv: %d/%d  Setting Next Port/VC:  %u/%u\n", group_id, router_id, port, vc, next_port, td_ev->getVC());
    td_ev->setNextPort(next_port);
}


internal_router_event* topo_dragonfly::process_input(RtrEvent* ev)
{
    dgnflyAddr dstAddr = {0, 0, 0, 0};
    idToLocation(ev->dest, &dstAddr);

    switch (algorithm) {
    case MINIMAL:
        dstAddr.mid_group = dstAddr.group;
        break;
    case VALIANT:
        if ( dstAddr.group == group_id ) {
            // staying here.
            dstAddr.mid_group = dstAddr.group;
        } else {
            do {
                dstAddr.mid_group = rand() % params.g;
            } while ( dstAddr.mid_group == group_id || dstAddr.mid_group == dstAddr.group );
        }
        break;
    }

    DPRINTF("Init packet from %d to %d to %u:%u:%u:%u\n", ev->src, ev->dest, dstAddr.group, dstAddr.mid_group, dstAddr.router, dstAddr.host);

    topo_dragonfly_event *td_ev = new topo_dragonfly_event(dstAddr);
    td_ev->setEncapsulatedEvent(ev);

    return td_ev;
}


Topology::PortState topo_dragonfly::getPortState(int port) const
{
    if ( (uint32_t)port < params.p ) return R2N;
    else return R2R;
}


void topo_dragonfly::idToLocation(int id, dgnflyAddr *location) const
{
    uint32_t hosts_per_group = params.p * params.a;
    location->group = id / hosts_per_group;
    location->router = (id % hosts_per_group) / params.p;
    location->host = id % params.p;
}


uint32_t topo_dragonfly::router_to_group(uint32_t group) const
{
    /* For now, assume only 1 connection to each group */
    if ( group < group_id ) {
        return group / params.h;
    } else if ( group > group_id ) {
        return (group-1) / params.h;
    } else {
        _abort(topo_dragonfly, "Trying to find router to own group.\n");
    }
}


/* returns local router port if group can't be reached from this router */
uint32_t topo_dragonfly::port_for_group(uint32_t group) const
{
    uint32_t tgt_rtr = router_to_group(group);
    if ( tgt_rtr == router_id ) {
        uint32_t port = params.p + params.a-1;
        if ( group < group_id ) {
            port += (group % params.h);
        } else {
            port += ((group-1) % params.h);
        }
        return port;
    } else {
        return port_for_router(tgt_rtr);
    }

}


uint32_t topo_dragonfly::port_for_router(uint32_t router) const
{
    uint32_t tgt = params.p + router;
    if ( router > router_id ) tgt--;
    return tgt;
}
