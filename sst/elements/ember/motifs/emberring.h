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


#ifndef _H_EMBER_RING
#define _H_EMBER_RING

#include <sst/core/params.h>
#include "embergen.h"

namespace SST {
namespace Ember {

class EmberRingGenerator : public EmberGenerator {

public:
	EmberRingGenerator(SST::Component* owner, Params& params);
	void configureEnvironment(const SST::Output* output, uint32_t rank, uint32_t worldSize);
        void generate(const SST::Output* output, const uint32_t phase, std::queue<EmberEvent*>* evQ);
	void finish(const SST::Output* output) { }

private:
    uint32_t size;
	uint32_t rank;
	uint32_t messageSize;
	uint32_t iterations;

};

}
}

#endif
