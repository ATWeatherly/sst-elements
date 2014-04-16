// -*- mode: c++ -*-

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


#ifndef COMPONENTS_MERLIN_ROUTER_H
#define COMPONENTS_MERLIN_ROUTER_H

#include <sst/core/component.h>
#include <sst/core/event.h>
#include <sst/core/link.h>
#include <sst/core/module.h>
#include <sst/core/timeConverter.h>

using namespace SST;

namespace SST {
namespace Merlin {

#define VERIFY_DECLOCKING 0
    
const int INIT_BROADCAST_ADDR = -1;

class TopologyEvent;
    
class Router : public Component {
private:
    bool requestNotifyOnEvent;

    Router() :
    	Component(),
    	requestNotifyOnEvent(false),
    	vcs_with_data(0)
    {}

protected:
    inline void setRequestNotifyOnEvent(bool state)
    { requestNotifyOnEvent = state; }

    int vcs_with_data;
    
public:

    Router(ComponentId_t id) :
	Component(id),
	requestNotifyOnEvent(false),
	vcs_with_data(0)
    {}

    virtual ~Router() {}
    
    inline bool getRequestNotifyOnEvent() { return requestNotifyOnEvent; }
   
    virtual void notifyEvent() {}

    inline void inc_vcs_with_data() { vcs_with_data++; }
    inline void dec_vcs_with_data() { vcs_with_data--; }
    inline int get_vcs_with_data() { return vcs_with_data; }

    virtual int const* getOutputBufferCredits() = 0;
    virtual void sendTopologyEvent(int port, TopologyEvent* ev) = 0;
    virtual void recvTopologyEvent(int port, TopologyEvent* ev) = 0;

};

#define MERLIN_ENABLE_TRACE


class BaseRtrEvent : public Event {

public:
    enum RtrEventType {CREDIT, PACKET, INTERNAL, TOPOLOGY};

    inline RtrEventType getType() const { return type; }

protected:
    BaseRtrEvent(RtrEventType type) :
	Event(),
	type(type)
    {}

private:
    RtrEventType type;

    friend class boost::serialization::access;
    template<class Archive>
    void
    serialize(Archive & ar, const unsigned int version )
    {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Event);
	ar & BOOST_SERIALIZATION_NVP(type);
    }

};
    

class RtrEvent : public BaseRtrEvent {

public:
    int dest;
    int src;
    int vc;
    int size_in_flits;

    enum TraceType {NONE, ROUTE, FULL};
    
    RtrEvent() :
        BaseRtrEvent(BaseRtrEvent::PACKET),
        trace(NONE), injectionTime(0)
    {}

    inline void setInjectionTime(SimTime_t time) {injectionTime = time;}
    inline void setTraceID(int id) {traceID = id;}
    inline void setTraceType(TraceType type) {trace = type;}
    virtual RtrEvent* clone(void) {
        return new RtrEvent(*this);
    }

    inline SimTime_t getInjectionTime(void) const { return injectionTime; }
    inline TraceType getTraceType() const {return trace;}
    inline int getTraceID() const {return traceID;}

    
private:
    TraceType trace;
    int traceID;
    SimTime_t injectionTime;

    friend class boost::serialization::access;
    template<class Archive>
    void
    serialize(Archive & ar, const unsigned int version )
    {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(BaseRtrEvent);
	ar & BOOST_SERIALIZATION_NVP(dest);
	ar & BOOST_SERIALIZATION_NVP(src);
	ar & BOOST_SERIALIZATION_NVP(vc);
	ar & BOOST_SERIALIZATION_NVP(size_in_flits);
	ar & BOOST_SERIALIZATION_NVP(trace);
	ar & BOOST_SERIALIZATION_NVP(traceID);
    }
    
};

class TopologyEvent : public BaseRtrEvent {
    // Allows Topology events to consume bandwidth.  If this is set to
    // zero, then no bandwidth is consumed.
    int size_in_flits;
    
    friend class boost::serialization::access;
    template<class Archive>
    void
    serialize(Archive & ar, const unsigned int version )
    {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(BaseRtrEvent);
	ar & BOOST_SERIALIZATION_NVP(size_in_flits);
    }
    
public:
    TopologyEvent(int size_in_flits) :
	BaseRtrEvent(BaseRtrEvent::TOPOLOGY),
	size_in_flits(size_in_flits)
    {}

    TopologyEvent() :
	BaseRtrEvent(BaseRtrEvent::TOPOLOGY),
	size_in_flits(0)
    {}

    inline void setSizeInFlits(int size) { size_in_flits = size; }
    inline int getSizeInFlits() { return size_in_flits; }

};

class credit_event : public BaseRtrEvent {
public:
    int vc;
    int credits;

    credit_event(int vc, int credits) :
	BaseRtrEvent(BaseRtrEvent::CREDIT),
	vc(vc),
	credits(credits)
    {}

private:
    credit_event() :
	BaseRtrEvent(BaseRtrEvent::CREDIT)
    {}

	friend class boost::serialization::access;
	template<class Archive>
	void
	serialize(Archive & ar, const unsigned int version )
	{
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(BaseRtrEvent);
		ar & BOOST_SERIALIZATION_NVP(vc);
		ar & BOOST_SERIALIZATION_NVP(credits);
	}
};


class internal_router_event : public BaseRtrEvent {
    int next_port;
    int next_vc;
    RtrEvent* encap_ev;

public:
    internal_router_event() :
	BaseRtrEvent(BaseRtrEvent::INTERNAL)
    {}
    internal_router_event(RtrEvent* ev) :
	BaseRtrEvent(BaseRtrEvent::INTERNAL)
    {encap_ev = ev;}

    virtual ~internal_router_event() {
	if ( encap_ev != NULL ) delete encap_ev;
    }

    virtual internal_router_event* clone(void)
    {
        return new internal_router_event(*this);
    };

    inline void setNextPort(int np) {next_port = np; return;}
    inline int getNextPort() {return next_port;}

    inline void setVC(int vc) {encap_ev->vc = vc; return;}
    inline int getVC() {return encap_ev->vc;}

    inline int getFlitCount() {return encap_ev->size_in_flits;}

    inline void setEncapsulatedEvent(RtrEvent* ev) {encap_ev = ev;}
    inline RtrEvent* getEncapsulatedEvent() {return encap_ev;}

    inline int getDest() {return encap_ev->dest;}
    inline int getSrc() {return encap_ev->src;}

    inline RtrEvent::TraceType getTraceType() {return encap_ev->getTraceType();}
    inline int getTraceID() {return encap_ev->getTraceID();}


private:
	friend class boost::serialization::access;
	template<class Archive>
	void
	serialize(Archive & ar, const unsigned int version )
	{
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(BaseRtrEvent);
		ar & BOOST_SERIALIZATION_NVP(next_port);
		ar & BOOST_SERIALIZATION_NVP(next_vc);
		ar & BOOST_SERIALIZATION_NVP(encap_ev);
	}
};

class Topology : public Module {
public:
    enum PortState {R2R, R2N, UNCONNECTED};
    Topology() {}
    virtual ~Topology() {}

    virtual void route(int port, int vc, internal_router_event* ev) = 0;
    virtual void reroute(int port, int vc, internal_router_event* ev)  { route(port,vc,ev); }
    virtual internal_router_event* process_input(RtrEvent* ev) = 0;
	
    // Returns whether the port is a router to router, router to nic, or unconnected
    virtual PortState getPortState(int port) const = 0;
    inline bool isHostPort(int port) const { return getPortState(port) == R2N; }

    // Methods used during init phase to route init messages
    virtual void routeInitData(int port, internal_router_event* ev, std::vector<int> &outPorts) = 0;
    virtual internal_router_event* process_InitData_input(RtrEvent* ev) = 0;

    // Sets the array that holds the credit values for all the output
    // buffers.  Format is:
    // For port=n, VC=x, location in array is n*num_vcs + x.

    // If topology does not need this information, then default
    // version will ignore it.  If topology needs the information, it
    // will need to overload function to store it.
    virtual void setOutputBufferCreditArray(int const* array) {};
	
    // When TopologyEvents arrive, they are sent directly to the
    // topology object for the router
    virtual void recvTopologyEvent(int port, TopologyEvent* ev) {};
    
};

class PortControl;

    class XbarArbitration : public Module {
public:
    XbarArbitration() {}
    virtual ~XbarArbitration() {}

#if VERIFY_DECLOCKING
    virtual void arbitrate(PortControl** ports, int* port_busy, int* out_port_busy, int* progress_vc, bool clocking) = 0;
#else
    virtual void arbitrate(PortControl** ports, int* port_busy, int* out_port_busy, int* progress_vc) = 0;
#endif
    virtual void setPorts(int num_ports, int num_vcs) = 0;
    virtual void reportSkippedCycles(Cycle_t cycles) {};
    virtual void dumpState(std::ostream& stream) {};
	
};

}
}

#endif // COMPONENTS_MERLIN_ROUTER_H
