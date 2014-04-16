// Copyright 2009-2014 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2009-2014, Sandia Corporation
// All rights reserved.
// 
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#include <sst_config.h>
#include "sst/core/serialization.h"

#include "sst/core/element.h"
#include "sst/elements/SS_router/SS_router/SS_router.h"

using namespace SST;
using namespace SST::SS_router;

static Component* 
create_router(SST::ComponentId_t id, 
                SST::Params& params)
{
    return new SST::SS_router::SS_router( id, params );
}


static const ElementInfoComponent components[] = {
    { "SS_router",
      "Cycle-accurate 3D torus network router",
      NULL,
      create_router
    },
    { NULL, NULL, NULL, NULL }
};

extern "C" {
    ElementLibraryInfo SS_router_eli = {
        "SS_router",
        "Cycle-accurate 3D torus network router",
        components,
    };
}

