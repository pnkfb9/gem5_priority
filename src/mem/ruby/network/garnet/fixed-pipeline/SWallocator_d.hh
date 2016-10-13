/*
 * Copyright (c) 2008 Princeton University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Niket Agarwal
 */

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_SW_ALLOCATOR_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_SW_ALLOCATOR_D_HH__

#include <iostream>
#include <vector>
#include <deque>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"

#include "debug/RubyPowerGatingSA.hh"

class Router_d;
class InputUnit_d;
class OutputUnit_d;

class SWallocator_d : public Consumer
{
  public:
    SWallocator_d(Router_d *router);
    void wakeup();
    void init();
    void clear_request_vector();
    void check_for_wakeup();
    int get_vnet (int invc);
    void print(std::ostream& out) const {};
    void arbitrate_inports();
    void arbitrate_outports();
    bool is_candidate_inport(int inport, int invc);

    inline double
    get_local_arbit_count()
    {
        return m_local_arbiter_activity;
    }
    inline double
    get_global_arbit_count()
    {
        return m_global_arbiter_activity;
    }
    
    inline double
    get_local_arbit_count_resettable()
    {
        return m_local_arbiter_activity_resettable;
    }
    inline double
    get_global_arbit_count_resettable()
    {
        return m_global_arbiter_activity_resettable;
    }
    
    inline void
    reset_arbit_count()
    {
        m_local_arbiter_activity_resettable=0;
        m_global_arbiter_activity_resettable=0;
    }

    inline bool
    get_wh_path_busy(int inport, int invc, int outport)
    {
         return m_wh_path_busy[inport][invc][outport];
    }

    inline void
    set_wh_path_busy(bool set, int inport, int invc, int outport)
    {
         m_wh_path_busy[inport][invc][outport] = set;
    }

	void get_invc_priority(int inport_iter,int invc_iter);	//added by Panc for priority scheduling
	std::deque<std::pair<int,int>> get_priority_vector();

  private:
    int m_num_inports, m_num_outports;
    int m_num_vcs, m_vc_per_vnet;

    double m_local_arbiter_activity, m_global_arbiter_activity;
    double m_local_arbiter_activity_resettable, m_global_arbiter_activity_resettable;

    Router_d *m_router;
    std::vector<int> m_round_robin_outport;
    std::vector<int> m_round_robin_inport;
    std::vector<std::vector<bool> > m_port_req;
    std::vector<std::vector<int> > m_vc_winners; // a list for each outport
    std::vector<InputUnit_d *> m_input_unit;
    std::vector<OutputUnit_d *> m_output_unit;

	std::vector<std::vector<int>> m_priority_invc;		//added by Panc for priority scheduling
	
	//to implement the fast switch when resynchs can block some links
	std::vector<bool> m_outport_busy;
	std::vector<bool> m_inport_credit_busy;
	// used for the wormhole flow in a vc router, allowing a per packet SA
	// instead of a RR on a per flit basis
	//
	// {inport|invc|outport|outvc}
	std::vector<std::vector<std::vector<bool>>> m_wh_path_busy;
	void checkAndReleaseReservedPath();

	//BUBBLE Check Function
   void checkBubbleNextCycle(int inport, int invc);
};

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_SW_ALLOCATOR_D_HH__
