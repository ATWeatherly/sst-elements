/*
 * File:   coherenceControllers.cc
 * Author: Caesar De la Paz III
 * Email:  caesar.sst@gmail.com
 */


#include <sst_config.h>
#include <sst/core/serialization.h>

#include <sstream>

#include "bus.h"

#include <csignal>
#include <boost/variant.hpp>

#include <sst/core/params.h>
#include <sst/core/simulation.h>
#include <sst/core/interfaces/stringEvent.h>
#include <sst/core/interfaces/memEvent.h>

using namespace std;
using namespace SST;
using namespace SST::MemHierarchy;
using namespace SST::Interfaces;

const Bus::key_t Bus::ANY_KEY = std::pair<uint64_t, int>((uint64_t)-1, -1);


Bus::Bus(ComponentId_t id, Params& params) : Component(id){
	configureParameters(params);
    configureLinks();
}

Bus::Bus() : Component(-1) {}

void Bus::processIncomingEvent(SST::Event *ev){
    eventQueue_.push(ev);
}

bool Bus::clockTick(Cycle_t time) {

    if(!eventQueue_.empty()){
        SST::Event* event = eventQueue_.front();
        
        if(broadcast_) broadcastEvent(event);
        else sendSingleEvent(event);
        
        eventQueue_.pop();
    }
    
    return false;
}



void Bus::broadcastEvent(SST::Event *ev){
    MemEvent* memEvent = dynamic_cast<MemEvent*>(ev); assert(memEvent);
    LinkId_t srcLinkId = lookupNode(memEvent->getSrc());
    SST::Link* srcLink = linkIdMap_[srcLinkId];

    for(int i = 0; i < numHighNetPorts_; i++) {
        if(highNetPorts_[i] == srcLink) continue;
        highNetPorts_[i]->send(new MemEvent(memEvent));
    }
    
    for(int i = 0; i < numLowNetPorts_; i++) {
        if(lowNetPorts_[i] == srcLink) continue;
        lowNetPorts_[i]->send( new MemEvent(memEvent));
    }
    
    delete memEvent;
}



void Bus::sendSingleEvent(SST::Event *ev){
    MemEvent *event = static_cast<MemEvent*>(ev); assert(event);
    dbg_.debug(_L3_,"\n\n----------------------------------------------------------------------------------------\n");    //raise(SIGINT);
    dbg_.debug(_L3_,"Incoming Event. Name: %s, Cmd: %s, Addr: %"PRIx64", BsAddr: %"PRIx64", Src: %s, Dst: %s, LinkID: %i \n", this->getName().c_str(), CommandString[event->getCmd()], event->getAddr(), event->getBaseAddr(), event->getSrc().c_str(), event->getDst().c_str(), ev->getDeliveryLink()->getId());

    LinkId_t dstLinkId = lookupNode(event->getDst());
    SST::Link* dstLink = linkIdMap_[dstLinkId];
    dstLink->send(new MemEvent(event));
    
    delete event;
}

//------------------
// EXTRAS
//------------------

void Bus::mapNodeEntry(const std::string &name, LinkId_t id){
	std::map<std::string, LinkId_t>::iterator it = nameMap_.find(name);
	assert(nameMap_.end() == it);
    nameMap_[name] = id;
}

LinkId_t Bus::lookupNode(const std::string &name){
	std::map<std::string, LinkId_t>::iterator it = nameMap_.find(name);
	assert(nameMap_.end() != it);
    return it->second;
}

void Bus::configureLinks(){
    SST::Link* link;
	for ( int i = 0 ; i < maxNumPorts_ ; i++ ) {
		std::ostringstream linkName;
		linkName << "high_network_" << i;
		std::string ln = linkName.str();
		link = configureLink(ln, "50 ps", new Event::Handler<Bus>(this, &Bus::processIncomingEvent));
		if(link){
            highNetPorts_.push_back(link);
            numHighNetPorts_++;
            //assert(highNetPorts_[i]);
            linkIdMap_[highNetPorts_[i]->getId()] = highNetPorts_[i];
            dbg_.output(CALL_INFO, "Port %lu = Link %d\n", highNetPorts_[i]->getId(), i);
        }
    }
    
    for ( int i = 0 ; i < maxNumPorts_ ; i++ ) {
		std::ostringstream linkName;
		linkName << "low_network_" << i;
		std::string ln = linkName.str();
		link = configureLink(ln, "50 ps", new Event::Handler<Bus>(this, &Bus::processIncomingEvent));
        if(link){
            lowNetPorts_.push_back(link);
            numLowNetPorts_++;
            linkIdMap_[lowNetPorts_[i]->getId()] = lowNetPorts_[i];
            dbg_.output(CALL_INFO, "Port %lu = Link %d\n", lowNetPorts_[i]->getId(), i);
        }
	}
    if(numLowNetPorts_ < 1 || numHighNetPorts_ < 1) _abort(Bus,"couldn't find number of Ports (numPorts)\n");

}

void Bus::configureParameters(SST::Params& params){
    dbg_.init("" + getName() + ": ", 0, 0, (Output::output_location_t)params.find_integer("debug", 0));
    numLowNetPortsX_ = params.find_integer("low_network_ports", 0);
	numHighNetPortsX_ = params.find_integer("high_network_ports", 0);
    numHighNetPorts_ = 0;
    numLowNetPorts_ = 0;
    maxNumPorts_ = 500;
    
	latency_ = params.find_integer("bus_latency_cycles", 1);
    busFrequency_ = params.find_string("bus_frequency", "");
    broadcast_ = params.find_integer("broadcast", 0);
    fanout_ = params.find_integer("fanout", 0);

    if(busFrequency_ == "Invalid") _abort(Bus, "Bus Frequency was not specified\n");
    if(broadcast_ < 0 || broadcast_ > 1) _abort(Bus, "Broadcast feature was not specified correctly\n");
    registerClock(busFrequency_, new Clock::Handler<Bus>(this, &Bus::clockTick));
}

void Bus::init(unsigned int phase){
    SST::Event *ev;

    for(int i = 0; i < numHighNetPorts_; i++) {
        while ((ev = highNetPorts_[i]->recvInitData())){
            
            MemEvent* memEvent = dynamic_cast<MemEvent*>(ev);
            if(!memEvent) delete memEvent;
            else if(memEvent->getCmd() == NULLCMD){
                mapNodeEntry(memEvent->getSrc(), highNetPorts_[i]->getId());
            }
            else{
                for(int k = 0; k < numLowNetPorts_; k++)
                    lowNetPorts_[k]->sendInitData(memEvent);
            }
        }
    }
    
    MemEvent* temp;
    for(int i = 0; i < numLowNetPorts_; i++) {
        while ((ev = lowNetPorts_[i]->recvInitData())){
            MemEvent* memEvent = dynamic_cast<MemEvent*>(ev);
            if(!memEvent) delete memEvent;
            else if(memEvent->getCmd() == NULLCMD){
                mapNodeEntry(memEvent->getSrc(), lowNetPorts_[i]->getId());
                for(int i = 0; i < numHighNetPorts_; i++) {
                    temp = new MemEvent(memEvent);
                    highNetPorts_[i]->sendInitData(temp);
                    //cout << "sent memEvent, src: " << temp->getSrc() << " linkID: " << highNetPorts_[i]->getId() << endl;
                }
            }
            else{/*Ignore responses */}
        }
    }
}



// Element Libarary / Serialization stuff

//BOOST_CLASS_EXPORT(Bus)
//BOOST_CLASS_EXPORT(BusEvent)


