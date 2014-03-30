
#include "emberallpingpong.h"

using namespace SST::Ember;

EmberAllPingPongGenerator::EmberAllPingPongGenerator(SST::Component* owner, Params& params) :
	EmberGenerator(owner, params) {

	iterations = (uint32_t) params.find_integer("iterations", 1024);
	messageSize = (uint32_t) params.find_integer("messagesize", 128);
	computeTime = (uint32_t) params.find_integer("computetime", 1000);

	assert(messageSize > 0);
}

void EmberAllPingPongGenerator::configureEnvironment(const SST::Output* output, uint32_t rank, uint32_t worldSize) {
	halfWorld = worldSize / 2;
	assert(halfWorld > 2);

	if(rank < halfWorld) {
		commWithRank = rank + halfWorld;
	} else {
		commWithRank = rank - halfWorld;
	}

	myRank = rank;
}

void EmberAllPingPongGenerator::generate(const SST::Output* output, const uint32_t phase, std::queue<EmberEvent*>* evQ) {
	if(phase < iterations) {
	       EmberComputeEvent* compute = new EmberComputeEvent(computeTime);
               evQ->push(compute);

               EmberRecvEvent* recvEv = new EmberRecvEvent(commWithRank, messageSize, 0, (Communicator) 0);
               EmberSendEvent* sendEv = new EmberSendEvent(commWithRank, messageSize, 0, (Communicator) 0);

		if(myRank < halfWorld) {
                	evQ->push(sendEv);
                	evQ->push(recvEv);
		} else {
	                evQ->push(recvEv);
       			evQ->push(sendEv);
		}
	} else {
		EmberFinalizeEvent* finalize = new EmberFinalizeEvent();
                evQ->push(finalize);
	}
}
