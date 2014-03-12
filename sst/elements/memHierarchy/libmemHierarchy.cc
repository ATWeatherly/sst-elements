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

#include <sst_config.h>

#include "sst/core/serialization.h"
#include "sst/core/element.h"
#include "sst/core/component.h"

#include "cacheController.h"
#include "bus.h"
#include "trivialCPU.h"
#include "streamCPU.h"
#include "memoryController.h"
#include "directoryController.h"
#include "dmaEngine.h"

using namespace SST;
using namespace SST::MemHierarchy;


static const char * memEvent_port_events[] = {"interfaces.MemEvent", NULL};
static const char * bus_port_events[] = {"memHierarchy.BusEvent", NULL};
static const char * net_port_events[] = {"memHierarchy.MemRtrEvent", NULL};



static Component* create_Cache(ComponentId_t id, Params& params)
{
	return Cache::cacheFactory(id, params);
}

static const ElementInfoParam cache_params[] = {
    {"cache_frequency",         "Cache Frequency.  Usually the same as the CPU's frequency"},
    {"cache_size",              "Size in bytes.  Eg.  4KB or 1MB "},
    {"associativity",           "Specifies the cache associativity. In set associative caches, this is the number of ways."},
    {"replacement_policy",      "Replacement policy of the cache array.  Options:  LRU, LFU, Random, or MRU. "},
    {"cache_line_size",         "Size of a cache block in bytes."},
    {"low_network_links",       "Number lower level caches are connected to this cache. This is usually the number of banks in the next level cache (closer to the main memory)."},
    {"high_network_links",      "Number higher level caches are connected to this cache (closer to the CPU)."},
    {"access_latency_cycles",   "Access Latency (in Cycles) taken to lookup data in the cache."},
    {"coherence_protocol",      "Coherence protocol.  Supported: MESI (default), MSI"},
    {"mshr_num_entries",        "Number of entries in the MSHR"},
    {"debug",                   "0 (default): No debugging, 1: STDOUT, 2: STDERR, 3: FILE.", "0"},
    {"prefetcher",              "Prefetcher Module:  0, 1", "0"},
    {"L1",                      "Specify whether cache is L1:  0, 1"},
    {"directory_at_next_level", "Specify if there is a flat directory-controller as the higher level memory: 0, 1"},
    {"statistics",              "Print cache stats at end of simulation: 0, 1", "0"},
    {"network_address",         "When using a directory controller, the network address of this cache.", ""},
	{"network_num_vc",          "When using a directory controller, the number of VCS on the on-chip network.", "3"},
    {NULL, NULL, NULL}
};

static const ElementInfoPort cache_ports[] = {
    {"low_network_%d",  "Ports connected to lower level caches (closer to main memory)", memEvent_port_events},
    {"high_network_%d", "Ports connected to higher level caches (closer to CPU)", memEvent_port_events},
    {"directory",       "Network link port", net_port_events},
    {NULL, NULL, NULL}
};




static Component* create_Bus(ComponentId_t id, Params& params)
{
	return new Bus( id, params );
}

static const ElementInfoParam bus_params[] = {
    {"numPorts",        "Number of Ports on the bus."},
    {"busDelay",        "Delay time for the bus.", "100ns"},
    {"atomicDelivery",  "0 (default) or 1.  If true, delivery to this bus is atomic to ALL members of a coherency strategy.", "0"},
    {"debug",           "0 (default): No debugging, 1: STDOUT, 2: STDERR, 3: FILE.", "0"},
    {NULL, NULL}
};


static const ElementInfoPort bus_ports[] = {
    {"port%d",          "Ports, range from 0 to numPorts-1.", bus_port_events},
    {NULL, NULL, NULL}
};


static Component* create_trivialCPU(ComponentId_t id, Params& params)
{
	return new trivialCPU( id, params );
}


static const ElementInfoParam cpu_params[] = {
    {"verbose",             "Determine how verbose the output from the CPU is", "1"},
    {"workPerCycle",        "How much work to do per cycle."},
    {"commFreq",            "How often to do a memory operation."},
    {"memSize",             "Size of physical memory."},
    {"do_write",            "Enable writes to memory (versus just reads).", "1"},
    {"num_loadstore",       "Stop after this many reads and writes.", "-1"},
    {"uncachedRangeStart",  "Beginning of range of addresses that are uncacheable.", "0x0"},
    {"uncachedRangeEnd",    "End of range of addresses that are uncacheable.", "0x0"},
    {NULL, NULL, NULL}
};


static Component* create_streamCPU(ComponentId_t id, Params& params)
{
	return new streamCPU( id, params );
}



static Component* create_MemController(ComponentId_t id, Params& params)
{
	return new MemController( id, params );
}

static const ElementInfoParam memctrl_params[] = {
    {"mem_size",        "Size of physical memory in MB", "0"},
    {"range_start",     "Address Range where physical memory begins", "0"},
    {"interleave_size", "Size of interleaved pages in KB.", "0"},
    {"interleave_step", "Distance between sucessive interleaved pages on this controller in KB.", "0"},
    {"memory_file",     "Optional backing-store file to pre-load memory, or store resulting state", "N/A"},
    {"clock",           "Clock frequency of controller", ""},
    {"divert_DC_lookups",  "Divert Directory controller table lookups from the memory system, use a fixed latency (access_time). Default:0", "0"},
    {"backend",         "Timing backend to use:  Default to simpleMem", "memHierarchy.simpleMem"},
    {"request_width",   "Size of a DRAM request in bytes.  Should be a power of 2 - default 64", "64"},
    {"direct_link_latency",   "Latency when using the 'direct_link', rather than 'snoop_link'", "10 ns"},
    {"debug",           "0 (default): No debugging, 1: STDOUT, 2: STDERR, 3: FILE.", "0"},
    {"statistics",      "0 (default): Don't print, 1: STDOUT, 2: STDERR, 3: FILE.", "0"},
    {"trace_file",       "File name (optional) of a trace-file to generate.", ""},
    {"coherence_protocol",      "Coherence protocol.  Supported: MESI (default), MSI"},
    {NULL, NULL, NULL}
};


static const ElementInfoPort memctrl_ports[] = {
    {"snoop_link",      "Connect to a memHiearchy.bus", bus_port_events},
    {"direct_link",     "Directly connect to another component (like a Directory Controller).", memEvent_port_events},
    {"cube_link",       "Link to VaultSim.", NULL}, /* TODO:  Make this generic */
    {NULL, NULL, NULL}
};


static Module* create_Mem_SimpleSim(Component* comp, Params& params)
{
    return new SimpleMemory(comp, params);
}

static const ElementInfoParam simpleMem_params[] = {
    {"access_time",     "When not using DRAMSim, latency of memory operation.", "100 ns"},
    {NULL, NULL}
};


#if defined(HAVE_LIBDRAMSIM)
static Module* create_Mem_DRAMSim(Component* comp, Params& params)
{
    return new DRAMSimMemory(comp, params);
}


static const ElementInfoParam dramsimMem_params[] = {
    {"device_ini",      "Name of DRAMSim Device config file", NULL},
    {"system_ini",      "Name of DRAMSim Device system file", NULL},
    {NULL, NULL, NULL}
};

#endif

#if defined(HAVE_LIBHYBRIDSIM)
static Module* create_Mem_HybridSim(Component* comp, Params& params)
{
    return new HybridSimMemory(comp, params);
}


static const ElementInfoParam hybridsimMem_params[] = {
    {"device_ini",      "Name of HybridSim Device config file", NULL},
    {"system_ini",      "Name of HybridSim Device system file", NULL},
    {NULL, NULL, NULL}
};

#endif

static Module* create_Mem_VaultSim(Component* comp, Params& params)
{
    return new VaultSimMemory(comp, params);
}

static const ElementInfoParam vaultsimMem_params[] = {
    {"access_time",     "When not using DRAMSim, latency of memory operation.", "100 ns"},
    {NULL, NULL, NULL}
};




static Component* create_DirectoryController(ComponentId_t id, Params& params)
{
	return new DirectoryController( id, params );
}

static const ElementInfoParam dirctrl_params[] = {
    {"network_address"      "Network address of component.", NULL},
    {"network_bw",          "Network link bandwidth.", NULL},
	{"network_num_vc",      "The number of VCS on the on-chip network.", "3"},
    {"addr_range_start",    "Start of Address Range, for this controller.", "0"},
    {"addr_range_end",      "End of Address Range, for this controller.", NULL},
    {"interleave_size",     "(optional) Size of interleaved pages in KB.", "0"},
    {"interleave_step",     "(optional) Distance between sucessive interleaved pages on this controller in KB.", "0"},
    {"clock",               "Clock rate of controller.", "1GHz"},
    {"entry_cache_size",    "Size (in # of entries) the controller will cache.", "0"},
    {"debug",               "0 (default): No debugging, 1: STDOUT, 2: STDERR, 3: FILE.", "0"},
    {"statistics",          "0 (default): Don't print, 1: STDOUT, 2: STDERR, 3: FILE.", "0"},
    {NULL, NULL, NULL}
};

static const ElementInfoPort dirctrl_ports[] = {
    {"memory",      "Link to Memory Controller", NULL},
    {"network",     "Network Link", NULL},
    {NULL, NULL, NULL}
};



static Component* create_DMAEngine(ComponentId_t id, Params& params)
{
	return new DMAEngine( id, params );
}

static const ElementInfoParam dmaengine_params[] = {
    {"debug",           "0 (default): No debugging, 1: STDOUT, 2: STDERR, 3: FILE.", "0"},
    {"clockRate",       "Clock Rate for processing DMAs.", "1GHz"},
    {"netAddr",         "Network address of component.", NULL},
	{"network_num_vc",  "The number of VCS on the on-chip network.", "3"},
    {"printStats",      "0 (default): Don't print, 1: STDOUT, 2: STDERR, 3: FILE.", "0"},
    {NULL, NULL, NULL}
};


static const ElementInfoPort dmaengine_ports[] = {
    {"netLink",     "Network Link",     net_port_events},
    {NULL, NULL, NULL}
};


static const ElementInfoModule modules[] = {
    {
        "simpleMem",
        "Simple constant-access time memory",
        NULL, /* Advanced help */
        NULL, /* ModuleAlloc */
        create_Mem_SimpleSim, /* Module Alloc w/ params */
        simpleMem_params
    },
#if defined(HAVE_LIBDRAMSIM)
    {
        "dramsim",
        "DRAMSim-driven memory timings",
        NULL, /* Advanced help */
        NULL, /* ModuleAlloc */
        create_Mem_DRAMSim, /* Module Alloc w/ params */
        dramsimMem_params
    },
#endif
#if defined(HAVE_LIBHYBRIDSIM)
    {
        "hybridsim",
        "HybridSim-driven memory timings",
        NULL, /* Advanced help */
        NULL, /* ModuleAlloc */
        create_Mem_HybridSim, /* Module Alloc w/ params */
        hybridsimMem_params
    },
#endif
    {
        "vaultsim",
        "VaultSim Memory timings",
        NULL, /* Advanced help */
        NULL, /* ModuleAlloc */
        create_Mem_VaultSim, /* Module Alloc w/ params */
        vaultsimMem_params
    },
    {NULL, NULL, NULL, NULL, NULL, NULL}
};


static const ElementInfoComponent components[] = {
	{ "Cache",
		"Cache Component",
		NULL,
        create_Cache,
        cache_params,
        cache_ports,
        COMPONENT_CATEGORY_MEMORY
	},
	{ "Bus",
		"Mem Hierarchy Bus Component",
		NULL,
		create_Bus,
        bus_params,
        bus_ports,
        COMPONENT_CATEGORY_MEMORY
	},
	{"MemController",
		"Memory Controller Component",
		NULL,
		create_MemController,
        memctrl_params,
        memctrl_ports,
        COMPONENT_CATEGORY_MEMORY
	},
	{"DirectoryController",
		"Coherencey Directory Controller Component",
		NULL,
		create_DirectoryController,
        dirctrl_params,
        dirctrl_ports,
        COMPONENT_CATEGORY_MEMORY
	},
	{"DMAEngine",
		"DMA Engine Component",
		NULL,
		create_DMAEngine,
        dmaengine_params,
        dmaengine_ports,
        COMPONENT_CATEGORY_MEMORY
	},
	{"trivialCPU",
		"Simple Demo CPU for testing",
		NULL,
		create_trivialCPU,
        cpu_params,
        NULL,
        COMPONENT_CATEGORY_PROCESSOR
	},
	{"streamCPU",
		"Simple Demo STREAM CPU for testing",
		NULL,
		create_streamCPU,
        cpu_params,
        NULL,
        COMPONENT_CATEGORY_PROCESSOR
	},
	{ NULL, NULL, NULL, NULL, NULL, NULL, 0}
};


extern "C" {
	ElementLibraryInfo memHierarchy_eli = {
		"memHierarchy",
		"Simple Memory Hierarchy",
		components,
        NULL, /* Events */
        NULL, /* Introspectors */
        modules,
	};
}

BOOST_CLASS_EXPORT(DMACommand)
