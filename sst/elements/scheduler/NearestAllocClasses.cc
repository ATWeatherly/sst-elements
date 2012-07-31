// Copyright 2011 Sandia Corporation. Under the terms                          
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.             
// Government retains certain rights in this software.                         
//                                                                             
// Copyright (c) 2011, Sandia Corporation                                      
// All rights reserved.                                                        
//                                                                             
// This file is part of the SST software package. For license                  
// information, see the LICENSE file in the top level directory of the         
// distribution.                                                               
/*
 *  to implement allocation algorithms of the family that
 * includes Gen-Alg, MM, and MC1x1; from each candidate center,
 * consider the closest points, and return the set of closest points
 * that is best.  Members of the family are specified by giving the
 * way to find candidate centers, how to measure "closeness" of points
 * to these, and how to evaluate the sets of closest points.

 GenAlg - try centering at open places;
 select L_1 closest points;
 eval with sum of pairwise L1 distances
 MM - center on intersection of grid in each direction by open places;
 select L_1 closest points
 eval with sum of pairwise L1 distances
 MC1x1 - try centering at open places
 select L_inf closest points
 eval with L_inf distance from center

 This file in particular implements the comparators, point collectors,
 generators, and scorers for use with the nearest allocators
 */


#include <string>
#include <sstream>
#include <vector>

#include "sst/core/serialization/element.h"

#include "NearestAllocClasses.h"

//Center Generators: 

vector<MeshLocation*>* FreeCenterGenerator::getCenters(vector<MeshLocation*>* available) {
  //returns vector containing contents of available (deep copy to match Intersection version)
  vector<MeshLocation*>* retVal = new vector<MeshLocation*>();
  for(unsigned int x = 0; x < available->size(); x++)
    retVal->push_back(new MeshLocation((*available)[x]));
  return retVal;
}

string FreeCenterGenerator::getSetupInfo(bool comment){
  string com;
  if(comment) com="# ";
  else com="";
  return com+"FreeCenterGenerator";
}

bool contains(vector<int>* vec, int i)
{
  bool ret = false;
  for(unsigned int x = 0; x < vec->size() && !ret ; x++)
    if(vec->at(x) == i)
      ret = true;
  return ret;
}


vector<MeshLocation*>* IntersectionCenterGen::getCenters(vector<MeshLocation*>* available) { 
  vector<MeshLocation*>* retVal = new vector<MeshLocation*>();
  //Collect available X,Y and Z coordinates
  vector<int> X;
  vector<int> Y;
  vector<int> Z;

  //no duplicate values
  for (vector<MeshLocation*>::iterator loc = available->begin(); loc != available->end(); ++loc) {
    if(!contains(&X,(*loc)->x))
      X.push_back((*loc)->x);
    if(!contains(&Y,(*loc)->y))
      Y.push_back((*loc)->y);
    if(!contains(&Z,(*loc)->z))
      Z.push_back((*loc)->z);
  }

  //Make all possible intersections of the X,Y and Z coordinates
  for (vector<int>::iterator ind_x = X.begin(); ind_x != X.end(); ind_x++) //ind_x is x val index
    for (vector<int>::iterator ind_y = Y.begin(); ind_y != Y.end(); ind_y++) 
      for (vector<int>::iterator ind_z = Z.begin(); ind_z != Z.end(); ind_z++) {
        //Get an intersection with current x, y and z values
        MeshLocation* val = new MeshLocation(*(ind_x), *(ind_y), *(ind_z) );                                                                  
        retVal->push_back(val); //Add to the return value list
      }
  return retVal;
}

string IntersectionCenterGen::getSetupInfo(bool comment){
  string com;
  if(comment) com="# ";
  else com="";
  return com+"IntersectionCenterGen";
}

//Point Collectors:

vector<MeshLocation*>* L1PointCollector::getNearest(MeshLocation* center, int num,
    vector<MeshLocation*>* available) {
  L1Comparator L1c(center->x, center->y, center->z);
  stable_sort(available->begin(),available->end(), L1c);  //sort available using L1c
  return available;
}

string  L1PointCollector::getSetupInfo(bool comment){
  string com;
  if(comment) com="# ";
  else com="";
  return com+"L1PointCollector";
}

vector<MeshLocation*>* LInfPointCollector::getNearest(MeshLocation* center, int num,
    vector<MeshLocation*>* available) {
  LInfComparator lic(center->x, center->y, center->z);
  stable_sort(available->begin(),available->end(), lic);  //sort available using L1c
  return available;
}

string LInfPointCollector::getSetupInfo(bool comment){
  string com;
  if(comment) 
    com="# ";
  else 
    com="";
  return com+"LInfPointCollector";
}

GreedyLInfPointCollector::PointInfo::PointInfo(MeshLocation* point, int L1toGroup){
  this->point = point;
  this->L1toGroup = L1toGroup;
  this->tieBreaker = 0;
}

bool GreedyLInfPointCollector::PointInfo::operator()(PointInfo* const& pi1, PointInfo* const& pi2){
  //true = 2 > 1
  if (pi1->L1toGroup == pi2->L1toGroup){
    if (pi1->tieBreaker == pi2->tieBreaker){
      return (*pi2->point)(pi1->point, pi2->point);   //TODO how does using pi2 location in the ordering add bias to the final result?
    }
    return (pi2->tieBreaker > pi1->tieBreaker);
  }
  return pi2->L1toGroup > pi1->L1toGroup;
}

string GreedyLInfPointCollector::PointInfo::toString(){
  stringstream ret;
  ret << "{"<<this->point<<","<<this->tieBreaker<<","<<this->L1toGroup<<"}";
  return ret.str();
}

vector<MeshLocation*>* GreedyLInfPointCollector::getNearest(MeshLocation* center, int num,
    vector<MeshLocation*>* available) {
  vector<MeshLocation*>* tempavail = new vector<MeshLocation*>;
  for(unsigned int x = 0; x < available->size(); x++)
    tempavail->push_back(available->at(x));

  LInfComparator lic(center->x, center->y, center->z);
  stable_sort(tempavail->begin(),tempavail->end(), lic);  //sort available using L1c

  //Skip to the outer shell
  int outerIndex = 0;	//The index of the first MeshLocation of the Outermost Shell
  int outerShell = (*tempavail)[0]->LInfDistanceTo(center);	// This gives us the shell number for a MeshLocation
  // one past the last one we would normally use
  for (int i=1; i<num; ++i){
    int newOuterShell = (*tempavail)[i]->LInfDistanceTo(center);
    if (newOuterShell > outerShell){
      outerShell = newOuterShell;
      outerIndex = i;
    }
  }

  //Put all of the Inner Shell's processors together
  vector<MeshLocation*>* innerProcs = new vector<MeshLocation*>();
  for (int i=0; i<outerIndex; ++i){
    innerProcs->push_back((*tempavail)[i]);
  }
  //Put points in the outer shell into PointInfos with L1 distance to rest of group
  vector<PointInfo*>* outerProcs = new vector<PointInfo*>();
  for (unsigned int i=outerIndex;i<tempavail->size();i++){
    PointInfo* outerPoint = new PointInfo((*tempavail)[i], L1toInner((*tempavail)[i],innerProcs));
    outerPoint->tieBreaker = outerPoint->point->L1DistanceTo(center);
    outerProcs->push_back(outerPoint);
  }
  PointInfo PIComp(NULL,0); //a dummy PointInfo that serves as the comparator (as we must give a specific comparator in C++)
  stable_sort(outerProcs->begin(), outerProcs->end(),PIComp );
  //(*(outerProcs->begin()))->point->print();
  //Find the minimum L1toGroup, add it to tempavail
  int totalSelected = innerProcs->size();	//Keeps track of all the processors thus far added
  while (totalSelected < num){
    PointInfo* first = *(outerProcs->begin());
    innerProcs->push_back(first->point);	//This is the current minimum of the set (lowest L1 distance to the rest of the inner procs)
    outerProcs->erase(outerProcs->begin());
    ++totalSelected; 	//Make progress in the loop
    //recalculate all the other distances adding this point into the inner group.
    if ( totalSelected < num){	//TODO bad form?
      for (vector<PointInfo*>::iterator info = outerProcs->begin(); info != outerProcs->end(); info++){
        //printf("info*: %p %p %d\n", *info, (*info)->point, (*info)->L1toGroup);
        if((*info)->L1toGroup < 0)
          exit(0);
        //instead of recalculating the whole thing, we just need to add another length to the total distance
        (*info)->L1toGroup +=  (*info)->point->L1DistanceTo(first->point);
        //printf("info* after change: %p %p %d\n", *info, (*info)->point, (*info)->L1toGroup);
      }
      stable_sort(outerProcs->begin(), outerProcs->end(),PIComp );
    }
    delete first; //the point may still be used but the PointInfo will not
    first = NULL;
  }
  while(!outerProcs->empty())
  {
    delete *outerProcs->begin();
    outerProcs->erase(outerProcs->begin());
  }
  delete outerProcs;
  tempavail->clear();
  delete tempavail;
  return  innerProcs;
}

//loc shouldn't be in innerProcs
int GreedyLInfPointCollector::L1toInner(MeshLocation* outer, vector<MeshLocation*>* innerProcs) {
  int distance = 0;
  for ( vector<MeshLocation*>::iterator inner = innerProcs->begin(); inner != innerProcs->end(); ++inner)
    distance += outer->L1DistanceTo(*inner);
  return distance;
}

string GreedyLInfPointCollector::getSetupInfo(bool comment){
  string com;
  if(comment) com="# ";
  else com="";
  return com+"LInfPointCollector";
}

//Scorers:

string PairwiseL1DistScorer::getSetupInfo(bool comment){
  string com;
  if(comment) com="# ";
  else com="";
  return com+"PairwiseL1DistScorer";
}

//Takes mesh center, available processors sorted by correct comparator,
//and number of processors needed and returns tiebreak value.
long Tiebreaker::getTiebreak(MeshLocation* center, vector<MeshLocation*>* avail, int num, MachineMesh* mesh){
  long ret=0;

  lastTieInfo = "0\t0\t0";

  if (avail->size() == (unsigned int)num)
    return 0;

  LInfComparator* lc = new LInfComparator(center->x,center->y,center->z);
  stable_sort(avail->begin(), avail->end(),*lc);
  delete lc;
  if(maxshells==0)
    return 0;

  long ascore = 0;
  long wscore = 0;
  long bscore = 0;

  long lastshell=center->LInfDistanceTo((*avail)[num-1]);
  long lastlook=lastshell+maxshells;
  lastTieInfo="";
  long ydim = mesh->getYDim();

  //Add to score for nearby available processors.
  if(availFactor != 0) {
    for (unsigned int i=num; i<avail->size(); i++) {
      long dist = center->LInfDistanceTo((*avail)[i]);
      if (dist > lastlook)
        break;
      else{
        ret += availFactor * (lastlook - dist + 1);
        ascore += availFactor * (lastlook - dist + 1);
      }
    }
  }

  //Subtract from score for nearby walls
  if(wallFactor != 0) {
    long xdim = mesh->getXDim();
    long zdim = mesh->getZDim();
    for(int i=0; i<num; i++){
      long dist = center->LInfDistanceTo((*avail)[i]);
      //if(dist == lastshell)
      if( (((*avail)[i]->x == 0 || (*avail)[i]->x == xdim-1) && xdim > 2) || 
          (((*avail)[i]->y == 0 || (*avail)[i]->y == ydim-1) && ydim > 2) || 
          (((*avail)[i]->z == 0 || (*avail)[i]->z == zdim-1) && zdim > 2)) {
        //NOTE: After removing if statement above, is this right?
        wscore -= wallFactor * (lastlook - dist + 1);
        ret -= wallFactor * (lastlook - dist + 1);
      }
    }
  }

  //Subtract from score for bordering allocated processors
  if(borderFactor!=0){
    vector<MeshLocation*>* used = mesh->usedProcessors();
    stable_sort(used->begin(), used->end(),*(new LInfComparator(center->x,center->y,center->z)));
    for(vector<MeshLocation*>::iterator it = used->begin(); it != used->end(); ++it){
      MeshLocation* ml = *it;
      long dist = center->LInfDistanceTo(ml);
      if(dist > lastlook)
        break;
      else if(dist == lastshell+1){
        ret -= borderFactor * (lastlook - dist + 1);
        bscore -= borderFactor * (lastlook - dist + 1);
      }
    }
  }

  //Add to score for being at a worse curve location
  //Only works for 2D now.
  long cscore = 0;
  if(curveFactor!=0){
    long centerLine = center->x/curveWidth;
    long tsc = ydim*centerLine;
    tsc += (centerLine%2 == 0)?(center->y):(ydim-center->y);
    cscore += curveFactor * tsc;
    ret += cscore;
  }

  stringstream sstemp;
  sstemp << lastTieInfo << ascore<<"\t"<<wscore<<"\t"<<bscore<<"\t"<<cscore;
  lastTieInfo = sstemp.str();

  return ret;
}

Tiebreaker::Tiebreaker(long ms, long af, long wf, long bf) {
  maxshells=ms;
  availFactor = af;
  wallFactor = wf;
  borderFactor = bf;

  bordercheck = false;
  curveFactor=0;
  curveWidth=2;
}

string Tiebreaker::getInfo() {
  stringstream ret;
  ret << "("<<maxshells<<","<<availFactor<<","<<wallFactor<<","<<borderFactor<<","<<curveFactor<<","<<curveWidth<<")";
  return ret.str();
}


pair<long,long>* LInfDistFromCenterScorer::valueOf(MeshLocation* center, vector<MeshLocation*>* procs, int num, MachineMesh* mach) {
  //returns the sum of the LInf distances of the num closest processors

  long retVal = 0;
  for (int i = 0; i < num; i++) 
    retVal += center->LInfDistanceTo((*procs)[i]);

  long tiebreak = tiebreaker->getTiebreak(center,procs,num, mach);

  return new pair<long,long>(retVal,tiebreak);
}

LInfDistFromCenterScorer::LInfDistFromCenterScorer(Tiebreaker* tb){
  tiebreaker=tb;
}


string  LInfDistFromCenterScorer::getSetupInfo(bool comment){
  string com;
  stringstream ret;
  if(comment) com="# ";
  else com="";
  ret << com<<"LInfDistFromCenterScorer (Tiebreaker: "<<tiebreaker->getInfo()<<")";
  return ret.str();
}

