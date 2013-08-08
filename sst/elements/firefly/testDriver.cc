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
#include "sst/core/serialization.h"

#include "sst/core/element.h"
#include "sst/core/component.h"
#include "sst/core/module.h"
#include "sst/core/timeLord.h"

#include "testDriver.h"

using namespace Hermes;
using namespace SST;
using namespace SST::Firefly;

TestDriver::TestDriver(ComponentId_t id, Params_t &params) :
    Component( id ),
    m_functor( DerivedFunctor( this, &TestDriver::funcDone ) ),
    my_rank( AnySrc ),
    my_size(0)
{
    // this has to come first 
    registerTimeBase( "100 ns", true);

    std::string name = params.find_string("hermesModule");
    if ( name == "" ) {
        _abort(TestDriver, "ERROR:  What Hermes module? '%s'\n", name.c_str());
    } 
   
    m_dbg.init("@t:TestDriver::@p():@l " + getName() + ": ", 
            params.find_integer("verboseLevel",0),0,
            (Output::output_location_t)params.find_integer("debug", 0));
    m_dbg.output(CALL_INFO,"loading module `%s`\n",name.c_str());

    std::ostringstream ownerName;
    ownerName << this;
    Params_t hermesParams = params.find_prefix_params("hermesParams." ); 
    hermesParams.insert( 
        std::pair<std::string,std::string>("owner", ownerName.str()));

    m_hermes = dynamic_cast<MessageInterface*>(loadModule(name,hermesParams));
    if ( !m_hermes ) {
        _abort(TestDriver, "ERROR:  Unable to find Hermes '%s'\n",
                                        name.c_str());
    }

    m_selfLink = configureSelfLink("Self", "100 ns",
        new Event::Handler<TestDriver>(this,&TestDriver::handle_event));

    m_traceFileName = params.find_string("traceFile");

    m_bufLen = params.find_integer( "bufLen" );
    assert( m_bufLen != (size_t)-1 ); 

    m_dbg.output(CALL_INFO,"bufLen=%lu\n",m_bufLen);
    m_recvBuf.resize(m_bufLen);
    m_sendBuf.resize(m_bufLen);
    
    m_root = 3;
    for ( unsigned int i = 0; i < m_sendBuf.size(); i++ ) {
        m_sendBuf[i] = i;
    } 
}
    
TestDriver::~TestDriver()
{
}

void TestDriver::init( unsigned int phase )
{
    m_dbg.verbose(CALL_INFO,1,0,"\n");
    m_hermes->_componentInit( phase );
}

void TestDriver::setup() 
{ 
    m_hermes->_componentSetup( );

    std::ostringstream tmp;

    tmp << m_traceFileName.c_str() << m_hermes->myWorldRank();

    if ( m_hermes->myWorldRank() == 0 ) {
//        m_selfLink->send(10000,NULL);
        m_selfLink->send(1,NULL);
    } else {
        m_selfLink->send(1,NULL);
    }

    m_traceFile.open( tmp.str().c_str() );
    m_dbg.verbose(CALL_INFO,1,0,"traceFile `%s`\n",tmp.str().c_str());
    if ( ! m_traceFile.is_open() ) {
        _abort(TestDriver, "ERROR:  Unable to open trace file '%s'\n",
                                        tmp.str().c_str() );
    }

    char buffer[100];
    snprintf(buffer,100,"@t:%d:TestDriver::@p():@l ",m_hermes->myWorldRank()); 
    m_dbg.setPrefix(buffer);

    m_collectiveOut = 0x12345678;
    m_collectiveIn =  m_hermes->myWorldRank() << ( m_hermes->myWorldRank() *8); 
}

void TestDriver::handle_event( Event* ev )
{
    getline( m_traceFile, m_funcName );

    m_dbg.verbose(CALL_INFO,1,0, "function `%s`\n" , m_funcName.c_str());

    m_funcName = m_funcName.c_str();
    if ( ! m_funcName.empty() ) {
        m_dbg.verbose(CALL_INFO,1,0,"%d: %s\n",my_rank, m_funcName.c_str());
    }
    if ( m_funcName.compare( "init" ) == 0 ) {
        m_hermes->init( &m_functor );
    } else if ( m_funcName.compare( "size" ) == 0 ) {
        m_hermes->size( GroupWorld, &my_size, &m_functor );
    } else if ( m_funcName.compare( "rank" ) == 0 ) {
        m_hermes->rank( GroupWorld, &my_rank, &m_functor );
    } else if ( m_funcName.compare( "recv" ) == 0 ) {
        m_dbg.verbose(CALL_INFO,1,0,"my_size=%d my_rank=%d\n",my_size, my_rank);
        m_hermes->recv( &m_recvBuf[0], m_recvBuf.size(), CHAR, 
                                    (my_rank + 1) % 2, 
                                    AnyTag, 
                                    GroupWorld, 
                                    &my_resp, 
                                    &m_functor);

    } else if ( m_funcName.compare( "irecv" ) == 0 ) {
        m_dbg.verbose(CALL_INFO,1,0,"my_size=%d my_rank=%d\n",my_size, my_rank);
        m_hermes->irecv( &m_recvBuf[0], m_recvBuf.size(), CHAR, 
                                    (my_rank + 1) % 2, 
                                    AnyTag, 
                                    GroupWorld, 
                                    &my_req, 
                                    &m_functor);

    } else if ( m_funcName.compare( "send" ) == 0 ) {
        m_dbg.verbose(CALL_INFO,1,0,"my_size=%d my_rank=%d\n",my_size, my_rank);
        m_hermes->send( &m_sendBuf[0], m_sendBuf.size(), CHAR,
                                (my_rank + 1 ) % 2,
                                0xdead, 
                                GroupWorld, 
                                &m_functor);

    } else if ( m_funcName.compare( "barrier" ) == 0 ) {
        m_dbg.verbose(CALL_INFO,1,0,"my_size=%d my_rank=%d\n",my_size, my_rank);
        m_hermes->barrier( GroupWorld, &m_functor );
    } else if ( m_funcName.compare( "allgather" ) == 0 ) {
        allgatherEnter();
    } else if ( m_funcName.compare( "allgatherv" ) == 0 ) {
        allgathervEnter();
    } else if ( m_funcName.compare( "gather" ) == 0 ) {
        gatherEnter();
    } else if ( m_funcName.compare( "gatherv" ) == 0 ) {
        gathervEnter();
    } else if ( m_funcName.compare( "reduce" ) == 0 ) {
        m_dbg.verbose(CALL_INFO,1,0,"my_size=%d my_rank=%d\n",my_size, my_rank);
        m_hermes->reduce( &m_collectiveIn, &m_collectiveOut, 1, INT,
                                SUM, m_root, GroupWorld, &m_functor );
    } else if ( m_funcName.compare( "allreduce" ) == 0 ) {
        m_dbg.verbose(CALL_INFO,1,0,"my_size=%d my_rank=%d\n",my_size, my_rank);
        m_hermes->allreduce( &m_collectiveIn, &m_collectiveOut, 1, INT,
                                SUM, GroupWorld, &m_functor );
    } else if ( m_funcName.compare( "wait" ) == 0 ) {
        m_hermes->wait( &my_req, &my_resp, &m_functor );
    } else if ( m_funcName.compare( "fini" ) == 0 ) {

        m_hermes->fini( &m_functor );
    }
}

void TestDriver::funcDone( int retval )
{
    m_selfLink->send(1,NULL);

    if ( m_funcName.compare( "size" ) == 0 ) {
        m_dbg.verbose(CALL_INFO,1,0,"`%s` size=%d\n" , m_funcName.c_str(),
             my_size);
    } else if ( m_funcName.compare( "rank" ) == 0 ) {
        m_dbg.verbose(CALL_INFO,1,0,"`%s` rank=%d\n" , m_funcName.c_str(),
             my_rank);
    } else if ( m_funcName.compare( "wait" ) == 0 ) {
        waitReturn();
    } else if ( m_funcName.compare( "recv" ) == 0 ) {
        recvReturn();
    } else if ( m_funcName.compare( "allgather" ) == 0 ) {
        allgatherReturn();
    } else if ( m_funcName.compare( "allgatherv" ) == 0 ) {
        allgathervReturn();
    } else if ( m_funcName.compare( "gather" ) == 0 ) {
        gatherReturn();
    } else if ( m_funcName.compare( "gatherv" ) == 0 ) {
        gathervReturn();
    } else if ( m_funcName.compare( "allreduce" ) == 0 ) {
        printf("%d: collective result %#x\n",my_rank, m_collectiveOut);
    } else if ( m_funcName.compare( "reduce" ) == 0 ) {
        if ( m_root == my_rank ) {
            printf("%d: collective result %#x\n",my_rank, m_collectiveOut);
        }
    } else {
        m_dbg.verbose(CALL_INFO,1,0,"`%s` retval=%d\n" ,
                                m_funcName.c_str(), retval);
    }
}

void TestDriver::recvReturn( )
{
    printf("%lu:%d: src=%d tag=%#x len=%lu\n",
            Simulation::getSimulation()->getCurrentSimCycle(),
                my_rank, my_resp.src, my_resp.tag,m_recvBuf.size());
    for ( unsigned int i = 0; i < m_recvBuf.size(); i++ ) {
        if ( m_recvBuf[i] != (i&0xff) ) {
            printf("ERROR %d != %d\n",i,m_recvBuf[i]);
        }
    }
}

void TestDriver::waitReturn( )
{
    printf("%lu:%d: src=%d tag=%#x len=%lu\n",
            Simulation::getSimulation()->getCurrentSimCycle(),
                my_rank, my_req.src, my_req.tag,m_recvBuf.size());
    for ( unsigned int i = 0; i < m_recvBuf.size(); i++ ) {
        if ( m_recvBuf[i] != (i&0xff) ) {
            printf("ERROR %d != %d\n",i,m_recvBuf[i]);
        }
    }
}


// GATHER
void TestDriver::gatherEnter( )
{
    m_dbg.verbose(CALL_INFO,1,0,"my_rank=%d\n", my_rank);
    assert( my_size != 0 ); 

    m_gatherRecvBuf.resize(m_bufLen * my_size);
    m_gatherSendBuf.resize(m_bufLen);

    for ( unsigned int i = 0; i < m_gatherSendBuf.size(); i++ ) {
        m_gatherSendBuf[ i ] = my_rank + 0xbeef0000; 
    }

    m_hermes->gather( &m_gatherSendBuf[0],  m_gatherSendBuf.size(), INT,
                    &m_gatherRecvBuf[0],  m_gatherRecvBuf.size()/my_size, INT,
                    m_root, GroupWorld, &m_functor );
}

void TestDriver::gatherReturn( )
{
    if ( my_rank == m_root ) {
        m_dbg.verbose(CALL_INFO,1,0,"\n");
        for ( unsigned int i = 0; i < m_gatherRecvBuf.size(); i++ ) {
            m_dbg.verbose(CALL_INFO,1,0,"%#x\n",m_gatherRecvBuf[i]);
        }
    }
}

// GATHERV 
void TestDriver::gathervEnter( )
{
    m_dbg.verbose(CALL_INFO,1,0,"my_rank=%d\n", my_rank);
    assert( my_size != 0 ); 

    m_recvcnt.resize( my_size );
    m_displs.resize( my_size );

    for ( int i = 0; i < my_size; i++ ) {
        m_recvcnt[i] = m_bufLen; 
        m_displs[i] = (m_bufLen * m_hermes->sizeofDataType(INT)) * i; 
    }
    m_gatherRecvBuf.resize(m_bufLen * my_size);
    m_gatherSendBuf.resize(m_bufLen);

    for ( unsigned int i = 0; i < m_gatherSendBuf.size(); i++ ) {
        m_gatherSendBuf[ i ] = my_rank + 0xbeef0000; 
    }

    m_hermes->gatherv( &m_gatherSendBuf[0],  m_gatherSendBuf.size(), INT,
                        &m_gatherRecvBuf[0],  &m_recvcnt[0], &m_displs[0], INT,
                        m_root, GroupWorld, &m_functor );
}

void TestDriver::gathervReturn( )
{
    if ( my_rank == m_root ) {
        m_dbg.verbose(CALL_INFO,1,0,"\n");
        for ( unsigned int i = 0; i < m_gatherRecvBuf.size(); i++ ) {
            m_dbg.verbose(CALL_INFO,1,0,"%#x\n",m_gatherRecvBuf[i]);
        }
    }
}

// ALLGATHER
void TestDriver::allgatherEnter( )
{
    m_dbg.verbose(CALL_INFO,1,0,"my_rank=%d\n", my_rank);
    assert( my_size != 0 ); 

    m_gatherRecvBuf.resize(m_bufLen * my_size);
    m_gatherSendBuf.resize(m_bufLen);

    for ( unsigned int i = 0; i < m_gatherSendBuf.size(); i++ ) {
        m_gatherSendBuf[ i ] = my_rank + 0xbeef0000; 
    }

    m_hermes->allgather( &m_gatherSendBuf[0],  m_gatherSendBuf.size(), INT,
                    &m_gatherRecvBuf[0],  m_gatherRecvBuf.size()/my_size, INT,
                    GroupWorld, &m_functor );
}

void TestDriver::allgatherReturn( )
{
    for ( unsigned int i = 0; i < m_gatherRecvBuf.size(); i++ ) {
        m_dbg.verbose(CALL_INFO,1,0,"%#x\n",m_gatherRecvBuf[i]);
    }
}

// ALLGATHERV
void TestDriver::allgathervEnter( )
{
    assert( my_size != 0 ); 

    m_recvcnt.resize( my_size );
    m_displs.resize( my_size );

#if 0
    m_gatherRecvBuf.resize(m_bufLen * my_size);
    m_gatherSendBuf.resize(m_bufLen);
#endif
    m_gatherSendBuf.resize(my_rank + 1);
    int tmp = 0;
    for ( int i = 0; i < my_size; i++ ) {
        tmp += (i + 1);
    }
    m_gatherRecvBuf.resize(tmp);
    
    m_dbg.verbose(CALL_INFO,1,0,"my_rank=%d sendLen=%lu recvLen=%lu\n", 
                    my_rank, m_gatherSendBuf.size(),m_gatherRecvBuf.size() );

    m_dbg.verbose(CALL_INFO,1,0,"sendBuf %p, recvBuf %p\n",
             &m_gatherSendBuf[0],&m_gatherRecvBuf[0]);
    for ( int next = 0, i = 0; i < my_size; i++ ) {
        m_recvcnt[i] = i + 1; 
; 
        m_displs[i] = next; 
        next += m_recvcnt[i] * m_hermes->sizeofDataType(INT); 
#if 0
        m_recvcnt[i] = m_bufLen; 
        m_displs[i] = (m_bufLen * m_hermes->sizeofDataType(INT)) * i; 
#endif
        m_dbg.verbose(CALL_INFO,1,0,"rank=%d ptr %p cnt=%i\n", i,
                    &m_gatherRecvBuf[0] + m_displs[i], m_recvcnt[i]);
    }

    for ( unsigned int i = 0; i < m_gatherSendBuf.size(); i++ ) {
        m_gatherSendBuf[ i ] = ((my_rank + 1) )<< 16 | (i+1); 
    }

    m_hermes->allgatherv( &m_gatherSendBuf[0],  m_gatherSendBuf.size(), INT,
                        &m_gatherRecvBuf[0],  &m_recvcnt[0], &m_displs[0], INT,
                        GroupWorld, &m_functor );
}

void TestDriver::allgathervReturn( )
{
    for ( unsigned int i = 0; i < m_gatherRecvBuf.size(); i++ ) {
        m_dbg.verbose(CALL_INFO,1,0,"%#x\n",m_gatherRecvBuf[i]);
    }
}
