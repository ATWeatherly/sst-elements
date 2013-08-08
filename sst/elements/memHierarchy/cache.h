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

#ifndef SST_MEMHIERARCHY_CACHE_H
#define SST_MEMHIERARCHY_CACHE_H

#include <deque>
#include <map>
#include <list>

#include <sst/core/event.h>
#include <sst/core/sst_types.h>
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/timeConverter.h>
#include <sst/core/output.h>

#include <sst/core/interfaces/memEvent.h>

#include "memNIC.h"
#include "bus.h"


using namespace SST::Interfaces;

namespace SST {
namespace MemHierarchy {

class CacheListener;

class Cache : public SST::Component {

private:
	typedef enum {DOWNSTREAM, SNOOP, DIRECTORY, UPSTREAM, SELF, PREFETCHER} SourceType_t;
	typedef enum {INCLUSIVE, EXCLUSIVE, STANDARD} CacheMode_t;
    typedef enum {SEND_DOWN, SEND_UP, SEND_BOTH} ForwardDir_t;

	class Request;
	class CacheRow;
	class CacheBlock;
	class SelfEvent;
	struct LoadInfo_t;
	typedef std::map<Addr, LoadInfo_t*> LoadList_t;

    struct SourceInfo_t {
        SourceType_t type;
        LinkId_t id; // -1 is a WildCard

        inline bool operator!=(const SourceInfo_t &o) const
        {
            return !(*this == o);
        }

        inline bool operator==(const SourceInfo_t &o) const
        {
            if ( type == o.type ) {
                if ( id == -1 || o.id == -1 ) return true;
                return (id == o.id);
            }
            return false;
        }
        inline bool operator<(const SourceInfo_t &o) const
        {

            if ( type == o.type ) {
                if ( id == -1 || o.id == -1 ) return false; // They're never less
                return (id < o.id);
            }
            return (type < o.type);
        }
    };

	class CacheBlock {
	public:
		/* Flags */
		typedef enum {
            INVALID,        // Nothing is here
            ASSIGNED,       // Reserved status, waiting on a load
            SHARED,         // Others may have an up to date copy
            EXCLUSIVE,      // Only in this cache, data dirty
            DIRTY_UPSTREAM, // for Inclusive mode, data not here, dirty/exclusive upstream
            DIRTY_PRESENT,  // for Inclusive mode, data here, modified
        } BlockStatus;

		Addr tag;
		Addr baseAddr;
		SimTime_t last_touched;
		BlockStatus status;
		Cache *cache;
		std::vector<uint8_t> data;
		uint32_t locked;

        LoadInfo_t *loadInfo;
		uint16_t row, col;
        bool wb_in_progress;
        bool user_lock_needs_wb;
        bool user_lock_sent_delayed;
        uint16_t user_locked;

		CacheBlock() {}
		CacheBlock(Cache *_cache) :
            tag(0), baseAddr(0), last_touched(0), status(INVALID),
            cache(_cache), data(std::vector<uint8_t>(cache->blocksize)),
            locked(0), loadInfo(NULL), wb_in_progress(false),
            user_lock_needs_wb(false), user_lock_sent_delayed(false),
            user_locked(0)
		{ }

		~CacheBlock()
		{ }

		void activate(Addr addr)
		{
			assert(ASSIGNED != status);
			assert(0 == locked);
			tag = cache->addrToTag(addr);
			baseAddr = cache->addrToBlockAddr(addr);
            cache->dbg.output(CALL_INFO, "CacheBlock:  %s: Activating block (%u, %u) for Address 1x%"PRIx64".\t"
					"baseAddr: 0x%"PRIx64"  Tag: 0x%"PRIx64"\n", cache->getName().c_str(), row, col, addr, baseAddr, tag);
			status = ASSIGNED;
		}

		bool isValid(void) const { return (INVALID != status && ASSIGNED != status); }
		bool isInvalid(void) const { return (INVALID == status); }
		bool isAssigned(void) const { return (ASSIGNED == status); }
        bool isDirty(void) const { return (DIRTY_UPSTREAM == status || DIRTY_PRESENT == status || EXCLUSIVE == status); }

        void lock() {
            cache->dbg.output(CALL_INFO, "CacheBlock:  Locking block %p [0x%"PRIx64"] (%u, %u) {%d -> %d}\n", this, baseAddr, row, col, locked, locked+1);
            locked++;
        }
        void unlock() {
            cache->dbg.output(CALL_INFO, "CacheBlock:  UNLocking block %p [0x%"PRIx64"] (%u, %u) {%d -> %d}\n", this, baseAddr, row, col, locked, locked-1);
            assert(locked);
            locked--;
        }
        bool isLocked() const {
            return locked > 0;
        }
	};

	class CacheRow {
	public:
		std::vector<CacheBlock> blocks;
        typedef std::deque<std::pair<MemEvent*, SourceInfo_t> > eventQueue_t;
        std::map<Addr, eventQueue_t> waitingEvents;
		Cache *cache;

		CacheRow() {}
		CacheRow(Cache *_cache) : cache(_cache)
		{
			blocks = std::vector<CacheBlock>(cache->n_ways, CacheBlock(cache));
		}

		CacheBlock* getLRU(void);

        void addWaitingEvent(MemEvent *ev, SourceInfo_t src);

        void printRow(void);
	};


    struct Invalidation {
        int waitingACKs;
        std::deque<std::pair<MemEvent*, SourceInfo_t> > waitingEvents;
        MemEvent::id_type issuingEvent;
        MemEvent *busEvent; // Snoop Bus event (for cancelation)
        CacheBlock *block;  // Optional
        CacheBlock::BlockStatus newStatus; // Optional
        bool canCancel;

        Invalidation() :
            waitingACKs(0), issuingEvent(0,-1), busEvent(NULL), block(NULL),
            newStatus(CacheBlock::INVALID), canCancel(true)
        { }
    };


	typedef union {
		struct {
			CacheBlock *block;
			CacheBlock::BlockStatus newStatus;
			bool decrementLock;
		} writebackBlock;
		struct {
            MemEvent *initiatingEvent;
			CacheBlock *block;
			SourceInfo_t src;
            bool isFakeSupply;
		} supplyData;
		struct {
			LoadInfo_t *loadInfo;
		} loadBlock;
        struct {
            Invalidation *inv;
        } invalidation;
	} BusHandlerArgs;

	typedef void(Cache::*BusFinishHandlerFunc)(BusHandlerArgs &args);
    typedef void(Cache::*BusInitHandlerFunc)(BusHandlerArgs &args, MemEvent *ev);

    struct BusHandlers {
        BusInitHandlerFunc init;
        BusFinishHandlerFunc finish;
        BusHandlerArgs args;
        BusHandlers(void) :
            init(NULL), finish(NULL)
        { }
        BusHandlers(BusInitHandlerFunc bihf, BusFinishHandlerFunc bfhf, BusHandlerArgs & args) :
            init(bihf), finish(bfhf), args(args)
        { }
    };

	class BusQueue {
        Bus::key_t makeBusKey(MemEvent *ev)
        {
            return ev->getID();
        }

	public:
		BusQueue(void) :
			comp(NULL), link(NULL), numPeers(0)
		{ }

		BusQueue(Cache *comp, SST::Link *link) :
			comp(comp), link(link), numPeers(0)
		{ }


        void init(const std::string &infoStr);
		void setup(Cache *_comp, SST::Link *_link);

        int getNumPeers(void) const { return numPeers; }
		size_t size(void) const { return map.size(); /* Don't use queue.size() -> that's an O(n) vs O(1) tradeoff */ }
		bool empty(void) const { return map.empty(); }

		void request(MemEvent *event, BusHandlers handlers = BusHandlers());
		bool cancelRequest(MemEvent *event);
		void clearToSend(BusEvent *busEvent);

        void printStatus(Output &out);


	private:
		Cache *comp;
		SST::Link *link;
		std::list<MemEvent*> queue;
		std::map<MemEvent*, BusHandlers> map;
        int numPeers;

	};

	typedef void (Cache::*SelfEventHandler)(MemEvent*, CacheBlock*, SourceInfo_t);
	typedef void (Cache::*SelfEventHandler2)(LoadInfo_t*, Addr addr, CacheBlock*);
	class SelfEvent : public SST::Event {
	public:
		SelfEvent() {} // For serialization

		SelfEvent(Cache *cache, SelfEventHandler handler, MemEvent *event, CacheBlock *block,
				SourceInfo_t event_source = (SourceInfo_t){SELF, -1}) :
			cache(cache), handler(handler), handler2(NULL),
            event(event), block(block), event_source(event_source),
            li(NULL), addr(0)
		{ }

		SelfEvent(Cache *cache, SelfEventHandler2 handler, LoadInfo_t *li, Addr addr, CacheBlock *block) :
			cache(cache), handler(NULL), handler2(handler),
            event(NULL), block(block), event_source((SourceInfo_t){SELF,-1}),
            li(li), addr(addr)
		{ }


        void fire(void) {
            if ( handler ) {
                (cache->*(handler))(event, block, event_source);
            } else if ( handler2 ) {
                (cache->*(handler2))(li, addr, block);
            }
        }

        Cache *cache;
		SelfEventHandler handler;
		SelfEventHandler2 handler2;
		MemEvent *event;
		CacheBlock *block;
		SourceInfo_t event_source;
        LoadInfo_t *li;
        Addr addr;
	};

	struct LoadInfo_t {
		Addr addr;
		CacheBlock *targetBlock;
		MemEvent *busEvent;
        MemEvent::id_type initiatingEvent;
        MemEvent::id_type loadingEvent;
        bool uncached;
        bool satisfied;
        bool eventScheduled; // True if a self-event has been scheduled that will need this (finishLoadBlock, finishLoadBlockBus)
        bool nackRescheduled; // True if a NACK caused a reschedule, and we haven't yet reached finishLoadBlock
        ForwardDir_t loadDirection;
		struct LoadElement_t {
			MemEvent * ev;
			SourceInfo_t src;
			SimTime_t issueTime;
			LoadElement_t(MemEvent *ev, SourceInfo_t src, SimTime_t issueTime) :
				ev(ev), src(src), issueTime(issueTime)
			{ }
		};
		std::deque<LoadElement_t> list;
		LoadInfo_t() : addr(0), targetBlock(NULL), busEvent(NULL), uncached(false), satisfied(false), eventScheduled(false), nackRescheduled(false), loadDirection(SEND_BOTH) { }
		LoadInfo_t(Addr addr) : addr(addr), targetBlock(NULL), busEvent(NULL), uncached(false), satisfied(false), eventScheduled(false), nackRescheduled(false), loadDirection(SEND_BOTH) { }
	};

	struct SupplyInfo {
        MemEvent *initiatingEvent;
		MemEvent *busEvent;
		bool canceled;
		SupplyInfo(MemEvent *id) : initiatingEvent(id), busEvent(NULL), canceled(false) { }
	};
	// Map from <addr, from where req came> to SupplyInfo
	typedef std::multimap<std::pair<Addr, SourceInfo_t>, SupplyInfo> supplyMap_t;

    class LatencyStats {
    private:
        uint64_t numPkts;
        SimTime_t minLat;
        SimTime_t maxLat;
        double m_n, m_old, s_n, s_old;
    public:
        LatencyStats() : numPkts(0), minLat(0), maxLat(0), m_n(0.0), m_old(0.0), s_n(0.0), s_old(0.0)
        { }
        void insertLatency(SimTime_t lat);
        uint64_t getNumPkts(void) const { return numPkts; }
        SimTime_t getMinLatency(void) const { return minLat; }
        SimTime_t getMaxLatency(void) const { return maxLat; }
        double getMeanLatency(void) const { return m_n; }
        double getVarianceLatency(void) const { return (m_n>1.0) ? (s_n/(m_n-1.0)) : 0.0; }
        double getStdDevLatency(void) const { return sqrt(getVarianceLatency()); }
    };


public:

	Cache(SST::ComponentId_t id, SST::Component::Params_t& params);
    bool clockTick(Cycle_t);
	virtual void init(unsigned int);
	virtual void setup();
	virtual void finish();
    virtual void printStatus(Output &out);

private:
    void handleBusEvent(SST::Event *event);
	void handleIncomingEvent(SST::Event *event, SourceType_t src);
	void handleIncomingEvent(SST::Event *event, SourceInfo_t src, bool firstTimeProcessed, bool firstPhaseComplete = false);
	void handleSelfEvent(SST::Event *event);
	void retryEvent(MemEvent *ev, CacheBlock *block, SourceInfo_t src);

    void handlePrefetchEvent(SST::Event *event);

	void handleCPURequest(MemEvent *ev, SourceInfo_t src, bool firstProcess);
	MemEvent* makeCPUResponse(MemEvent *ev, CacheBlock *block, SourceInfo_t src);
	void sendCPUResponse(MemEvent *ev, CacheBlock *block, SourceInfo_t src);

	void issueInvalidate(MemEvent *ev, SourceInfo_t src, CacheBlock *block, CacheBlock::BlockStatus newStatus, ForwardDir_t direction, bool cancelable = true);
	void issueInvalidate(MemEvent *ev, SourceInfo_t src, Addr addr, ForwardDir_t direction, bool cancelable = true);
    void finishBusSendInvalidation(BusHandlerArgs &args);
	void finishIssueInvalidate(Addr addr);


	void loadBlock(MemEvent *ev, SourceInfo_t src);
    std::pair<LoadInfo_t*, bool> initLoad(Addr addr, MemEvent *ev, SourceInfo_t src);
	void finishLoadBlock(LoadInfo_t *li, Addr addr, CacheBlock *block);
	void finishLoadBlockBus(BusHandlerArgs &args);

	void handleCacheRequestEvent(MemEvent *ev, SourceInfo_t src, bool firstProcess);
	void supplyData(MemEvent *ev, CacheBlock *block, SourceInfo_t src);
    void prepBusSupplyData(BusHandlerArgs &args, MemEvent *ev);
	void finishBusSupplyData(BusHandlerArgs &args);
	void handleCacheSupplyEvent(MemEvent *ev, SourceInfo_t src);
	void handleInvalidate(MemEvent *ev, SourceInfo_t src, bool finishedUpstream);
    void sendInvalidateACK(MemEvent *ev, SourceInfo_t src);

	bool waitingForInvalidate(Addr addr);
	bool cancelInvalidate(CacheBlock *block);
    void ackInvalidate(MemEvent *ev);

	void writebackBlock(CacheBlock *block, CacheBlock::BlockStatus newStatus);
    void prepWritebackBlock(BusHandlerArgs &args, MemEvent *ev);
	void finishWritebackBlockVA(BusHandlerArgs &args);
	void finishWritebackBlock(CacheBlock *block, CacheBlock::BlockStatus newStatus, bool decrementLock);

    void handleFetch(MemEvent *ev, SourceInfo_t src, bool invalidate, bool hasInvalidated);
    void fetchBlock(MemEvent *ev, CacheBlock *block, SourceInfo_t src);

    void handleNACK(MemEvent *ev, SourceInfo_t src);
    void respondNACK(MemEvent *ev, SourceInfo_t src);

    void handleUncachedWrite(MemEvent *ev, SourceInfo_t src);
    void handleWriteResp(MemEvent *ev, SourceInfo_t src);


    void handlePendingEvents(CacheRow *row, CacheBlock *block);
	void updateBlock(MemEvent *ev, CacheBlock *block);
	int numBits(int x);
	Addr addrToTag(Addr addr);
	Addr addrToBlockAddr(Addr addr);
	CacheBlock* findBlock(Addr addr, bool emptyOK = false);
	CacheRow* findRow(Addr addr);

    bool supplyInProgress(Addr addr, SourceInfo_t src);
    supplyMap_t::iterator getSupplyInProgress(Addr addr, SourceInfo_t src);
    supplyMap_t::iterator getSupplyInProgress(Addr addr, SourceInfo_t src, MemEvent::id_type id);

    std::string findTargetDirectory(Addr addr);

	void printCache(Output &out);

    void registerNewCPURequest(MemEvent::id_type id);
    void clearCPURequest(MemEvent::id_type id);

    Output dbg;
    Output::output_location_t statsOutputTarget;
    CacheListener* listener;
	int n_ways;
	int n_rows;
	uint32_t blocksize;
	std::string access_time;
	std::vector<CacheRow> database;
	std::string next_level_name;
    CacheMode_t cacheMode;
    bool isL1;

	int rowshift;
	unsigned rowmask;
	int tagshift;

    std::map<Addr, Invalidation> invalidations;
	LoadList_t waitingLoads;
	supplyMap_t suppliesInProgress;
    std::map<MemEvent::id_type, std::pair<MemEvent*, SourceInfo_t> > outstandingWrites;

	BusQueue snoopBusQueue;

    std::map<MemEvent::id_type, SimTime_t> responseTimes;
    SimTime_t maxResponseTimeAllowed;

	int n_upstream;
	SST::Link *snoop_link; // Points to a snoopy bus, or snoopy network (if any)
	MemNIC *directory_link; // Points to a network for directory lookups (if any)
	SST::Link **upstream_links; // Points to directly upstream caches or cpus (if any) [no snooping]
	SST::Link *downstream_link; // Points to directly downstream cache (if any)
	SST::Link *self_link; // Used for scheduling access
	std::map<LinkId_t, int> upstreamLinkMap;
    std::vector<MemNIC::ComponentInfo> directories;

	/* Stats */
	uint64_t num_read_hit;
	uint64_t num_read_miss;
	uint64_t num_supply_hit;
	uint64_t num_supply_miss;
	uint64_t num_write_hit;
	uint64_t num_write_miss;
	uint64_t num_upgrade_miss;
    uint64_t num_invalidates;
    LatencyStats latStats;


};

}
}

#endif  /* SST_MEMHIERARCHY_CACHE_H */
