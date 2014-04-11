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
 * File:   coherenceControllers.h
 * Author: Caesar De la Paz III
 * Email:  caesar.sst@gmail.com
 */


#ifndef COHERENCECONTROLLERS_H
#define COHERENCECONTROLLERS_H

#include "assert.h"
#include "sst/core/link.h"
#include <bitset>
#include <cstring>
#include "cacheArray.h"
#include "sst/core/output.h"
#include "cacheListener.h"
#include <boost/assert.hpp>
#include <queue>
#include "memNIC.h"

using namespace std;
using namespace SST::MemHierarchy;
using namespace SST::Interfaces;

namespace SST { namespace MemHierarchy {

class Cache;

class CoherencyController{
public:
    typedef CacheArray::CacheLine CacheLine;
    typedef unsigned int uint;
    typedef long long unsigned int uint64;

    struct response {
        MemEvent* event;
        uint64_t deliveryTime;
        bool cpuResponse;
    };
    
    MemNIC*    directoryLink_;
    uint64_t   timestamp_;
    uint64_t   accessLatency_;
    virtual void sendOutgoingCommands() = 0;
    
protected:
    CoherencyController(const Cache* _cache, Output* _dbg, uint _lineSize): timestamp_(0), accessLatency_(1),
                        owner_(_cache), d_(_dbg), lineSize_(_lineSize), sentEvents_(0){}
    ~CoherencyController(){}
    const Cache* owner_;
    Output*    d_;
    uint       lineSize_;
    int        sentEvents_;
    bool       L1_;
    queue<response> outgoingEventQueue_;
};




}}

#endif	/* COHERENCECONTROLLERS_H */
