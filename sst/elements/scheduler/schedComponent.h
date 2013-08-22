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

#ifndef SST_SCHEDULER_SCHEDCOMPONENT_H__
#define SST_SCHEDULER_SCHEDCOMPONENT_H__

#include <fstream>
#include <string>
#include <boost/filesystem.hpp>

#include <sst/core/component.h>
#include <sst/core/link.h>

namespace SST {
    class Event;
    class Link;
    namespace Scheduler {

        class Job;
        class FaultEvent;
        class CompletionEvent;
        class ArrivalEvent;
        class Machine;
        class Scheduler;
        class Allocator;
        class Statistics;
        class AllocInfo;


        // the maximum length of a job ID.  used primarily for job list parsing.
#define JobIDlength 16

        struct IAI {int i; AllocInfo *ai;};

        class schedComponent : public SST::Component {
            public:

                schedComponent(SST::ComponentId_t id, SST::Params& params);
                ~schedComponent(); 
                void setup();
                void finish();
                Machine* getMachine();
                void startJob(AllocInfo* ai);

                //ConstraintAllocator needs this
                std::string getNodeID(int nodeIndex){
                    return nodeIDs.at(nodeIndex);
                }


            private:
                unsigned long lastfinaltime;
                schedComponent();  // for serialization only
                schedComponent(const schedComponent&); // do not implement
                void operator=(const schedComponent&); // do not implement

                bool validateJob( Job * j, std::vector<Job> * jobs, long runningTime );

                void registerThis();
                void unregisterThis();
                bool isRegistered();

                bool registrationStatus;

                std::string trace;
                std::string jobListFilename;

                void readJobs();
                bool checkJobFile();
                bool newJobLine( std::string line );
                bool newYumYumJobLine( std::string line );

                void handleCompletionEvent(Event *ev, int n);
                void handleJobArrivalEvent(Event *ev);
                //virtual bool clockTic( SST::Cycle_t );

                void unregisterYourself();

                void startNextJob();

                void logJobStart(IAI iai);
                void logJobFinish(IAI iai);
                void logJobFault(IAI iai, FaultEvent * faultEvent );

                typedef std::vector<int> targetList_t;

                std::vector<Job> jobs;
                std::list<CompletionEvent*> finishingcomp;
                std::list<ArrivalEvent*> finishingarr;
                Machine* machine;
                Scheduler* scheduler;
                Allocator* theAllocator;
                Statistics* stats;
                std::vector<SST::Link*> nodes;
                std::vector<std::string> nodeIDs;
                SST::Link* selfLink;
                std::map<int, IAI> runningJobs;

                std::string jobListFileName;
                boost::filesystem::path jobListFileNamePath;
                std::string jobLogFileName;
                std::ofstream jobLog;
                bool printJobLog;
                bool printYumYumJobLog;       // should the Job Log use the YumYum format?
                bool useYumYumTraceFormat;    // should we expect the incoming job list to use the YumYum format?
                // useYumYumTraceFormat is regularly used to decide if YumYum functionality should be used or not.

                bool useYumYumSimulationKill;         // should the simulation end on a special job (true), or just when the job list is exhausted (false)?
                bool YumYumSimulationKillFlag;        // this will signal the schedComponent to unregister itself iff useYumYumSimulationKill == true
                int YumYumPollWait;                   // this is the length of time in ms to wait between checks for new jobs
                time_t LastJobFileModTime;            // Contains the last time that the job file was modified
                char lastJobRead[ JobIDlength ];      // The ID of the last job read from the Job list file


                friend class boost::serialization::access;
                template<class Archive>
                    void save(Archive & ar, const unsigned int version) const
                    {
                        ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Component);
                        ar & BOOST_SERIALIZATION_NVP(nodes);
                        ar & BOOST_SERIALIZATION_NVP(selfLink);
                    }

                template<class Archive>
                    void load(Archive & ar, const unsigned int version) 
                    {
                        ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Component);
                        ar & BOOST_SERIALIZATION_NVP(nodes);
                        ar & BOOST_SERIALIZATION_NVP(selfLink);
                        //restore links
                        for (unsigned int i = 0; i < nodes.size(); ++i) {
                            nodes[i] -> setFunctor(new SST::Event::Handler<schedComponent,int>(this,
                                                                                             &schedComponent::handleCompletionEvent,i));
                        }

                        selfLink -> setFunctor(new SST::Event::Handler<schedComponent>(this,
                                                                                     &schedComponent::handleJobArrivalEvent));
                    }

                BOOST_SERIALIZATION_SPLIT_MEMBER()

        };

    }
}
#endif /* _SCHEDCOMPONENT_H */
