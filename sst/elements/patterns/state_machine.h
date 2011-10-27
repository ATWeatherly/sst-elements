//
// Copyright (c) 2011, IBM Corporation
// All rights reserved.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef _STATE_MACHINE_H
#define _STATE_MACHINE_H

#include <stdlib.h>	// For exit()
#include <stdint.h>	// For uint32_t
#include <assert.h>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
#include <sst/core/serialization/element.h>
#include "patterns.h"

const int SM_START_EVENT= 0;
const int SM_MAX_DATA_FIELDS= 2;

#define _sm_abort( name, fmt, args...)\
{\
    fprintf(stderr, "%s::%s():%d:ABORT: "fmt, #name, __FUNCTION__, __LINE__, ## args ); \
    exit(-1); \
}



// This object is the event that gets passed among state machines
// It contains the event ID (number) and some data for those
// state machines that need to pass parameters to each other
// FIXME: It would be nice if SM_MAX_DATA_FIELDS wasn't hard-coded
class state_event   {
    public:
	state_event(void)
	{
	    // Initialize things
	    payload_size= sizeof(packed_data);
	    payload= &packed_data;
	    for (int i= 0; i < SM_MAX_DATA_FIELDS; i++)   {
		packed_data.Fdata[i]= 0.0;
		packed_data.Idata[i]= 0;
	    }
	    restart= false;
	    event= -1;
	    packed_data.epoch= -1;
	}

	void set_Fdata(double F1, double F2= 0.0)   {
	    packed_data.Fdata[0]= F1;
	    packed_data.Fdata[1]= F2;
	}

	double get_Fdata(int pos= 0)   {
	    assert(pos < SM_MAX_DATA_FIELDS);
	    return packed_data.Fdata[pos];
	}

	void set_Idata(long long I1, long long I2= 0)   {
	    packed_data.Idata[0]= I1;
	    packed_data.Idata[1]= I2;
	}

	long long get_Idata(int pos= 0)   {
	    assert(pos < SM_MAX_DATA_FIELDS);
	    return packed_data.Idata[pos];
	}

	void set_epoch(int epoch)   {packed_data.epoch= epoch;}
	int get_epoch(void)   {return packed_data.epoch;}

	int payload_size;
	void *payload;
	int event;

	// For runtime debugging. Before returning from a SM call, we make sure the
	// event we are returning has this flag set.
	bool restart;

    private:
	// Some state machine specific data that travels in the event
	// Most state machines wont need this. Others may use it any way they wish
	typedef struct    {
	    double Fdata[SM_MAX_DATA_FIELDS];
	    long long Idata[SM_MAX_DATA_FIELDS];
	    int epoch;

	    friend class boost::serialization::access;
	    template<class Archive>
	    void serialize(Archive & ar, const unsigned int version)
	    {
		ar & BOOST_SERIALIZATION_NVP(Fdata);
		ar & BOOST_SERIALIZATION_NVP(Idata);
	    }
	} packed_data_t;

	packed_data_t packed_data;

        friend class boost::serialization::access;
        template<class Archive>
        void serialize(Archive & ar, const unsigned int version)
        {
	    ar & BOOST_SERIALIZATION_NVP(payload_size);
	    ar & BOOST_SERIALIZATION_NVP(payload);
	    ar & BOOST_SERIALIZATION_NVP(event);
	    ar & BOOST_SERIALIZATION_NVP(restart);
	    ar & BOOST_SERIALIZATION_NVP(packed_data);
        }
};



// This transfers to another state by sending oursleves an event.
// This will probably be seldom used. It is necessary when you
// want to go to the next state, but can't do any work there (yet)
// and would have to block.
// See if goto_state() works better for your needs.
#define state_transition(event, new_state)   {\
	    state= new_state;\
	    self_event_send(event, 0);\
	}

// This jumps (calls) directly to a function that handles a state
#define goto_state(func, new_state, trigger_event)   {\
	    state_event e; \
	    state= new_state;\
	    e.event= trigger_event; \
	    func(e);\
	}



class State_machine   {
    public:
	State_machine(const int rank) :
	    // my_rank is only needed for debug output
	    my_rank(rank)
	{
	}

        ~State_machine() {}


	// Each SM has some (or none) SM-specific data that gets sent with each
	// event. That data is stored here and can be updated with the state_event
	// object methods.
	state_event SM_data;


	uint32_t SM_create(void *obj, void (*handler)(void *obj, state_event event));
	void SM_call(int machineID, state_event start_event, state_event return_event);
	void SM_return(state_event return_event);
	uint32_t SM_current_tag(void);

	// Comm_pattern needs to call handle_state_events()
	friend class Comm_pattern;

    private:

#ifdef SERIALIZATION_WORKS_NOW
	State_machine();  // For serialization only
#endif  // SERIALIZATION_WORKS_NOW
	void handle_state_events(uint32_t tag, state_event event);
	void deliver_missed_events(void);

	typedef struct   {
	    void (*handler)(void *obj, state_event event);
	    void *obj;
	    uint32_t tag;
	    std::list <state_event>missed_events;

	    friend class boost::serialization::access;
	    template<class Archive>
	    void serialize(Archive & ar, const unsigned int version)
	    {
		ar & BOOST_SERIALIZATION_NVP(handler);
		ar & BOOST_SERIALIZATION_NVP(obj);
		ar & BOOST_SERIALIZATION_NVP(tag);
		ar & BOOST_SERIALIZATION_NVP(missed_events);
	    }
	} SM_t;

	std::vector <SM_t>SM;
	std::vector <int>SMstack;

	int currentSM;
	int lastSM;

	// For debugging
	const int my_rank;

        friend class boost::serialization::access;
        template<class Archive>
        void serialize(Archive & ar, const unsigned int version)
        {
	    // FIXME: I don't know why I can't serialize this....
	    ar & BOOST_SERIALIZATION_NVP(SM_data);
	    ar & BOOST_SERIALIZATION_NVP(SM);
	    ar & BOOST_SERIALIZATION_NVP(SMstack);
	    ar & BOOST_SERIALIZATION_NVP(currentSM);
	    ar & BOOST_SERIALIZATION_NVP(lastSM);
	    ar & BOOST_SERIALIZATION_NVP(my_rank);
        }

};

#endif // _STATE_MACHINE_H
