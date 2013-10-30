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

#include "sst_config.h"
#include "Factory.h"

#include <fstream>
#include <sstream>
#include <stdio.h>

//#include <QFileSystemWatcher>

#include <sst/core/params.h>

#include "BestFitAllocator.h"
#include "ConstraintAllocator.h"
#include "EASYScheduler.h"
#include "FirstFitAllocator.h"
#include "GranularMBSAllocator.h"
#include "LinearAllocator.h"
#include "MBSAllocator.h"
#include "Machine.h"
#include "MachineMesh.h"
#include "NearestAllocator.h"
#include "OctetMBSAllocator.h"
#include "PQScheduler.h"
#include "RandomAllocator.h"
#include "RoundUpMBSAllocator.h"
#include "schedComponent.h"
#include "SimpleAllocator.h"
#include "SimpleMachine.h"
#include "SortedFreeListAllocator.h"
#include "StatefulScheduler.h"
#include "misc.h"

using namespace SST::Scheduler;
using namespace std;

/* 
 * Factory file helps parse the parameters in the sdl file
 * returns correct type of machine, allocator, and scheduler
 */

const Factory::schedTableEntry Factory::schedTable[] = {
    {PQUEUE, "pqueue"},
    {EASY, "easy"},
    {CONS, "cons"},
    {PRIORITIZE, "prioritize"},
    {DELAYED, "delayed"},
    {ELC, "elc"},
};

const Factory::machTableEntry Factory::machTable[] = {
    {SIMPLEMACH, "simple"},
    {MESH, "mesh"},
};

const Factory::allocTableEntry Factory::allocTable[] = {
    {SIMPLEALLOC, "simple"},
    {RANDOM, "random"},
    {NEAREST, "nearest"},
    {GENALG, "genalg"},
    {MM, "mm"},
    {MC1X1, "mc1x1"},
    {OLDMC1X1, "oldmc1x1"},
    {MBS, "mbs"},
    {GRANULARMBS, "granularmbs"},
    {OCTETMBS, "octetmbs"},
    {FIRSTFIT, "firstfit"},
    {BESTFIT, "bestfit"},
    {SORTEDFREELIST, "sortedfreelist"},
    {CONSTRAINT, "constraint"},
};

const Factory::FSTTableEntry Factory::FSTTable[] = {
    {NONE, "none"},
    {RELAXED, "relaxed"},
    {STRICT, "strict"},
};

const int Factory::numSchedTableEntries = 6;
const int Factory::numAllocTableEntries = 14;
const int Factory::numMachTableEntries = 2;
const int Factory::numFSTTableEntries = 3;

Factory::Factory() 
{
    schedout.init("", 8, 0, Output::STDOUT);
}

Scheduler* Factory::getScheduler(SST::Params& params, int numProcs)
{
    if(params.find("scheduler") == params.end()){
        schedout.verbose(CALL_INFO, 1, 0, "Defaulting to Priority Scheduler with FIFO queue\n");
        return new PQScheduler(PQScheduler::JobComparator::Make("fifo"));
    }
    else
    {
        int filltimes = 0;
        vector<string>* schedparams = parseparams(params["scheduler"]);
        if(schedparams->size() == 0)
            schedout.fatal(CALL_INFO, 1, 0, 0, "Error in parsing scheduler parameter");
        switch(schedulername(schedparams->at(0)))
        {
            //Priority Queue Scheduler
        case PQUEUE:
            schedout.debug(CALL_INFO, 4, 0, "Priority Queue Scheduler\n");
            if(schedparams->size() == 1)
                return new PQScheduler(PQScheduler::JobComparator::Make("fifo"));
            else
                return new PQScheduler(PQScheduler::JobComparator::Make(schedparams->at(1)));
            break;

            //EASY Scheduler
        case EASY:
            schedout.debug(CALL_INFO, 4, 0, "Easy Scheduler\n");
            if (schedparams -> size() == 1) {
                return new EASYScheduler(EASYScheduler::JobComparator::Make("fifo"));
            } else if (schedparams -> size() == 2) {
                EASYScheduler::JobComparator* comp = EASYScheduler::JobComparator::Make(schedparams->at(1));
                if (comp == NULL) {
                    schedout.fatal(CALL_INFO, 1, 0, 0, "Argument to Easy Scheduler parameter not found:%s", schedparams->at(1).c_str());
                }
                return new EASYScheduler(comp);
            } else {
                schedout.fatal(CALL_INFO, 1, 0, 0, "EASY Scheduler requires 1 or 0 parameters (determines type of queue or defaults to FIFO");
            }
            break;

            //Stateful Scheduler with Convervative Manager
        case CONS:
            schedout.debug(CALL_INFO, 4, 0, "Conservative Scheduler\n");
            if (schedparams -> size() == 1) {
                return new StatefulScheduler(numProcs, StatefulScheduler::JobComparator::Make("fifo"), true);
            } else {
                return new StatefulScheduler(numProcs, StatefulScheduler::JobComparator::Make(schedparams->at(1)), true);
            }
            break;

            //Stateful Scheduler with PrioritizeCompression Manager
        case PRIORITIZE:
            schedout.debug(CALL_INFO, 4, 0, "Prioritize Scheduler\n");
            if (schedparams -> size() == 1) {
                schedout.fatal(CALL_INFO, 1, 0, 0, "PrioritizeCompression scheduler requires number of backfill times as an argument");
            }
            filltimes = strtol(schedparams->at(1).c_str(),NULL,0);
            if (2 == schedparams -> size()) {
                return new StatefulScheduler(numProcs, StatefulScheduler::JobComparator::Make("fifo"),filltimes);
            } else {
                return new StatefulScheduler(numProcs, StatefulScheduler::JobComparator::Make(schedparams->at(2)), filltimes);
            }
            break;

            //Stateful Scheduler with Delayed Compression Manager
        case DELAYED:
            //if(DEBUG) printf("Delayed Compression Scheduler\n");
            schedout.debug(CALL_INFO, 4, 0, "Delayed Compression Scheduler\n");
            if (schedparams -> size() == 1) {
                return new StatefulScheduler(numProcs, StatefulScheduler::JobComparator::Make("fifo"));
            } else {
                return new StatefulScheduler(numProcs, StatefulScheduler::JobComparator::Make(schedparams -> at(1)));
            }
            break;

            //Stateful Scheduler with Even Less Conservative Manager
        case ELC:
            //if(DEBUG) printf("Even Less Convervative Scheduler\n");
            schedout.debug(CALL_INFO, 4, 0, "Even Less Convervative Scheduler\n");
            if (schedparams -> size() == 1) {
                schedout.fatal(CALL_INFO, 1, 0, 0, "Even Less Conservative scheduler requires number of backfill times as an argument");
            }
            filltimes = strtol(schedparams->at(1).c_str(),NULL,0);
            if (schedparams -> size() == 2) {
                return new StatefulScheduler(numProcs, StatefulScheduler::JobComparator::Make("fifo"),filltimes, true);
            } else {
                return new StatefulScheduler(numProcs, StatefulScheduler::JobComparator::Make(schedparams -> at(2)), filltimes, true);
            }
            break;

            //Default: scheduler name not matched
        default:
            schedout.fatal(CALL_INFO, 1, 0, 0, "Could not parse name of scheduler");
        }

    }
    return NULL; //control never reaches here
}

//returns the correct machine based on the parameters
Machine* Factory::getMachine(SST::Params& params, int numProcs, schedComponent* sc)
{
    if (params.find("machine") == params.end()) {
        //default: FIFO queue priority scheduler
        schedout.verbose(CALL_INFO, 4, 0, "Defaulting to Simple Machine\n");
        return new SimpleMachine(numProcs, sc, false);
    }
    else
    {
        vector<string>* schedparams = parseparams(params["machine"]);
        switch(machinename(schedparams -> at(0)))
        {
            //simple machine
        case SIMPLEMACH:
            schedout.debug(CALL_INFO, 4, 0, "Simple Machine\n");
            return new SimpleMachine(numProcs, sc, false);
            break;

            //Mesh Machine
        case MESH:
            {
                //if(DEBUG) printf("Mesh Machine\n");
                schedout.debug(CALL_INFO, 4, 0, "Mesh Machine\n");

                if (schedparams -> size() != 3 && schedparams -> size() != 4) {
                    schedout.fatal(CALL_INFO, 1, 0, 0, "Wrong number of arguments for Mesh Machine:\nNeed 3 (x, y, and z dimensions) or 2 (z defaults to 1)");
                }
                int x = strtol(schedparams -> at(1).c_str(), NULL, 0); 
                int y = strtol(schedparams -> at(2).c_str(), NULL, 0); 
                int z;
                if (schedparams -> size() == 4) {
                    z = strtol(schedparams -> at(3).c_str(), NULL, 0); 
                } else {
                    z = 1;
                }
                if (x * y * z != numProcs) {
                    schedout.fatal(CALL_INFO, 1, 0, 0, "The dimensions of the mesh do not correspond to the number of processors");
                }
                return new MachineMesh(x,y,z,sc);
                break;

            }
        default:
            schedout.fatal(CALL_INFO, 1, 0, 0, "Could not parse name of machine");
        }
    }
    return NULL; //control never reaches here
}


//returns the correct allocator based on the parameters
Allocator* Factory::getAllocator(SST::Params& params, Machine* m)
{
    if (params.find("allocator") == params.end()) {
        //default: FIFO queue priority scheduler
        schedout.verbose(CALL_INFO, 4, 0, "Defaulting to Simple Allocator\n");
        //printf("Defaulting to Simple Allocator\n");
        SimpleMachine* mach = dynamic_cast<SimpleMachine*>(m);
        if (mach == NULL) {
            schedout.fatal(CALL_INFO, 1, 0, 0, "Simple Allocator requires SimpleMachine");
        }
        return new SimpleAllocator(mach);
    } else {
        vector<string>* schedparams = parseparams(params["allocator"]);
        vector<string>* nearestparams = NULL;
        switch (allocatorname(schedparams -> at(0)))
        {
            //Simple Allocator for use with simple machine
        case SIMPLEALLOC:
            {
                //if(DEBUG) printf("Simple Allocator\n");
                schedout.debug(CALL_INFO, 4, 0, "Simple Allocator\n");

                SimpleMachine* mach = dynamic_cast<SimpleMachine*>(m);
                if (mach == NULL) {
                    schedout.fatal(CALL_INFO, 1, 0, 0, "SimpleAllocator requires SimpleMachine");
                }
                return new SimpleAllocator(mach);
                break;
            }


            //Random Allocator, allocates procs randomly from a mesh
        case RANDOM:
            schedout.debug(CALL_INFO, 4, 0, "Random Allocator\n");
            //if(DEBUG) printf("Random Allocator\n");
            return new RandomAllocator(m);
            break;

            //Nearest Allocators try to minimize distance between processors
            //according to various metrics
        case NEAREST:
            schedout.debug(CALL_INFO, 4, 0, "Nearest Allocator\n");
            //if(DEBUG) printf("Nearest Allocator\n");
            nearestparams = new vector<string>;
            for (int x = 0; x < (int)schedparams -> size(); x++) {
                nearestparams -> push_back(schedparams -> at(x));
            }
            return new NearestAllocator(nearestparams, m);
            break;
        case GENALG:
            schedout.debug(CALL_INFO, 4, 0, "General Algorithm Nearest Allocator\n");
            //if(DEBUG) printf("General Algorithm Nearest Allocator\n");
            nearestparams = new vector<string>;
            nearestparams -> push_back("genAlg");
            return new NearestAllocator(nearestparams, m);
            break;
        case MM:
            schedout.debug(CALL_INFO, 4, 0, "MM Allocator\n");
            //if(DEBUG) printf("MM Allocator\n");
            nearestparams = new vector<string>;
            nearestparams -> push_back("MM");
            return new NearestAllocator(nearestparams, m);
            break;
        case MC1X1:
            schedout.debug(CALL_INFO, 4, 0, "MC1x1 Allocator\n");
            //if(DEBUG) printf("MC1x1 Allocator\n");
            nearestparams = new vector<string>;
            nearestparams -> push_back("MC1x1");
            return new NearestAllocator(nearestparams, m);
            break;
        case OLDMC1X1:
            //if(DEBUG) printf("Old MC1x1 Allocator\n");
            schedout.debug(CALL_INFO, 4, 0, "Old MC1x1 Allocator\n");
            nearestparams = new vector<string>;
            nearestparams -> push_back("OldMC1x1");
            return new NearestAllocator(nearestparams, m);
            break;

            //MBS Allocators use a block-based approach
        case MBS:
            //if(DEBUG) printf("MBS Allocator\n");
            schedout.debug(CALL_INFO, 4, 0, "MBS Allocator\n");
            return new MBSAllocator(nearestparams, m);
        case GRANULARMBS:
            //if(DEBUG) printf("Granular MBS Allocator\n");
            schedout.debug(CALL_INFO, 4, 0, "Granular MBS Allocator\n");
            return new GranularMBSAllocator(nearestparams, m);
        case OCTETMBS: 
            //if(DEBUG) printf("Octet MBS Allocator\n");
            schedout.debug(CALL_INFO, 4, 0, "Octet MBS Allocator\n");
            return new OctetMBSAllocator(nearestparams, m);

            //Linear Allocators allocate in a linear fashion
            //along a curve
        case FIRSTFIT:
            schedout.debug(CALL_INFO, 4, 0, "First Fit Allocator\n");
            nearestparams = new vector<string>;
            for (int x = 1; x < (int)schedparams -> size(); x++) {
                nearestparams -> push_back(schedparams -> at(x));
            }
            return new FirstFitAllocator(nearestparams, m);
        case BESTFIT:
            schedout.debug(CALL_INFO, 4, 0, "Best Fit Allocator\n");
            nearestparams = new vector<string>;
            for (int x = 1; x < (int)schedparams -> size(); x++) {
                nearestparams -> push_back(schedparams -> at(x));
            }
            return new BestFitAllocator(nearestparams, m);
        case SORTEDFREELIST:
            schedout.debug(CALL_INFO, 4, 0, "Sorted Free List Allocator\n");
            nearestparams = new vector<string>;
            for (int x = 1; x < (int)schedparams -> size(); x++)
                nearestparams -> push_back(schedparams -> at(x));
            return new SortedFreeListAllocator(nearestparams, m);

            //Constraint Allocator tries to separate nodes whose estimated failure rates are close
        case CONSTRAINT:
            {
                if (params.find("ConstraintAllocatorDependencies") == params.end()) {
                    schedout.fatal(CALL_INFO, 1, 0, 0, "Constraint Allocator requires ConstraintAllocatorDependencies scheduler parameter");
                }
                if (params.find("ConstraintAllocatorConstraints") == params.end()) {
                    schedout.fatal(CALL_INFO, 1, 0, 0, "Constraint Allocator requires ConstraintAllocatorConstraints scheduler parameter");
                }
                SimpleMachine* mach = dynamic_cast<SimpleMachine*>(m);
                if (NULL == mach) {
                    schedout.fatal(CALL_INFO, 1, 0, 0, "ConstraintAllocator requires SimpleMachine");
                }
                // will get these file names from schedparams eventually
                return new ConstraintAllocator(mach, params.find("ConstraintAllocatorDependencies") -> second, params.find("ConstraintAllocatorConstraints") -> second );
                break;
            }

        default:
            schedout.fatal(CALL_INFO, 1, 0, 0, "Could not parse name of allocator");
        }
    }
    return NULL; //control never reaches here
}

int Factory::getFST(SST::Params& params)
{
    if(params.find("FST") == params.end()){
        //default: FIFO queue priority scheduler
        //schedout.verbose(CALL_INFO, 4, 0, "Defaulting to no FST");
        return 0;
    } else {
        vector<string>* FSTparams = parseparams(params["FST"]);
        switch (FSTname(FSTparams -> at(0)))
        {
        case NONE:
            return 0;
        case STRICT:
            return 1;
        case RELAXED:
            return 2;
        default:
            schedout.fatal(CALL_INFO, 1, 0, 0, "Could not parse FST type; should be none, strict, or relaxed");
        }
    }
    schedout.fatal(CALL_INFO, 1, 0, 0, "Could not parse FST type; should be none, strict, or relaxed");
    return 0; 
}

vector<double>* Factory::getTimePerDistance(SST::Params& params)
{
    vector<double>* ret = new vector<double>;
    for (int x = 0; x < 4; x++) {
        ret -> push_back(0);
    }
    if(params.find("timeperdistance") == params.end()){
        //default: FIFO queue priority scheduler
        //schedout.verbose(CALL_INFO, 4, 0, "Defaulting to no FST");
        return ret;
    } else {
        vector<string>* tpdparams = parseparams(params["timeperdistance"]);
        for (unsigned int x = 0; x < tpdparams -> size(); x++) {
            ret -> at(x) = atof(tpdparams -> at(x).c_str());
            //printf("%s %f %f\n", tpdparams -> at(x).c_str(), atof(tpdparams->at(x).c_str()), ret->at(x));
        }
        return ret;
        //return atof(tpdparams -> at(0).c_str());
    }
    //schedout.fatal(CALL_INFO, 1, 0, 0, "Could not parse timeperdistance; should be a floating point integer");
    return ret; 
}


//takes in a parameter and breaks it down from class[arg,arg,...]
//into {class, arg, arg}
vector<string>* Factory::parseparams(string inparam)
{
    vector<string>* ret = new vector<string>;
    stringstream ss;
    ss.str(inparam);
    string str;
    getline(ss,str,'[');
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    ret -> push_back(str);
    while(getline(ss, str, ',') && str != "]") {
        transform(str.begin(), str.end(), str.begin(), ::tolower);
        if(*(str.rbegin()) == ']') {
            str = str.substr(0,str.size()-1);
        }
        ret -> push_back(str);
    }
    return ret;
}

Factory::SchedulerType Factory::schedulername(string inparam)
{
    for (int i = 0; i < numSchedTableEntries; i++) {
        if (inparam == schedTable[i].name) return schedTable[i].val;
    }
    schedout.fatal(CALL_INFO, 1, 0, 0, "Scheduler name not found:%s", inparam.c_str());
    exit(0); // control never reaches here
}

Factory::MachineType Factory::machinename(string inparam)
{
    for(int i = 0; i < numMachTableEntries; i++) {
        if (inparam == machTable[i].name) return machTable[i].val;
    }
    schedout.fatal(CALL_INFO, 1, 0, 0, "Machine name not found:%s", inparam.c_str());
    exit(0);
}

Factory::AllocatorType Factory::allocatorname(string inparam)
{
    for(int i = 0; i < numAllocTableEntries; i++) {
        if (inparam == allocTable[i].name) return allocTable[i].val;
    }
    schedout.fatal(CALL_INFO, 1, 0, 0, "Allocator name not found:%s", inparam.c_str());
    exit(0);
}

Factory::FSTType Factory::FSTname(string inparam)
{
    for(int i = 0; i < numFSTTableEntries; i++) {
        if (inparam == FSTTable[i].name) return FSTTable[i].val;
    } 
    schedout.fatal(CALL_INFO, 1, 0, 0, "FST name not found:%s", inparam.c_str());
    exit(0);
}
