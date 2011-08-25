/*
 * =====================================================================================
// Copyright 2010 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2010, Sandia Corporation
// All rights reserved.
// 
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.
 * =====================================================================================
 */

#ifndef  _ROUTER_PARAMS_H_INC
#define  _ROUTER_PARAMS_H_INC

#include	"../interfaces/genericHeader.h"

enum defaults { 
    DEFAULT_PORTS=5, 
    DEFAULT_VCS=1, 
    DEFAULT_NO_RCR=4, 
    DEFAULT_RC_M = static_cast<int>(XY), 
    DEFAULT_NO_NODES = 64,
    DEFAULT_GRID_SIZE = 8
};

/*
 * =====================================================================================
 *        Class:  Router_params
 *  Description:  class that holds all the parameters used by the router and
 *  the routers sub components. By default the router instantiated needs a
 *  node_id to be specified) is a 5 port/1vc/4cr noc router in a MESH(64-node/8x8) 
 *  with XY DOR routing. See genericheader for defaults.
 *
 *  parmeters may be updated from a runtime config and are shared by all
 *  sub-components of the router.
 *
 *  Try to keep this singleton 
 *  try to make the members non static.. may want to update 
 * =====================================================================================
 */
class Router_params
{
    public:
        static Router_params& getInstance()
        {
            if (r_param == NULL )
                r_param = new Router_params();

            return *r_param;
        }

        // param list
        uint16_t ports;
        uint16_t vcs;
        uint16_t credits;   // flit level
        routing_scheme_t rc_scheme; 
        uint16_t no_nodes;
        uint16_t grid_size; // for mesh & tori helps determine the x and y co-ord
        bool use_virtual_networks;
        uint16_t buffer_size;

    private:
        static Router_params* r_param;

        Router_params(uint16_t p=DEFAULT_PORTS, uint16_t v= DEFAULT_VCS, 
                      uint16_t cr=DEFAULT_NO_RCR,
                      routing_scheme_t r=(routing_scheme_t)DEFAULT_RC_M, 
                      uint16_t no_n=DEFAULT_NO_NODES, uint16_t gs=DEFAULT_GRID_SIZE, 
                      bool vn_flag=false, uint16_t b=DEFAULT_NO_RCR)
        {
            ports = p;
            vcs = v;
            credits = cr;
            rc_scheme = r;
            no_nodes = no_n;
            grid_size = gs;
            use_virtual_networks = vn_flag;
            buffer_size = b;
        }

        ~Router_params() {}

}; /* -----  end of class Router_params  ----- */

// Instance in genericHeader.cc
extern Router_params& r_param; 

#endif   /* ----- #ifndef _ROUTER_PARAMS_H_INC  ----- */
