// Copyright 2009-2017 Sandia Corporation. Under the terms
// of Contract DE-NA0003525 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2017, Sandia Corporation
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef _H_EMBER_ALL_PINGPONG_MOTIF
#define _H_EMBER_ALL_PINGPONG_MOTIF

#include "mpi/embermpigen.h"

namespace SST {
namespace Ember {

class EmberAllPingPongGenerator : public EmberMessagePassingGenerator {

public:
    SST_ELI_REGISTER_SUBCOMPONENT(
        EmberAllPingPongGenerator,
        "Ember",
        "AllPingPongMotif",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Performs a All Ping Pong Motif",
        "SST::Ember::EmberGenerator"
    )

    SST_ELI_DOCUMENT_PARAMS(
        {   "arg.messageSize",      "Sets the message size of the ping pong operation", "128"},
        {   "arg.iterations",       "Sets the number of ping pong operations to perform",   "1024"},
        {   "arg.computetime",      "Sets the time spent computing some values",        "1000"},
    )

    SST_ELI_DOCUMENT_STATISTICS(
        { "time-Init", "Time spent in Init event",          "ns",  0},
        { "time-Finalize", "Time spent in Finalize event",  "ns", 0},
        { "time-Rank", "Time spent in Rank event",          "ns", 0},
        { "time-Size", "Time spent in Size event",          "ns", 0},
        { "time-Send", "Time spent in Recv event",          "ns", 0},
        { "time-Recv", "Time spent in Recv event",          "ns", 0},
        { "time-Irecv", "Time spent in Irecv event",        "ns", 0},
        { "time-Isend", "Time spent in Isend event",        "ns", 0},
        { "time-Wait", "Time spent in Wait event",          "ns", 0},
        { "time-Waitall", "Time spent in Waitall event",    "ns", 0},
        { "time-Waitany", "Time spent in Waitany event",    "ns", 0},
        { "time-Compute", "Time spent in Compute event",    "ns", 0},
        { "time-Barrier", "Time spent in Barrier event",    "ns", 0},
        { "time-Alltoallv", "Time spent in Alltoallv event", "ns", 0},
        { "time-Alltoall", "Time spent in Alltoall event",  "ns", 0},
        { "time-Allreduce", "Time spent in Allreduce event", "ns", 0},
        { "time-Reduce", "Time spent in Reduce event",      "ns", 0},
        { "time-Bcast", "Time spent in Bcast event",        "ns", 0},
        { "time-Gettime", "Time spent in Gettime event",    "ns", 0},
        { "time-Commsplit", "Time spent in Commsplit event", "ns", 0},
        { "time-Commcreate", "Time spent in Commcreate event", "ns", 0},
    )

public:
	EmberAllPingPongGenerator(SST::Component* owner, Params& params);
    bool generate( std::queue<EmberEvent*>& evQ);

private:
	uint32_t m_loopIndex;
	uint32_t m_iterations;
	uint32_t m_messageSize;
	uint32_t m_computeTime;
    uint64_t m_startTime;
    uint64_t m_stopTime;

    MessageResponse m_resp;
    void*    m_sendBuf;
    void*    m_recvBuf;
};

}
}

#endif
