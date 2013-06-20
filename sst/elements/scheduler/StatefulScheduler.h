// Copyright 2011 Sandia Corporation. Under the terms                          
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

#ifndef SST_SCHEDULER__STATEFULSCHEDULER_H__
#define SST_SCHEDULER__STATEFULSCHEDULER_H__

//#include <functional>
#include <map>
#include <set>
#include <string>

#include "Scheduler.h"

namespace SST {
    namespace Scheduler {
        class Job;

        class SchedChange{
            protected:
                unsigned long time;
                SchedChange* partner;

            public:
                bool isEnd;
                Job* j;

                //need to implement some sort of comparison function for SchedChange

                const unsigned long getTime()
                {
                    return time;
                }

                SchedChange* getPartner()
                {
                    return partner;
                }

                int freeProcChange();
                char* toString();
                void print();
                SchedChange(unsigned long intime, Job* inj, bool end, SchedChange* inpartner = NULL);

        };

        class SCComparator {
            public:
                bool operator()(SchedChange* const& first, SchedChange* const& second);
        };

        class StatefulScheduler : public Scheduler {
            private:
                static const int numCompTableEntries;
                enum ComparatorType {  //to represent type of JobComparator
                    FIFO = 0,
                    LARGEFIRST = 1,
                    SMALLFIRST = 2,
                    LONGFIRST = 3,
                    SHORTFIRST = 4,
                    BETTERFIT = 5
                };
                struct compTableEntry {
                    ComparatorType val;
                    std::string name;
                };
                static const compTableEntry compTable[6];

                std::string compSetupInfo;
                std::set<SchedChange*, SCComparator> *estSched;
                unsigned long findTime(std::set<SchedChange*, SCComparator> *sched, Job* Job, unsigned long time);

                int numProcs;
                int freeProcs;

            public:
                void jobArrives(Job* j, unsigned long time, Machine* mach);
                void jobFinishes(Job* j, unsigned long time, Machine* mach);

                //Make????
                void reset();
                unsigned long scheduleJob(Job* job, unsigned long time);
                unsigned long zeroCase(std::set<SchedChange*, SCComparator> *sched, Job* filler, unsigned long time);
                AllocInfo* tryToStart(Allocator* alloc, unsigned long time, Machine* mach, Statistics* stats);
                std::string getSetupInfo(bool comment);
                void printPlan();
                void done()
                {
                    heart -> done(); 
                }
                void removeJob(Job* j, unsigned long time);

                class JobComparator : public std::binary_function<Job*,Job*,bool> {
                    public:
                        static JobComparator* Make(std::string typeName);  //return NULL if name is invalid
                        static void printComparatorList(std::ostream& out);  //print list of possible comparators
                        bool operator()(Job*& j1, Job*& j2);
                        bool operator()(Job* const& j1, Job* const& j2);
                        std::string toString();
                    private:
                        JobComparator(ComparatorType type);
                        ComparatorType type;
                };

                //MANAGERS:******************************************************
                class Manager{
                    protected:
                        StatefulScheduler* scheduler;
                    public:
                        virtual void arrival(Job* j, unsigned long time) = 0;
                        virtual void start(Job* j, unsigned long time) = 0;
                        virtual void tryToStart(unsigned long time) = 0;
                        virtual void printPlan() = 0;
                        virtual void onTimeFinish(Job* j, unsigned long time) = 0;
                        virtual void reset() = 0;
                        virtual void done() = 0;
                        virtual void earlyFinish(Job* j, unsigned long time) = 0;
                        virtual void removeJob(Job* j, unsigned long time) = 0;
                        virtual std::string getString() = 0;
                        void compress(unsigned long time) ;
                };

                class ConservativeManager : public Manager{
                    public:
                        ConservativeManager(StatefulScheduler* inscheduler)
                        {
                            scheduler = inscheduler;
                        }
                        void earlyFinish(Job* j, unsigned long time)
                        {
                            compress(time);
                        }
                        void removeJob(Job* j, unsigned long time)
                        {
                            compress(time);
                        }
                        void arrival(Job* j, unsigned long time) { }
                        void start(Job* j, unsigned long time) { }
                        void tryToStart(unsigned long time){ }
                        void printPlan() { }
                        void onTimeFinish(Job* j, unsigned long time) { }
                        void reset() { }
                        void done() { }
                        std::string getString();
                };

                class PrioritizeCompressionManager : public Manager{
                    protected:
                        std::set<Job*, JobComparator>* backfill;
                        int fillTimes;
                        int* numSBF;
                    public:
                        PrioritizeCompressionManager(StatefulScheduler* inscheduler, JobComparator* comp, int infillTimes);
                        void reset();
                        void arrival(Job* j, unsigned long time) {
                            backfill -> insert(j);
                        }
                        void start(Job *j, unsigned long time) {
                            backfill -> erase(j);
                        }
                        void printPlan();
                        void done();
                        void earlyFinish(Job* j, unsigned long time);
                        void tryToStart(unsigned long time);
                        void removeJob(Job* j, unsigned long time);
                        void onTimeFinish(Job* j, unsigned long time);
                        std::string getString();
                };

                class DelayedCompressionManager : public Manager {
                    protected:
                        std::set<Job*, JobComparator>* backfill;
                    public:
                        DelayedCompressionManager(StatefulScheduler* inscheduler, JobComparator* comp);
                        void reset();
                        void arrival(Job* j, unsigned long time);
                        void start(Job* j, unsigned long time){
                            backfill -> erase(j);
                        }
                        void tryToStart(unsigned long time);
                        void printPlan();
                        void done();
                        void earlyFinish(Job *j, unsigned long time);
                        void fill(unsigned long time);
                        void removeJob(Job* j, unsigned long time);
                        void onTimeFinish(Job* j, unsigned long time);
                        std::string getString();
                    private:
                        int results;
                };

                class EvenLessManager : public Manager{
                    protected:
                        std::set<Job*, JobComparator>* backfill;
                        std::set<SchedChange*, SCComparator> *guarantee;
                        std::map<Job*, SchedChange*, JobComparator> *guarJobToEvents;
                        int bftimes;
                    public:
                        EvenLessManager(StatefulScheduler* inscheduler, JobComparator* comp, int infillTimes);
                        void deepCopy(std::set<SchedChange*, SCComparator> *from, std::set<SchedChange*, SCComparator> *to, std::map<Job*, SchedChange*, JobComparator> *toJ);
                        void backfillfunc(unsigned long time);
                        void arrival(Job* j, unsigned long time);
                        void start(Job* j, unsigned long time);
                        void tryToStart(unsigned long time){};
                        void printPlan();
                        void done(){};
                        void earlyFinish(Job* j, unsigned long time);
                        void onTimeFinish(Job* j, unsigned long time);
                        void fill(unsigned long time);
                        void removeJob(Job* j, unsigned long time);
                        void reset();
                        std::string getString();
                    private:
                        int results;
                };
                //MANAGERS OVER***************************************************************

                std::map<Job*, SchedChange*, JobComparator>* jobToEvents;

                StatefulScheduler(int numprocs, JobComparator* comp, bool dummy);
                StatefulScheduler(int numprocs, JobComparator* comp, int fillTimes);
                StatefulScheduler(int numprocs, JobComparator* comp);
                StatefulScheduler(int numprocs, JobComparator* comp, int fillTimes, bool dummy);
                StatefulScheduler(JobComparator* comp);

            protected:
                Manager *heart;
        };


    }
}
#endif
