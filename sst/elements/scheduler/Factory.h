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


/* Factory file helps parse the parameters in the sdl file
 * returns correct type of machine, allocator, and scheduler
 */

#ifndef SST_SCHEDULER_FACTORY_H__
#define SST_SCHEDULER_FACTORY_H__

#include "sst/core/serialization.h"
#include "sst/core/element.h"
#include <string>
#include <vector>

namespace SST {
    class Params;
    namespace Scheduler {


        //forward declared classes
        class schedComponent;
        class Machine;
        class Scheduler;
        class Allocator;
        class FST;

        class Factory{
            public:
                Factory(); //only sets up the output class
                Scheduler* getScheduler(SST::Params& params, int numProcs);
                Machine* getMachine(SST::Params& params, int numProcs, schedComponent* sc);
                Allocator* getAllocator(SST::Params& params, Machine* m);
                int getFST(SST::Params& params);
                std::vector<double>* getTimePerDistance(SST::Params& params);
            private:
                std::vector<std::string>* parseparams(std::string inparam);

                //the following is for easy conversion from strings to ints
                //to add a new (say) scheduler, add its string to Schedulertype
                //and schedTable[], and increment numSchedTableEntries and the
                //size of schedTable.  Then add a case to getScheduler() that calls
                //its constructor
                enum SchedulerType{
                    PQUEUE = 0,
                    EASY = 1,
                    CONS = 2,
                    PRIORITIZE = 3,
                    DELAYED = 4,
                    ELC = 5,
                };
                enum MachineType{
                    SIMPLEMACH = 0,
                    MESH = 1,
                };
                enum AllocatorType{
                    SIMPLEALLOC = 0,
                    RANDOM = 1,
                    NEAREST = 2,
                    GENALG = 3,
                    MM = 4,
                    MC1X1 = 5,
                    OLDMC1X1 = 6,
                    MBS = 7,
                    GRANULARMBS = 8,
                    OCTETMBS = 9,
                    FIRSTFIT = 10,
                    BESTFIT = 11,
                    SORTEDFREELIST = 12,
                    CONSTRAINT = 13,
                };
                enum FSTType{
                    NONE = 0,
                    STRICT = 1,
                    RELAXED = 2,
                };

                struct schedTableEntry{
                    SchedulerType val;
                    std::string name;
                };
                struct machTableEntry{
                    MachineType val;
                    std::string name;
                };
                struct allocTableEntry{
                    AllocatorType val;
                    std::string name;
                };

                struct FSTTableEntry{
                    FSTType val;
                    std::string name;
                };

                static const schedTableEntry schedTable[6];
                static const machTableEntry machTable[2];
                static const allocTableEntry allocTable[14];
                static const FSTTableEntry FSTTable[3];

                SchedulerType schedulername(std::string inparam);
                MachineType machinename(std::string inparam);
                AllocatorType allocatorname(std::string inparam);
                FSTType FSTname(std::string inparam);
                static const int numSchedTableEntries;
                static const int numAllocTableEntries;
                static const int numMachTableEntries;
                static const int numFSTTableEntries;
        };

    }
}
#endif
