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

/* 
 * File:   cacheFactory.cc
 * Author: Branden Moore / Si Hammond
 */


#ifndef _H_MEMHIERARCHY_CACHE_LISTENER
#define _H_MEMHIERARCHY_CACHE_LISTENER

#include <sst/core/serialization/element.h>
#include <sst/core/simulation.h>
#include <sst/core/element.h>
#include <sst/core/event.h>
#include <sst/core/module.h>

using namespace SST;
using namespace SST::Interfaces;

namespace SST {
class Output;

namespace MemHierarchy {

class CacheListener : public Module {
public:
    enum NotifyAccessType {
        READ,
        WRITE
    };

    enum NotifyResultType {
        HIT,
        MISS
    };


    CacheListener() {}
    virtual ~CacheListener() {}

    virtual void printStats(Output &out) {}
    virtual void setOwningComponent(const SST::Component* owner) {}
    virtual void notifyAccess(NotifyAccessType notifyType, NotifyResultType notifyResType, Addr addr) { }
    virtual void registerResponseCallback(Event::HandlerBase *handler) { delete handler; }
};

}
}

#endif

