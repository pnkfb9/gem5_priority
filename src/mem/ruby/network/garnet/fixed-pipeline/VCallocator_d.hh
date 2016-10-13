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

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_VC_ALLOCATOR_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_VC_ALLOCATOR_D_HH__

#include <iostream>
#include <fstream>
#include <utility>
#include <vector>
#include <deque>
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flit_d.hh"

#include "config/use_speculative_va_sa.hh"
	
using namespace std;
class Router_d;
class InputUnit_d;
class OutputUnit_d;

class VCallocator_d : public Consumer
{
  public:
    VCallocator_d(Router_d *router);
    virtual void init();
    void wakeup();
    void check_for_wakeup();
    void clear_request_vector();
    int get_vnet(int invc);
    void print(std::ostream& out) const {}
    virtual void arbitrate_invcs();
    virtual void arbitrate_outvcs();
    bool is_invc_candidate(int inport_iter, int invc_iter);
    virtual void select_outvc(int inport_iter, int invc_iter);
	 
    	void get_invc_priority(int inport_iter,int invc_iter);
	void check_for_multiple_vc_max_priority();
	std::vector<int> get_first_vc_by_priority();
	void write_stat_on_file(std::string ss);
	void write_priorities_on_file();
	std::deque<std::pair<int,int>> get_priority_vector();

    inline double
    get_local_arbit_count(int vnet)
    {
        return m_local_arbiter_activity[vnet];
    }

    inline double
    get_global_arbit_count(int vnet)
    {
        return m_global_arbiter_activity[vnet];
    }
    
    inline double
    get_local_arbit_count_resettable(int vnet)
    {
        return m_local_arbiter_activity_resettable[vnet];
    }

    inline double
    get_global_arbit_count_resettable(int vnet)
    {
        return m_global_arbiter_activity_resettable[vnet];
    }

    inline void
    reset_arbit_count()
    {
        //for(auto& i : m_local_arbiter_activity_resettable) i=0;
        //for(auto& i : m_global_arbiter_activity_resettable) i=0;
		
 		for(auto i=m_local_arbiter_activity_resettable.begin();i!=m_local_arbiter_activity_resettable.end();i++) *i=0;
 		for(auto i=m_global_arbiter_activity_resettable.begin();i!=m_global_arbiter_activity_resettable.end();i++) *i=0;    
	}
    
  protected:
    int m_num_vcs, m_vc_per_vnet;
    int m_num_inports;
    int m_num_outports;

    std::vector<double > m_local_arbiter_activity;
    std::vector<double > m_global_arbiter_activity;
    std::vector<double > m_local_arbiter_activity_resettable;
    std::vector<double > m_global_arbiter_activity_resettable;

    Router_d *m_router;

    // First stage of arbitration
    // where all vcs select an output vc to contend for
    std::vector<std::vector<int> > m_round_robin_invc;
    std::vector<std::vector<int> > m_priority_invc;	//vector containing all the priorities
    std::deque<std::pair<int,int>> m_max_prio_rr;			//deque that contains pair (inport,invc) corresponding (if any) the pairs with maximum equal priority 
    int m_prio_rr_counter,m_last_max_priority; 
   ofstream m_va_tracer;
    // Arbiter for every output vc
    std::vector<std::vector<std::pair<int, int> > > m_round_robin_outvc;

    // [outport][outvc][inport][invc]
    // set true in the first phase of allocation
    std::vector<std::vector<std::vector<std::vector<bool> > > > m_outvc_req;

    std::vector<std::vector<bool> > m_outvc_is_req;

    std::vector<InputUnit_d *> m_input_unit;
    std::vector<OutputUnit_d *> m_output_unit;
	
	std::vector<std::vector<int> > vnet_in_this_vc_outport;
	int m_virtual_networks_protocol;
	int m_virtual_networks_spurious;
  public:
	std::vector<std::vector<int> >& getVnetInThisVcOutport(){return vnet_in_this_vc_outport;}

#if USE_SPECULATIVE_VA_SA==1
	//SPECULATIVE: Add support vector to SWallocator speculation
  public:
 //SPECULATIVE function prototype
    bool spec_SWallocator(int inport, int invc, int outport, int outvc);
    bool spec_SWallocator_arbitrate_inports(int inport, int invc, int outport, int outvc);
    bool spec_SWallocator_arbitrate_outport(int inporti, int invc, int outport, int outvc);
    bool spec_SWallocator_is_candidate_inport(int inport, int invc);
    void spec_SWallocator_setup();
    void spec_SWallocator_startup();
    void spec_SWallocator_clear_request_vector();	
  protected:
    //SPECULATIVE global arbiter for SW
    double m_spec_local_arbiter_activity, m_spec_global_arbiter_activity;
    double m_spec_local_arbiter_activity_resettable, m_spec_global_arbiter_activity_resettable;	
  private:	
    std::vector<int> m_round_robin_outport;
	std::vector<std::vector<bool> > m_port_req;
	//to implement the fast switch when resynchs can block some links
	std::vector<bool> m_outport_busy;
	std::vector<bool> m_inport_credit_busy;
#endif

	public:
		bool isTurnAllowed(int,int);
		int getPckSizeFromVnet(int vnet, flit_d *t_flit); //FIXME remove
	private:
		//internal functions to allow selected turns
		bool checkTurnXY(int,int,int,int,int,int);	
		bool checkTurnYX(int,int,int,int,int,int);
		bool checkNorthFirst(int,int,int,int,int,int);
		bool checkNegativeFirst(int,int,int,int,int,int);
};

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_VC_ALLOCATOR_D_HH__
