// Copyright 2011 Sandia Corporation-> Under the terms                          
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U->S->             
// Government retains certain rights in this software->                         
//                                                                             
// Copyright (c) 2011, Sandia Corporation                                      
// All rights reserved->                                                        
//                                                                             
// This file is part of the SST software package-> For license                  
// information, see the LICENSE file in the top level directory of the         
// distribution->                                                               
/**
 * By default the MBSAllocator provides a layered 2D mesh approach to
 * the Multi Buddy Strategy
 * A Note on Extending:  The only thing you need to do is override the initialize method,
 * create complete blocks, and make sure the "root" blocks are in the FBR->
 */

#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <time.h>
#include <math.h>

#include "sst/core/serialization/element.h"

#include "MBSAllocator.h"
#include "Machine.h"
#include "MachineMesh.h"
#include "AllocInfo.h"
#include "MBSAllocInfo.h"
#include "Job.h"
#include "misc.h"

#define MIN(a,b)  ((a)<(b)?(a):(b))
#define DEBUG false


using namespace SST::Scheduler;


MBSAllocator::MBSAllocator(Machine* mach){
  //this constructor doesn't call initialize() and is for derived classes
  MachineMesh* m = dynamic_cast<MachineMesh*>(mach);
  if(m== NULL)
    error("MBS Allocator requires a mesh machine");
  meshMachine = m; //make us happy
  machine = m;     //make Allocator happy
  FBR = new vector<set<Block*,Block>*>();
  ordering = new vector<int>();
}

MBSAllocator::MBSAllocator(MachineMesh* m, int x, int y, int z){
  meshMachine = m; //make us happy
  machine = m;     //make Allocator happy
  FBR = new vector<set<Block*,Block>*>();
  ordering = new vector<int>();

  //create the starting blocks
  initialize(new MeshLocation(x,y,z),new MeshLocation(0,0,0));
  if (DEBUG) printFBR("Post Initialize:");
}

MBSAllocator::MBSAllocator(vector<string>* params, Machine* mach){

  MachineMesh* m = dynamic_cast<MachineMesh*>(mach);
  if(m== NULL)
    error("MBS Allocator requires a mesh machine");
  meshMachine = m; //make us happy
  machine = m;     //make Allocator happy
  FBR = new vector<set<Block*,Block>*>();
  ordering = new vector<int>();

  //create the starting blocks
  initialize(
      new MeshLocation(m->getXDim(),m->getYDim(),m->getZDim()), 
      new MeshLocation(0,0,0));

  if (DEBUG) printFBR("Post Initialize:");
}

string MBSAllocator::getSetupInfo(bool comment){
  string com;
  if(comment) com="# ";
  else com="";
  return com+"Multiple Buddy Strategy (MBS) Allocator";
}

string MBSAllocator::getParamHelp(){
  return "";
}

/**
 * Initialize will fill in the FBR with z number of blocks (1 for
 * each layer) that fit into the given x,y dimensions.  It is
 * assumed that those dimensions are non-zero.
 */
void MBSAllocator::initialize(MeshLocation* dim, MeshLocation* off){
  if (DEBUG) printf("Initializing a %dx%dx%d region at %s\n", dim->x, dim->y, dim->z, off->toString().c_str());

  //Figure out the largest possible block possible
  int maxSize = (int) (log((double) MIN(dim->x,dim->y))/log(2.0));
  int sideLen = (int) (1 << maxSize); //supposed to be 2^maxSize
  //create a flat square
  MeshLocation* blockDim = new MeshLocation(sideLen,sideLen,1);
  int size = blockDim->x*blockDim->y;
  size *= blockDim->z;

  //see if we have already made one of these size blocks
  int rank = distance(ordering->begin(), find(ordering->begin(), ordering->end(), size));
  if(rank == (int)ordering->size()){
    rank = createRank(size);
  }

  //add block to the set at the given rank, determined by lookup
  for(int i=0; i<dim->z; i++){
    Block* block = new Block(new MeshLocation(off->x,off->y,i),new MeshLocation(blockDim->x,blockDim->y,blockDim->z)); 
    FBR->at(rank)->insert(block);
    createChildren(block);

    //update the rank (createChildren may have added new ranks to ordering and FBR)
    rank = distance(ordering->begin(), find(ordering->begin(), ordering->end(), size));
  }

  //initialize the two remaining rectangles of the region
  if(dim->x - sideLen > 0)
    initialize(new MeshLocation(dim->x - sideLen,dim->y,dim->z),new MeshLocation(off->x+sideLen,off->y,1));
  if(dim->y - sideLen > 0)
    initialize(new MeshLocation(sideLen,dim->y-sideLen,dim->z),new MeshLocation(off->x,off->y+sideLen,1));
  delete blockDim;
  
}

/*
 * Creates a rank in both the FBR, and in the ordering.
 * If a rank already exists, it does not create a new rank,
 * it just returns the one already there
 */

int MBSAllocator::createRank(int size){
  vector<int>::iterator it = find(ordering->begin(), ordering->end(), size);
  int i = distance(ordering->begin(),it); 
  if (it != ordering->end())
    return i;


  vector<set<Block*,Block>*>::iterator FBRit = FBR->begin();
  i = 0;
  for (it=ordering->begin();it!=ordering->end() && *it < size; it++){
    i++;
    FBRit++;
  }

  //add this block size into the ordering
  ordering->insert(it,size);

  //make our corresponding set
  Block* BComp = new Block();
  FBR->insert(FBRit, new set<Block*, Block>(*BComp));
  delete BComp;

  if (DEBUG) printf("Added a rank %d for size %d\n", i, size);

  return i;
}

/**
 *  Essentially this will reinitialize a block, except add the
 *  children to the b->children, then recurse
 */
void MBSAllocator::createChildren(Block* b){
  set<Block*, Block>* childrenset = splitBlock(b);
  set<Block*, Block>::iterator children = childrenset->begin();
  Block* next;

  if (DEBUG) printf("Creating children for %s :: ", b->toString().c_str());

  while (children != childrenset->end()){
    next = *children;

    if (DEBUG) printf("%s ", next->toString().c_str());

    b->addChild(next);

    //make sure the proper rank exists, in both ordering and FBR
    int size = next->dimension->x * next->dimension->y*next->dimension->z;
    createRank(size);

    if(next->size() > 1)
      createChildren(next);
    children++;
  }
  if (DEBUG) printf("\n");
}

set<Block*, Block>* MBSAllocator::splitBlock (Block* b) {
  //create the set to iterate over
  Block* BCComp = new Block();
  set<Block*, Block>* children = new set<Block*, Block>(*BCComp);
  delete BCComp;

  //determine the size (blocks should be cubes, thus dimension->x=dimension->y)
  int size = (int) (log((double) b->dimension->x)/log(2));
  //we want one size smaller, but they need to be
  if(size-1 >= 0){
    int sideLen =  1 << (size-1);
    MeshLocation* dim = new MeshLocation(sideLen,sideLen,1 /*sideLen*/);

    children->insert(new Block(new MeshLocation(b->location->x, b->location->y, b->location->z), dim,b));
    children->insert(new Block(new MeshLocation(b->location->x, b->location->y+sideLen, b->location->z), dim,b));
    children->insert(new Block(new MeshLocation(b->location->x+sideLen, b->location->y+sideLen, b->location->z), dim,b));
    children->insert(new Block(new MeshLocation(b->location->x+sideLen, b->location->y, b->location->z), dim,b));
  }
  if (DEBUG) printf("Made blocks for splitBlock(%s)\n", b->toString().c_str());
  return children;
}

MBSMeshAllocInfo* MBSAllocator::allocate(Job* job){
  if (DEBUG) printf("Allocating %s\n",job->toString().c_str());

  MBSMeshAllocInfo* retVal = new MBSMeshAllocInfo(job);
  int allocated = 0;

  //a map of dimensions to numbers
  map<int,int>* RBR = factorRequest(job);

  while(allocated < job->getProcsNeeded()){
    //Start trying allocate the largest blocks
    if(RBR->empty())
      error("RBR empty in allocate()");
    int currentRank = RBR->rbegin()->first; //this gives us the largest key in RBR

    //see if there is a key like that in FBR->
    if(FBR->at(currentRank)->size() > 0){  //TODO: try/catch for error?
      //Move the block from FBR to retVal
      Block* newBlock = *(FBR->at(currentRank)->begin());
      retVal->blocks->insert(newBlock);
      FBR->at(currentRank)->erase(newBlock);

      //add all the processors to retVal, and make progress
      //in the loop
      set<MeshLocation*, MeshLocation>* newBlockprocs = newBlock->processors();
      set<MeshLocation*, MeshLocation>::iterator it = newBlockprocs->begin();
      //processors() is sorted by MeshLocation comparator
      for (int i=allocated;it!= newBlockprocs->end();i++){
        retVal->processors->at(i) = *(it);
        retVal->nodeIndices[i] = (*it)->toInt((MachineMesh*)machine);
        it++;
        allocated++;
      }
      int currentval = RBR->find(currentRank)->second;
      //also be sure to remove the allocated block from the RBR
      if (currentval -1 > 0){
        RBR->erase(currentRank);
        RBR->insert(pair<int, int>(currentRank,currentval-1));
      } else {
        RBR->erase(currentRank);
      }
    } else {
      //See if there is a larger one we can split up
      if(!splitLarger(currentRank)){
        //since we were unable to split a larger block, make request smaller
        splitRequest(RBR,currentRank);

        //if there are non left to request at the current rank, clean up the RBR
        if(RBR->find(currentRank)->second <= 0){
          RBR->erase(currentRank);

          //make sure we look at the next lower rank (commented out in the Java)
          //currentRank = currentRank - 1;
        }

      }
      if (DEBUG) printFBR("After all splitting");
    }
  }
  RBR->clear();
  delete RBR;
  return retVal;
}

/**
 * Calculates the RBR, which is a map of ranks to number of blocks at that rank
 */
map<int,int>* MBSAllocator::factorRequest(Job* j){
  map<int,int>* retVal = new map<int,int>();
  int procs = 0;

  while (procs < j->getProcsNeeded()){
    //begin our search
    vector<int>::iterator sizes = ordering->begin();

    //look for the largest size block that fits the procs needed
    int size = -1;
    int prevSize = -1;
    while(sizes != ordering->end()){
      prevSize = size;
      size = *(sizes);
      sizes++;
      if(size > (j->getProcsNeeded() - procs)){
        //cancel the progress made with this run through the loop
        size = prevSize;
        break;
      }
    }
    //make sure something was done
    if(prevSize == -1 || size == -1){
      //catch the special case where we only have one size
      if(ordering->size() == 1){
        size = ordering->at(0);
      } else {
        error("while loop never ran in MBSAllocator");
      }
    }

    //get the rank
    int rank = distance(ordering->begin(), find(ordering->begin(),ordering->end(), size));
    if(retVal->find(rank) == retVal->end()){
      retVal->insert(pair<int,int>(rank,0));
    }

    //increment that value of the map
    retVal->find(rank)->second++;

    //make progress in the larger while loop
    procs += ordering->at(rank);
  }

  if (DEBUG){
    printf("Factored request: \n");
    printRBR(retVal);
  }
  return retVal;
}

/**
 * Breaks up a request for a block with a given rank into smaller request if able
 */

  void MBSAllocator::splitRequest(map<int,int>* RBR, int rank){
    if (RBR->count(rank) == 0)
      error("Out of bounds in MBSAllocator::splitRequest()");
    if (rank <= 0)
      error("Cannot split a request of size 0");
    if (RBR->find(rank)->second == 0){
      //throw new UnsupportedOperationException("Cannot split a block of size 0");
      error("Cannot split a block of size 0");
      return;
    }

    //decrement the current rank
    RBR->find(rank)->second--;

    //get the number of blocks we need from the previous rank
    int count = (int) ordering->at(rank)/ordering->at(rank-1);

    //increment the previous rank, and if it doesn't exists create it
    if(RBR->find(rank-1) != RBR->end()) {
      RBR->find(rank-1)->second += count;
    } else {
      RBR->insert(pair<int,int>(rank-1,count));
    }


    if (DEBUG){
      printf("Split a request up\n");
      printRBR(RBR);

    }
  }

/**
 * Determines whether a split up of a possible larger block was
 * successful->  It begins looking at one larger than rank->
 */
bool MBSAllocator::splitLarger(int rank){
  if (DEBUG) printf("Splitting a block at rank %d\n",rank);

  //make sure that we can search in rank+1
  //FBR has same size as ordering
  if (rank+1 >= (int)FBR->size())
    return false;

  //pass off the work
  if(FBR->at(rank+1)->size() == 0){
    //recurse! if necessary
    if(!splitLarger(rank+1))
      return false;
  }

  //split a block since by this point in the method we have guaranteed its existence
  Block* toSplit = *(FBR->at(rank+1)->begin());
  set<Block*, Block>::iterator spawn = toSplit->getChildren()->begin();

  //add children to the FBR
  while (spawn != toSplit->getChildren()->end()){
    FBR->at(rank)->insert(*spawn);
    spawn++;
  }

  //remove toSplit from the FBR
  FBR->at(rank+1)->erase(toSplit);

  return true;
}

void MBSAllocator::deallocate(AllocInfo* alloc){
  if (DEBUG) printf("Deallocating job with %d procs\n",alloc->job->getProcsNeeded());
  //check to make sure it is a MBSMeshAllocInfo->->->                        
  if (dynamic_cast<MBSMeshAllocInfo*>(alloc) == NULL){
    error("MBS allocator can only deallocate instances of MBSMeshAllocInfo");
  } else {
    unallocate( (MBSMeshAllocInfo*) alloc);
  }
}

void MBSAllocator::unallocate(MBSMeshAllocInfo* info){
  //add all blocks back into the FBR
  for(set<Block*,Block>::iterator b = info->blocks->begin(); b != info->blocks->end(); b++){
    int rank = distance(ordering->begin(), find(ordering->begin(), ordering->end(), (*b)->size()));
    FBR->at(rank)->insert(*b);
  }
  //for each block see if its parent is all free
  for(set<Block*,Block>::iterator b = info->blocks->begin(); b != info->blocks->end(); b++){
    mergeBlock((*b)->parent);
  }
  //delete info;
}

void MBSAllocator::mergeBlock(Block* p){
  if(p == NULL)
    return;

  //make sure p isn't in FBR
  int rank = distance(ordering->begin(), find(ordering->begin(), ordering->end(), p->size()));
  if(FBR->at(rank)->count(p) == 1)
    return;

  //see if children are in the FBR
  for(set<Block*, Block>::iterator child = p->children->begin(); child != p->children->end(); child++){
    rank = distance(ordering->begin(), find(ordering->begin(), ordering->end(), (*child)->size()));
    if(FBR->at(rank)->count(*child) == 0)
      return;
  }
  //by this point in the code they all are
  for(set<Block*, Block>::iterator child = p->children->begin(); child != p->children->end(); child++){
    rank = distance(ordering->begin(), find(ordering->begin(), ordering->end(), (*child)->size()));
    FBR->at(rank)->erase(*child);
  }
  rank = distance(ordering->begin(), find(ordering->begin(), ordering->end(), p->size()));
  FBR->at(rank)->insert(p);
  //recurse!
  mergeBlock(p->parent);
}

void MBSAllocator::printRBR(map<int,int>* RBR){
  for(map<int,int>::iterator key = RBR->begin(); key != RBR->end(); key++)
    printf("Rank %d has %d requested blocks\n", key->first, key->second);
}

void MBSAllocator::printFBR(string msg){
  printf("%s\n",msg.c_str());
  if(ordering->size() != FBR->size())
    error("Ordering vs FBR size mismatch");
  for (int i=0;i<(int)ordering->size();i++){
    printf("Rank: %d for size %d\n", i, ordering->at(i));
    set<Block*, Block>::iterator it = FBR->at(i)->begin();
    while (it != FBR->at(i)->end()){
      printf("  %s\n",(*it)->toString().c_str()); 
      it++;
    }
  }
}

string MBSAllocator::stringFBR(){
  stringstream retVal;
  if(ordering->size() != FBR->size())
    error("Ordering vs FBR size mismatch");
  for (int i=0;i<(int)ordering->size();i++){
    retVal << "Rank: "<<i<<" for size "<<ordering->at(i)<<"\n";
    set<Block*, Block>::iterator it = FBR->at(i)->begin();
    while (it != FBR->at(i)->end()){
      retVal << "  " << (*it)->toString() << "\n";
      ++it;
    }
    it = FBR->at(i)->begin();
    while (it != FBR->at(i)->end()){
      retVal << "  " << (*it)->toString(); //the Java does not have toString
      ++it;
    }
  }
  return retVal.str();
}

