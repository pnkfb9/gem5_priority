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

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_OUTPUT_UNIT_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_OUTPUT_UNIT_D_HH__

#include <iostream>
#include <vector>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutVcState_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"

class Router_d;

class OutputUnit_d : public Consumer
{
  public:
    OutputUnit_d(int id, Router_d *router);
    ~OutputUnit_d();
    void set_out_link(NetworkLink_d *link);
    void set_credit_link(CreditLink_d *credit_link);
    void wakeup();
    flitBuffer_d* getOutQueue();
    void update_vc(int vc, int in_port, int in_vc);
    void print(std::ostream& out) const {};
    void decrement_credit(int out_vc);

    int
    get_credit_cnt(int vc)
    {
        return m_outvc_state[vc]->get_credit_count();
    }

    inline int
    get_outlink_id()
    {
        return m_out_link->get_id();
    }

    inline void
    set_vc_state(VC_state_type state, int vc, Time curTime)
    {
        //Called by VCallocator_d of the same router. As it does not cross clock
        //domain boundaries this is correct.

        m_outvc_state[vc]->setState(state, curTime + 1); 
    }
	inline VC_state_type
	get_vc_state(int vc, Time curTime)
	{
		return m_outvc_state[vc]->getState(curTime);
	}


    inline bool
    is_vc_idle(int vc, Time curTime)
    {
		assert(vc>=0&&vc<m_num_vcs && " OU->is_vc_idle(vc) vc out of bound ");
        return (m_outvc_state[vc]->isInState(IDLE_, curTime));
    }

    inline void
    insert_flit(flit_d *t_flit)
    {
        m_out_buffer->insert(t_flit);
		m_out_link->getReshnchronizer()->sendRequest();
        m_out_link->scheduleEvent(1);
    }
   
	NetworkLink_d* getOutLink_d(){return m_out_link;}

	inline int get_id(){return m_id;}

	inline int getInport(int out_vc){return m_outvc_state[out_vc]->get_inport();}
	inline int getInvc(int out_vc){return m_outvc_state[out_vc]->get_invc();}

    /* NON_ATOMIC_VC_ALLOC
	New function to extraction data from inv for atomic allocation
     */
    inline void inc_vc_counter_pkt(int out_vc, Time curTime) { m_outvc_state[out_vc]->inc_counter_pkt(curTime); }
    inline void dec_vc_counter_pkt(int out_vc, Time curTime) { m_outvc_state[out_vc]->dec_counter_pkt(curTime); }
    inline int get_vc_counter_pkt(int out_vc, Time curTime) { return m_outvc_state[out_vc]->get_counter_pkt(curTime); }
    inline void set_vc_busy_to_vnet_id(int out_vc, int vnet) { m_outvc_state[out_vc]->set_busy_to_vnet_id(vnet); }
    inline int get_vc_busy_to_vnet_id(int out_vc) { return  m_outvc_state[out_vc]->get_busy_to_vnet_id(); }
	Router_d* get_router(){return m_router;}
  private:
    int m_id;
    int m_num_vcs;
    Router_d *m_router;
    NetworkLink_d *m_out_link;
    CreditLink_d *m_credit_link;
       int m_last_max_priority_vc;	//used to save last priority vc
    flitBuffer_d *m_out_buffer; // This is for the network link to consume
    std::vector<OutVcState_d *> m_outvc_state; // vc state of downstream router
  public:
	void set_max_priority_vc(int outvc)
	{
		m_last_max_priority_vc = outvc;
	}

	int get_max_priority_vc()
	{
		return m_last_max_priority_vc;
	}
#if USE_VICHAR==1
	private:
	/////////////////////////////////////
	///// VICHAR SUPPORT ////////////////
	//NOTE: vichar bufdepth is max pkt len since it does not matter, as the
	//number of VCs since it stops floding flits when no flit slots are
	//available downstream. Thus the only interesting value is the available
	//slots per vnet and the usedSlots per vnet passed by the router.
	std::vector<int> availSlotPerVnet;
	public:
	int getAvailSlotPerVnet(int vnet)
	{	
		assert(vnet>=0 && vnet<availSlotPerVnet.size());
		return availSlotPerVnet.at(vnet);
	}
	void incrAvailSlotPerVnet(int vnet)
	{	
		assert(vnet>=0 && vnet<availSlotPerVnet.size());
		availSlotPerVnet.at(vnet)++;
		assert(availSlotPerVnet.at(vnet)<=m_router->getTotVicharSlotPerVnet());
	}
	void decrAvailSlotPerVnet(int vnet)
	{	
		assert(vnet>=0 && vnet<availSlotPerVnet.size());
		availSlotPerVnet.at(vnet)--;
		assert(availSlotPerVnet.at(vnet)>=0);
	}

	/////////////////////////////////////
#endif
};

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_OUTPUT_UNIT_D_HH__
