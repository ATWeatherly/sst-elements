// Copyright 2013 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013, Sandia Corporation
// All rights reserved.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#include <sst_config.h>

#include <string.h>
#include <sstream>

#include <sst/core/link.h>

#include "functionSM.h"
#include "ctrlMsg.h"

using namespace SST::Firefly;
using namespace Hermes;

const char* FunctionSM::m_functionName[] = {
    FOREACH_FUNCTION(GENERATE_STRING)
};

class DriverEvent : public SST::Event {
  public:
    DriverEvent( Hermes::Functor* _retFunc, int _retval ) :
        Event(),
        retFunc( _retFunc ),
        retval( _retval )
    { }
    Hermes::Functor* retFunc;
    int retval;
  private:
};

FunctionSM::FunctionSM( SST::Params& params, SST::Component* obj, Info& info, 
        SST::Link* toProgressLink, std::map<std::string,ProtocolAPI*>& proto ) :
    m_sm( NULL ),
    m_info( info ),
    m_params( params ),
    m_owner( obj ),
    m_proto( proto )
{
    int verboseLevel = params.find_integer("verboseLevel",0);
    Output::output_location_t loc = 
            (Output::output_location_t)params.find_integer("debug", 0);

    m_dbg.init("@t:FunctionSM::@p():@l ", verboseLevel, 0, loc );

    m_toDriverLink = obj->configureSelfLink("ToDriver", "1 ps",
        new Event::Handler<FunctionSM>(this,&FunctionSM::handleToDriver));

    m_fromDriverLink = obj->configureSelfLink("FromDriver", "1 ps",
        new Event::Handler<FunctionSM>(this,&FunctionSM::handleStartEvent));
    assert( m_fromDriverLink );

    m_toMeLink = obj->configureSelfLink("ToMe", "1 ps",
        new Event::Handler<FunctionSM>(this,&FunctionSM::handleEnterEvent));
    assert( m_toMeLink );

    std::map<std::string,ProtocolAPI*>::iterator iter = proto.begin();
    while ( iter != proto.end() ) {
        iter->second->setRetLink( m_toMeLink );
        ++iter;
    }
}

FunctionSM::~FunctionSM()
{
    m_dbg.verbose(CALL_INFO,1,0,"\n");
    for ( unsigned int i=0; i < m_smV.size(); i++ ) {
        delete m_smV[i];
    }
    delete m_fromDriverLink;
}

void FunctionSM::printStatus( Output& out )
{
    m_sm->printStatus(out); 
}

void FunctionSM::setup()
{
    char buffer[100];
    snprintf(buffer,100,"@t:%d:%d:FunctionSM::@p():@l ",m_info.nodeId(),
                                                m_info.worldRank());
    m_dbg.setPrefix(buffer);

    m_smV.resize( NumFunctions );

    Params defaultParams;
    defaultParams[ "module" ] = m_params.find_string("defaultModule","firefly");
    defaultParams[ "latency" ] = m_params.find_string("defaultLatency","0");
    defaultParams[ "debug" ]   = m_params.find_string("defaultDebug","0");
    defaultParams[ "verbose" ] = m_params.find_string("defaultVerbose","0"); 
    std::ostringstream tmp;
    tmp <<  m_info.nodeId(); 
    defaultParams[ "nodeId" ] = tmp.str();
    tmp.str("");
    tmp << m_info.worldRank(); 
    defaultParams[ "worldRank" ] = tmp.str();

    for ( int i = 0; i < NumFunctions; i++ ) {
        std::string name = functionName( (FunctionEnum) i );
        Params tmp = m_params.find_prefix_params( name + "." );  
        defaultParams[ "name" ] = name;
        initFunction( m_owner, &m_info, (FunctionEnum) i,
                                        name, defaultParams, tmp ); 
    }
}

void FunctionSM::initFunction( SST::Component* obj, Info* info,
    FunctionEnum num, std::string name, Params& defaultParams, Params& params)
{
    std::string module = params.find_string("module"); 
    if ( module.empty() ) {
        module = defaultParams["module"];
    }

    m_dbg.verbose(CALL_INFO,3,0,"func=`%s` module=`%s`\n",
                            name.c_str(),module.c_str());

    if ( params.find_string("name").empty() ) {
        params["name"] = defaultParams[ "name" ];
    }

    if ( params.find_string("verbose").empty() ) {
        params["verbose"] = defaultParams[ "verbose" ];
    }

    if ( params.find_string("debug").empty() ) {
        params["debug"] = defaultParams[ "debug" ];
    }

    params["nodeId"] = defaultParams[ "nodeId" ];
    params["worldRank"] = defaultParams[ "worldRank" ];

    m_smV[ num ] = (FunctionSMInterface*)obj->loadModule( module + "." + name,
                             params );

    assert( m_smV[ Init ] );
    m_smV[ num ]->setInfo( info ); 

    if ( ! m_smV[ num ]->protocolName().empty() ) {
        ProtocolAPI* proto = m_proto[ m_smV[ num ]->protocolName() ];
        m_dbg.verbose(CALL_INFO,3,0,"%p\n", proto );
        m_smV[ num ]->setProtocol( proto ); 
    }
}

void FunctionSM::enter( )
{
    m_dbg.verbose(CALL_INFO,3,0,"%s\n",m_sm->name().c_str());
    m_toMeLink->send( NULL );
}

void FunctionSM::start( int type, Hermes::Functor* retFunc, SST::Event* e )
{
    assert( e );
    m_retFunc = retFunc;
    assert( ! m_sm );
    m_sm = m_smV[ type ];
    m_dbg.verbose(CALL_INFO,3,0,"%s enter\n",m_sm->name().c_str());
    m_fromDriverLink->send( m_sm->enterLatency(), e );
}

void FunctionSM::handleStartEvent( SST::Event* e )
{
    Retval retval;
    assert( e );
    assert( m_sm );
    m_dbg.verbose(CALL_INFO,3,0,"%s\n",m_sm->name().c_str());
    m_sm->handleStartEvent( e, retval );
    processRetval( retval );
}


void FunctionSM::handleEnterEvent( SST::Event* e )
{
    assert( m_sm );
    m_dbg.verbose(CALL_INFO,3,0,"%s\n",m_sm->name().c_str());
    Retval retval;
    m_sm->handleEnterEvent( retval );
    processRetval( retval );
}

void FunctionSM::processRetval(  Retval& retval )
{
    if ( retval.isExit() ) {
        m_dbg.verbose(CALL_INFO,3,0,"Exit %d\n", retval.value() );
        DriverEvent* x = new DriverEvent( m_retFunc, retval.value() );
        m_toDriverLink->send( 0, x ); 
    } else if ( retval.isDelay() ) {
        m_dbg.verbose(CALL_INFO,3,0,"Delay %d\n", retval.value() );
        m_toMeLink->send( retval.value(), NULL ); 
    } else {
    }
}

void FunctionSM::handleToDriver( Event* e )
{
    assert( e );
    m_dbg.verbose(CALL_INFO,3,0," returning\n");
    DriverEvent* event = static_cast<DriverEvent*>(e);
    (*event->retFunc)( event->retval );
    m_sm = NULL;
    delete e;
}

