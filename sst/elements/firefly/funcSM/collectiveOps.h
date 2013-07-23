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

#ifndef COMPONENTS_FIREFLY_FUNCSM_COLLECTIVEOPS_H
#define COMPONENTS_FIREFLY_FUNCSM_COLLECTIVEOPS_H

#include "sst/elements/hermes/msgapi.h"

namespace SST {
namespace Firefly {

template< class T >
T min( T x, T y )
{
    return x < y ? x : y; 
}

template< class T >
T max( T x, T y )
{
    return x > y ? x : y; 
}

template< class T >
T sum( T x, T y )
{
    return x + y;
}

template< class T>
T doOp( T x, T y, Hermes::ReductionOperation op )
{
    T retval;
    switch( op ) {
      case Hermes::SUM: 
        retval = sum( x, y );
        break;
      case Hermes::MIN: 
        retval = min( x, y );
        break;
      case Hermes::MAX: 
        retval = max( x, y );
        break;
    } 
    return retval;
} 

template< class T >
void collectiveOp( T* input[], int numIn, T result[],
                    int count, Hermes::ReductionOperation op )
{
         
    for ( int c = 0; c < count; c++ ) {
        result[c] = input[0][c];
        for ( int n = 1; n < numIn; n++ ) {
            result[c] = doOp( result[c], input[n][c], op );  
        }  
    } 
}

inline void collectiveOp( void* input[], int numIn, void* result, int count, 
        Hermes::PayloadDataType dtype, Hermes::ReductionOperation op )
{
    switch ( dtype  ) {
      case Hermes::CHAR: 

            collectiveOp( (char**)(input), numIn, static_cast<char*>(result),
                        count, op );
            break;
      case Hermes::INT:
            collectiveOp( (int**)(input), numIn, static_cast<int*>(result),
                        count, op );
            break;
      case Hermes::LONG:
            collectiveOp( (long**)(input), numIn, static_cast<long*>(result),
                        count, op );
            break;
      case Hermes::DOUBLE:
            collectiveOp( (double**)(input), numIn,static_cast<double*>(result),
                        count, op );
            break;
      case Hermes::FLOAT:
            collectiveOp( (float**)(input), numIn, static_cast<float*>(result),
                        count, op );
            break;
      case Hermes::COMPLEX:
            assert(0);
            break;
    }
}

}
}

#endif
