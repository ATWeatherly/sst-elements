/* 
 * File:   cacheFactory.cc
 * Author: Caesar De la Paz III
 * Email:  caesar.sst@gmail.com
 */


#include <boost/algorithm/string/predicate.hpp>
#include "hash.h"
#include "cacheController.h"
#include "util.h"
#include "cacheListener.h"
#include <sst/core/params.h>
#include <boost/lexical_cast.hpp>


namespace SST{ namespace MemHierarchy{
    using namespace SST::MemHierarchy;
    using namespace std;

Cache* Cache::cacheFactory(ComponentId_t id, Params& params){
    Output* dbg = new Output();
    
    int debugLevel = params.find_integer("debug_level", 0);
    if(debugLevel < 0 || debugLevel > 8)     _abort(Cache, "Debugging level must be betwee 0 and 8. \n");
    
    dbg->init("", debugLevel, 0,(Output::output_location_t)params.find_integer("debug", 0));
    dbg->debug(C,L1,0,"\n--------------------------- Initializing [Memory Hierarchy] --------------------------- \n\n");

    /* Get Parameters */
    string cacheFrequency       = params.find_string("cache_frequency", "" );            //Hertz
    string replacementProtocol  = params.find_string("replacement_policy", "lru");
    int associativity           = params.find_integer("associativity", -1);
    string sizeStr              = params.find_string("cache_size", "");                  //Bytes
    int lineSize                = params.find_integer("cache_line_size", -1);            //Bytes
    //int numParents              = params.find_integer("low_network_links", -1);
    //int numChildren             = params.find_integer("high_network_links", -1);
    int accessLatency           = params.find_integer("access_latency_cycles", -1);                 //ns
    int mshrSize                = params.find_integer("mshr_num_entries", -1);           //number of entries
    string preF                 = params.find_string("prefetcher");
    int L1int                   = params.find_integer("L1", 0);
    int directoryAtNextLevel    = params.find_integer("directory_at_next_level", 0);
    string coherenceProtocol    = params.find_string("coherence_protocol", "");

    /* Check user specified all required fields */
    if(cacheFrequency.empty())                _abort(Cache, "No cache frequency specified (usually frequency = cpu frequency).\n");
    if(-1 >= associativity)                   _abort(Cache, "Associativity was not specified.\n");
    if(sizeStr.empty())                       _abort(Cache, "Cache size was not specified. \n")
    if(-1 == lineSize)                        _abort(Cache, "Line size was not specified (blocksize).\n");
    if(mshrSize == -1)                        mshrSize = 4096;//_abort(Cache, "MSHR Size not specified correctly\n");
    if(L1int != 1 && L1int != 0)              _abort(Cache, "Not specified whether cache is L1 (0 or 1)\n");
    //if(L1int == 1) numChildren = 1;
    //if(numParents <= 0 || numChildren <= 0)   _abort(Cache, "Number of children and parents has each to be greater than zero (L1 have core as a parent). \n");
    if(accessLatency == -1 )                  _abort(Cache, "Access time not specified\n");
    if(directoryAtNextLevel > 1 ||
       directoryAtNextLevel < 0)              _abort(Cache, "Did not specified correctly where there exists a directory controller at higher level cache");
    long cacheSize = SST::MemHierarchy::convertToBytes(sizeStr);
    uint numLines = cacheSize/lineSize;
    uint protocol;
    
    //TODO:
    assert(lineSize == 64);  //Work in progress to allow different cache line sizes
    
    /* Initialization */
    HashFunction* ht = new PureIdHashFunction;
    
    if(coherenceProtocol == "MESI" || coherenceProtocol == "mesi") protocol = 1;
    else protocol = 0;
    
    SST::MemHierarchy::LRUReplacementMgr* replacementManager;
    if (boost::iequals(replacementProtocol, "lru"))         replacementManager = new LRUReplacementMgr(dbg, numLines,true);
    else if (boost::iequals(replacementProtocol, "lfu"))    replacementManager = new LRUReplacementMgr(dbg, numLines, true);
   // TODO: else if (boost::iequals(replacementProtocol, "random")) replacementManager = new RandomReplacementMgr(numLines, true);
    //else if (boost::iequals(replacementProtocol, "mru"))    replacementManager = new MRUReplacementMgr(numLines, true);
    else _abort(Cache, "Replacement policy was not entered correctly or is not supported.\n");
    
    bool L1 = (L1int == 1) ? true : false;
    CacheArray* array = new SetAssociativeArray(dbg, cacheSize, lineSize, associativity, replacementManager, ht, !L1);
    Cache* cache = new Cache(id, params, cacheFrequency, array, protocol, dbg,
                             replacementManager, numLines, lineSize, mshrSize, L1, directoryAtNextLevel);
    return cache;
}



Cache::Cache(ComponentId_t id, Params& params, string _cacheFrequency, CacheArray* _cacheArray, uint _protocol,
             Output* _dbg, LRUReplacementMgr* _rm, uint _numLines, uint _lineSize, uint _MSHRSize,
             bool _L1, bool _dirControllerExists) :
               Component(id), cArray_(_cacheArray), protocol_(_protocol), d_(_dbg), replacementMgr_(_rm),
               numLines_(_numLines), lineSize_(_lineSize), MSHRSize_(_MSHRSize), L1_(_L1),
               dirControllerExists_(_dirControllerExists){

    d_->debug(_INFO_,"--------------------------- Initializing [Cache]: %s... \n", this->Component::getName().c_str());
    pMembers();
    errorChecking();
    /* Prefetcher */

    stats_ = params.find_integer("statistics", 0);
    accessLatency_ = params.find_integer("access_latency_cycles", -1);
    string prefetcher = params.find_string("prefetcher");
    if (prefetcher.empty()) listener_ = new CacheListener();
    else {
        listener_ = dynamic_cast<CacheListener*>(loadModule(prefetcher, params));
        assert(listener_);
    }
    listener_->setOwningComponent(this);
    listener_->registerResponseCallback(new Event::Handler<Cache>(this, &Cache::handlePrefetchEvent));

    /* MSHR */
    mshr_ = new MSHR(this, MSHRSize_);
    mshrUncached_ = new MSHR(this, MSHRSize_);
    /* Links */
    registerTimeBase("2 ns", true);       //  TODO:  Is this right?
    lowNetPorts_ = new vector<Link*>();
    highNetPorts_ = new vector<Link*>();
    
    if (dirControllerExists_) {
        assert(isPortConnected("directory_link"));
        MemNIC::ComponentInfo myInfo;
        myInfo.link_port = "directory_link";
        myInfo.link_bandwidth = "2 ns"; // Time base as registered earlier
		myInfo.num_vcs = params.find_integer("network_num_vc", 3);
        myInfo.name = getName();
        myInfo.network_addr = params.find_integer("network_address");
        myInfo.type = MemNIC::TypeCache;
        myInfo.typeInfo.cache.blocksize = lineSize_;
        myInfo.typeInfo.cache.num_blocks = numLines_;

        directoryLink_ = new MemNIC(this, myInfo, new Event::Handler<Cache>(this, &Cache::processIncomingEvent));
    } else {
        directoryLink_ = NULL;
    }
    
    configureLinks();


    /* Coherence Controllers */
    sharersAware_ = (L1_) ? false : true;
    (!L1_) ? topCC_ = new MESITopCC(this, d_, protocol_, numLines_, lineSize_, accessLatency_, highNetPorts_) : topCC_ = new TopCacheController(this, d_, lineSize_, accessLatency_, highNetPorts_);
    bottomCC_ = new MESIBottomCC(this, this->getName(), d_, lowNetPorts_, listener_, lineSize_, accessLatency_, L1_, directoryLink_);
    /* Replacement Manager */
    replacementMgr_->setTopCC(topCC_);  replacementMgr_->setBottomCC(bottomCC_);
    
    registerClock(_cacheFrequency, new Clock::Handler<Cache>(this, &Cache::clockTick));
    timestamp_ = 0;
    STAT_GetSExReceived_ = 0, STAT_InvalidateWaitingForUserLock_ = 0, STAT_TotalInstructionsRecieved_ = 0;
}


void Cache::configureLinks(){
    char buf[200];
    char buf2[200];
    bool highNetExists = false;
    bool lowNetExists = false;
    sprintf(buf2, "High network port was not specified correctly on componenent %s.  Please name ports \'high_network_x' where x is the port number and starts at 0\n", this->getName().c_str());
    sprintf(buf, "Low network port was not specified correctly on component %s.  Please name ports \'low_network_x' where x is the port number and starts at 0\n", this->getName().c_str());
    if(!dirControllerExists_){
        for(uint id = 0 ; id < 200; id++) {
            string linkName = "low_network_" + boost::lexical_cast<std::string>(id);
            SST::Link* link = configureLink(linkName, "50ps", new Event::Handler<Cache>(this, &Cache::processIncomingEvent));
            if(link){
                d_->debug(_INFO_,"Low Netowork Link ID: %u \n", (uint)link->getId());
                lowNetPorts_->push_back(link);
                lowNetExists = true;
            }else break;
            
        }
    }

    for(uint id = 0 ; id < 200; id++) {
        string linkName = "high_network_" + boost::lexical_cast<std::string>(id);
        SST::Link* link = configureLink(linkName, "50ps", new Event::Handler<Cache>(this, &Cache::processIncomingEvent));  //TODO: fix
        if(link) {
            d_->debug(_INFO_,"High Network Link ID: %u \n", (uint)link->getId());
            highNetPorts_->push_back(link);
            highNetExists = true;
        }else break;
    }
    
    
    if(!dirControllerExists_) BOOST_ASSERT_MSG(lowNetExists, buf);
    BOOST_ASSERT_MSG(highNetExists,  buf2);
    selfLink_ = configureSelfLink("Self", "50ps", new Event::Handler<Cache>(this, &Cache::handleSelfEvent));
}

}}
