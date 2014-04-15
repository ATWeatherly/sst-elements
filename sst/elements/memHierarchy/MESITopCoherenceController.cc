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
 * File:   MESITopCoherenceController.cc
 * Author: Caesar De la Paz III
 * Email:  caesar.sst@gmail.com
 */

#include <sst_config.h>
#include <vector>
#include "coherenceControllers.h"
#include "MESITopCoherenceController.h"
using namespace SST;
using namespace SST::MemHierarchy;

/*-------------------------------------------------------------------------------------
 * Top Coherence Controller Implementation
 *-------------------------------------------------------------------------------------*/

bool TopCacheController::handleAccess(MemEvent* _event, CacheLine* _cacheLine){
    Command cmd           = _event->getCmd();
    vector<uint8_t>* data = _cacheLine->getData();
    BCC_MESIState state   = _cacheLine->getState();

    switch(cmd){
        case GetS:
            if(state == S || state == M || state == E)
                return sendResponse(_event, S, data);
            break;
        case GetX:
        case GetSEx:
            if(state == M) return sendResponse(_event, M, data);
            break;
        default:
            _abort(MemHierarchy::CacheController, "Wrong command type!");
    }
    return false;
}

bool MESITopCC::handleAccess(MemEvent* _event, CacheLine* _cacheLine){
    Command cmd = _event->getCmd();
    int id = lowNetworkNodeLookup(_event->getSrc());
    CCLine* ccLine = ccLines_[_cacheLine->index()];
    bool ret       = false;

    switch(cmd){
        case GetS:
            processGetSRequest(_event, _cacheLine, id, ret);
            break;
        case GetX:
        case GetSEx:
            processGetXRequest(_event, _cacheLine, id, ret);
            break;
        case PutS:
            processPutSRequest(ccLine, id, ret);
            break;
        case PutM:
        case PutE:
            processPutMRequest(ccLine, _cacheLine->getState(), id, ret);
            break;

        default:
            _abort(MemHierarchy::CacheController, "Wrong command type!");
    }

    return ret;
}

void MESITopCC::handleInvalidate(int _lineIndex, Command _cmd){
    CCLine* l = ccLines_[_lineIndex];
    if(l->isShareless()) return;
    
    if(l->exclusiveSharerExists()){
        sendInvalidates(_cmd, _lineIndex, false, "", true);
    }
    else{
        sendInvalidates(_cmd, _lineIndex, false, "", false);
        l->removeAllSharers();
    }
    return;
}

void MESITopCC::handleFetchInvalidate(CacheLine* _cacheLine, Command _cmd){
    CCLine* l = ccLines_[_cacheLine->index()];
    if(!l->exclusiveSharerExists() && l->numSharers() == 0) return;

    switch(_cmd){
        case FetchInvalidate:
            if(l->exclusiveSharerExists()) {
                assert(l->numSharers() == 1);
                sendInvalidates(Inv, _cacheLine->index(), false, "", true);
            }
            else if(l->numSharers() > 0){
                sendInvalidates(Inv, _cacheLine->index(), false, "", false);
                l->removeAllSharers();
            }else{
                _abort(MemHierarchy::CacheController, "Command not supported");
            }
            break;
        case FetchInvalidateX:
            assert(0);
        default:
            _abort(MemHierarchy::CacheController, "Command not supported");
    }
}

void MESITopCC::handleInvAck(MemEvent* _event, CCLine* _ccLine){
    //assert(_ccLine->getAckCount() > 0);
    int sharerId = lowNetworkNodeLookup(_event->getSrc());
    if(_ccLine->exclusiveSharerExists()) _ccLine->clearExclusiveSharer(sharerId);
    else if(_ccLine->isSharer(sharerId)) _ccLine->removeSharer(sharerId);
    _ccLine->decAckCount();
}

/* Function sends invalidates to lower level caches, removes sharers if needed.  
 * Currently it implements weak consistency, ie. invalidates to sharers do not need acknowledgment
 * Returns true if eviction requires a response from Child, and false if no response is expected */
bool MESITopCC::handleEviction(int lineIndex,  BCC_MESIState _state){
    if(_state == I) return false;
    bool waitForInvalidateAck = false;
    assert(!CacheArray::CacheLine::inTransition(_state));
    CCLine* ccLine = ccLines_[lineIndex];
    assert(ccLine->valid());
    
    if(ccLine->exclusiveSharerExists()){
        waitForInvalidateAck = true;
        assert(!ccLine->isShareless());
    }
    if(!ccLine->isShareless()){
        d_->debug(_L1_,"Stalling request: Eviction requires invalidation of lw lvl caches. St = %s, ExSharerFlag = %s \n", BccLineString[_state], waitForInvalidateAck ? "True" : "False");
        if(waitForInvalidateAck){
            sendInvalidates(Inv, lineIndex, true, "", true);
            return (ccLine->getState() != V);
        }
        else{
            assert(ccLine->exclusiveSharerExists() || _state != IM);
            assert(ccLine->getState() == V);
            sendInvalidates(Inv, lineIndex, true, "", false);
            ccLine->removeAllSharers();
        }
    }
    return false;
}


void MESITopCC::sendInvalidates(Command cmd, int lineIndex, bool eviction, string requestingNode, bool acksNeeded){
    CCLine* ccLine = ccLines_[lineIndex];
    assert(!ccLine->isShareless());  //no sharers for this address in the cache
    unsigned int sentInvalidates = 0;
    int requestingId = requestingNode.empty() ? -1 : lowNetworkNodeLookup(requestingNode);
    
    d_->debug(_L1_,"Number of Sharers: %u \n", ccLine->numSharers());

    MemEvent* invalidateEvent;
    for(map<string, int>::iterator sharer = lowNetworkNameMap_.begin(); sharer != lowNetworkNameMap_.end(); sharer++){
        int sharerId = sharer->second;
        if(requestingId == sharerId) continue;
        if(ccLine->isSharer(sharerId)){
            if(acksNeeded) ccLine->setState(Inv_A);
            sentInvalidates++;
            
            if(!eviction) InvReqsSent_++;
            else EvictionInvReqsSent_++;
            
            invalidateEvent = new MemEvent((Component*)owner_, ccLine->getBaseAddr(), cmd);
            d_->debug(_L1_,"Invalidate sent: %u (numSharers), Invalidating Addr: %"PRIx64", Dst: %s\n", ccLine->numSharers(), ccLine->getBaseAddr(),  sharer->first.c_str());
            invalidateEvent->setDst(sharer->first);
            response resp = {invalidateEvent, timestamp_ + accessLatency_, false};
            outgoingEventQueue_.push(resp);
        }
    }
}



/*---------------------------------------------------------------------------------------------------
 * Helper Functions
 *--------------------------------------------------------------------------------------------------*/



void MESITopCC::processGetSRequest(MemEvent* _event, CacheLine* _cacheLine, int _sharerId, bool& ret){
    vector<uint8_t>* data = _cacheLine->getData();
    BCC_MESIState state   = _cacheLine->getState();
    int lineIndex         = _cacheLine->index();
    CCLine* l             = ccLines_[_cacheLine->index()];

    /* Send Data in E state */
    if(protocol_ && l->isShareless() && (state == E || state == M)){
        l->setExclusiveSharer(_sharerId);
        ret = sendResponse(_event, E, data);
    }
    
    /* If exclusive sharer exists, downgrade it to S state */
    else if(l->exclusiveSharerExists()) {
        d_->debug(_L5_,"GetS Req: Exclusive sharer exists \n");
        assert(!l->isSharer(_sharerId));
        assert(l->numSharers() == 1);                      // TODO: l->setState(InvX_A);  //sendInvalidates(InvX, lineIndex);
        //l->setState(InvX_A);
        //sendInvalidates(InvX, lineIndex, false, -1, true);
        sendInvalidates(Inv, lineIndex, false, "", true);
    }
    /* Send Data in S state */
    else if(state == S || state == M || state == E){
        l->addSharer(_sharerId);
        ret = sendResponse(_event, S, data);
    }
    else{
        _abort(MemHierarchy::CacheController, "Unkwown state!");
    }
}

void MESITopCC::processGetXRequest(MemEvent* _event, CacheLine* _cacheLine, int _sharerId, bool& _ret){
    BCC_MESIState state   = _cacheLine->getState();
    int lineIndex         = _cacheLine->index();
    CCLine* ccLine        = ccLines_[lineIndex];

    /* Invalidate any exclusive sharers before responding to GetX request */
    if(ccLine->exclusiveSharerExists()){
        d_->debug(_L5_,"GetX Req: Exclusive sharer exists \n");
        assert(!ccLine->isSharer(_sharerId));
        sendInvalidates(Inv, lineIndex, false, _event->getSrc(), true);
        return;
    }
    /* Sharers exist */
    else if(ccLine->numSharers() > 0){
        d_->debug(_L5_,"GetX Req:  Sharers 'S' exists \n");
        sendInvalidates(Inv, lineIndex, false, _event->getSrc(), false);
        ccLine->removeAllSharers();   //Weak consistency model, no need to wait for InvAcks to proceed with request
    }
    
    if(state == E || state == M){
        ccLine->setExclusiveSharer(_sharerId);
        sendResponse(_event, M, _cacheLine->getData());
        _ret = true;
    }
}


void MESITopCC::processPutMRequest(CCLine* _ccLine, BCC_MESIState _state, int _sharerId, bool& _ret){
    _ret = true;
    assert(_state == M || _state == E);

    if(_ccLine->exclusiveSharerExists()) _ccLine->clearExclusiveSharer(_sharerId);
    else if(_ccLine->isSharer(_sharerId)) _ccLine->removeSharer(_sharerId);

    if(_ccLine->getState() == V) return;
    _ccLine->decAckCount();

    if(_ccLine->getState() == InvX_A){
        _ccLine->addSharer(_sharerId);  // M->S
        assert(_ccLine->numSharers() == 1);
     }

}

void MESITopCC::processPutSRequest(CCLine* _ccLine, int _sharerId, bool& _ret){
    _ret = true;
    if(_ccLine->isSharer(_sharerId)) _ccLine->removeSharer(_sharerId);
}

void MESITopCC::printStats(int _stats){
    Output* dbg = new Output();
    dbg->init("", 0, 0, (Output::output_location_t)_stats);
    dbg->output(C,"Invalidates sent (non-eviction): %u\n", InvReqsSent_);
    dbg->output(C,"Invalidates sent due to evictions: %u\n", EvictionInvReqsSent_);
}


//TODO: Fix/Refactor this mess!
bool TopCacheController::sendResponse(MemEvent *_event, BCC_MESIState _newState, std::vector<uint8_t>* _data){
    if(_event->isPrefetch()){ //|| _sharerId == -1){
         d_->debug(_WARNING_,"Warning: No Response sent! Thi event is a prefetch or sharerId in -1");
        return true;
    }
    MemEvent *responseEvent; Command cmd = _event->getCmd();
    Addr offset = 0, base = 0;
    switch(cmd){
        case GetS: case GetSEx: case GetX:
            assert(_data);
            if(L1_){
                base = (_event->getAddr()) & ~(lineSize_ - 1);
                offset = _event->getAddr() - base;
                responseEvent = _event->makeResponse((SST::Component*)owner_);
                responseEvent->setPayload(_event->getSize(), &_data->at(offset));
            }
            else responseEvent = _event->makeResponse((SST::Component*)owner_, *_data, _newState);
            responseEvent->setDst(_event->getSrc());
            break;
        default:
            _abort(CoherencyController, "Command not valid as a response. \n");
    }

    if(L1_ && (cmd == GetS || cmd == GetSEx)) printData(d_, "Response Data", _data, offset, (int)_event->getSize());
    else printData(d_, "Response Data", _data);
    
    d_->debug(_L1_,"Sending Response:  Addr = %"PRIx64",  Dst = %s, Size = %i, Granted State = %s\n", _event->getAddr(), responseEvent->getDst().c_str(), responseEvent->getSize(), BccLineString[responseEvent->getGrantedState()]);
    uint64_t deliveryTime = _event->queryFlag(MemEvent::F_UNCACHED) ? timestamp_ : timestamp_ + accessLatency_;
    
    response resp = {responseEvent, deliveryTime, true};
    outgoingEventQueue_.push(resp);
    return true;
}

int MESITopCC::lowNetworkNodeLookup(const std::string &name){
	int id = -1;
	std::map<string, int>::iterator it = lowNetworkNameMap_.find(name);
	if(lowNetworkNameMap_.end() == it) {
        id = lowNetworkNodeCount_++;
		lowNetworkNameMap_[name] = id;
	} else {
		id = it->second;
	}
	return id;
}

