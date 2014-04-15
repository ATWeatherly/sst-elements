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


#ifndef _H_EMBER_IRECV_EVENT
#define _H_EMBER_IRECV_EVENT

#include <sst/elements/hermes/msgapi.h>
#include "emberevent.h"

using namespace SST::Hermes;

namespace SST {
namespace Ember {

class EmberIRecvEvent : public EmberEvent {

public:
	EmberIRecvEvent(uint32_t rank, uint32_t recvSizeBytes, int tag, Communicator comm, MessageRequest* req);
	~EmberIRecvEvent();
	EmberEventType getEventType();
	uint32_t getRecvFromRank();
	uint32_t getMessageSize();
	MessageRequest* getMessageRequestHandle();
	int getTag();
	std::string getPrintableString();
	Communicator getCommunicator();

protected:
	MessageRequest* request;
	uint32_t recvSizeBytes;
	uint32_t rank;
	int recvTag;
	Communicator comm;

};

}
}

#endif
