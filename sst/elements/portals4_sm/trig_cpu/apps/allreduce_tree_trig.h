// Copyright 2009-2010 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2009-2010, Sandia Corporation
// All rights reserved.
// 
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef COMPONENTS_TRIG_CPU_ALLREDUCE_TREE_TRIGGERED_H
#define COMPONENTS_TRIG_CPU_ALLREDUCE_TREE_TRIGGERED_H

#include "sst/elements/portals4_sm/trig_cpu/application.h"
#include "sst/elements/portals4_sm/trig_cpu/trig_cpu.h"
#include "sst/elements/portals4_sm/trig_cpu/portals.h"

class allreduce_tree_triggered :  public application {
public:
    allreduce_tree_triggered(trig_cpu *cpu, bool nary) : application(cpu), init(false)
    {
        radix = cpu->getRadix();
        ptl = cpu->getPortalsHandle();

        if (nary) {
            boost::tie(my_root, my_children) = buildNaryTree(radix);
        } else {
            boost::tie(my_root, my_children) = buildBinomialTree(radix);
        }
        num_children = my_children.size();

        in_buf = 1;
        out_buf = 0;
        tmp_buf = 0;
        zero_buf = 0;
    }

    bool
    operator()(Event *ev)
    {
        ptl_md_t md;
        ptl_me_t me;

        crBegin();

        if (!init) {
            // setup md handles
            ptl->PtlCTAlloc(PTL_CT_OPERATION, up_tree_ct_h);
            crReturn();
            me.start = &tmp_buf;
            me.length = 8;
            me.ignore_bits = ~0x0;
            me.ct_handle = up_tree_ct_h;
            ptl->PtlMEAppend(PT_UP, me, PTL_PRIORITY_LIST, NULL, up_tree_me_h);
            crReturn();

            md.start = &tmp_buf;
            md.length = 8;
            md.eq_handle = PTL_EQ_NONE;
            md.ct_handle = PTL_CT_NONE;
            ptl->PtlMDBind(md, &up_tree_md_h);
            crReturn();

            md.start = &zero_buf;
            md.length = 8;
            md.eq_handle = PTL_EQ_NONE;
            md.ct_handle = PTL_CT_NONE;
            ptl->PtlMDBind(md, &zero_md_h);
            crReturn();

            init = true;
        }

        // 200ns startup time
        start_time = cpu->getCurrentSimTimeNano();
        cpu->addBusyTime("200ns");
        crReturn();

        // Create description of user buffer.  We can't possibly have
        // a result to need this information before we add our portion
        // to the result, so this doesn't need to be persistent.
        ptl->PtlCTAlloc(PTL_CT_OPERATION, user_ct_h);
        crReturn();
        me.start = &out_buf;
        me.length = 8;
        me.ignore_bits = ~0x0;
        me.ct_handle = user_ct_h;
        ptl->PtlMEAppend(PT_DOWN, me, PTL_PRIORITY_LIST, NULL, user_me_h);
        crReturn();

        md.start = &out_buf;
        md.length = 8;
        md.eq_handle = PTL_EQ_NONE;
        md.ct_handle = PTL_CT_NONE;
        ptl->PtlMDBind(md, &user_md_h);
        crReturn();

        out_buf = in_buf;

        if (num_children == 0) {
            // leaf node - push directly to the upper level's up tree
            ptl->PtlAtomic(user_md_h, 0, 8, 0, my_root, PT_UP, 0, 0, NULL, 0, PTL_SUM, PTL_DOUBLE);
            crReturn();
        } else {
            // add our portion to the mix
            ptl->PtlAtomic(user_md_h, 0, 8, 0, my_id, PT_UP, 0, 0, NULL, 
                           0, PTL_SUM, PTL_DOUBLE);
            crReturn();
            if (my_root == my_id) {
                // setup trigger to move data to right place, then send
                // data out of there down the tree
                ptl->PtlTriggeredPut(up_tree_md_h, 0, 8, 0, my_id, PT_DOWN, 0, 0, NULL, 
                                     0, up_tree_ct_h, num_children + 1);
                crReturn();
            } else {
                // setup trigger to move data up the tree when we get enough updates
                ptl->PtlTriggeredAtomic(up_tree_md_h, 0, 8, 0, my_root, PT_UP,
                                        0, 0, NULL, 0, PTL_SUM, PTL_DOUBLE,
                                        up_tree_ct_h, num_children + 1);
                crReturn();
            }

            // and to clean up after ourselves
            ptl->PtlTriggeredPut(zero_md_h, 0, 8, 0, my_id, PT_UP, 0, 0, NULL, 
                                 0, up_tree_ct_h, num_children + 1);
            crReturn();
            ptl->PtlTriggeredCTInc(up_tree_ct_h, -(num_children + 2), up_tree_ct_h, num_children + 2);
            crReturn();

            // push down the tree
            for (i = 0 ; i < num_children ; ++i) {
                ptl->PtlTriggeredPut(user_md_h, 0, 8, 0, my_children[i], PT_DOWN,
                                     0, 0, NULL, 0, user_ct_h, 1);
                crReturn();
            }
        }

        while (!ptl->PtlCTWait(user_ct_h, 1)) { crReturn(); }

        ptl->PtlMEUnlink(user_me_h);
        crReturn();
        ptl->PtlCTFree(user_ct_h);
        crReturn();
        trig_cpu::addTimeToStats(cpu->getCurrentSimTimeNano()-start_time);

        assert(out_buf == (uint64_t) cpu->getNumNodes());

        crFinish();
        return true;
    }

private:
    allreduce_tree_triggered();
    allreduce_tree_triggered(const application& a);
    void operator=(allreduce_tree_triggered const&);

    portals *ptl;
    SimTime_t start_time;
    int i;
    bool init;
    int radix;

    int my_root;
    std::vector<int> my_children;
    int num_children;

    uint64_t in_buf, out_buf, tmp_buf, zero_buf;

    ptl_handle_ct_t ct_handle;

    ptl_handle_ct_t up_tree_ct_h;
    ptl_handle_me_t up_tree_me_h;
    ptl_handle_md_t up_tree_md_h;

    ptl_handle_ct_t user_ct_h;
    ptl_handle_me_t user_me_h;
    ptl_handle_md_t user_md_h;

    ptl_handle_md_t zero_md_h;

    static const int PT_UP = 0;
    static const int PT_DOWN = 1;
};

#endif // COMPONENTS_TRIG_CPU_ALLREDUCE_TREE_TRIGGERED_H
