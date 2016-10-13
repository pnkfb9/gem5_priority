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

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_INPUT_UNIT_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_INPUT_UNIT_D_HH__

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <cassert>
#include <list>
#include <queue>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/VirtualChannel_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"

#include "config/use_speculative_va_sa.hh"
#include "config/use_lrc.hh"

#define DEBUG_REMAP 0
#define DEBUG_REMAP_R 5	// router to check
#define DEBUG_REMAP_I 1	// input port to check 

class Router_d;

class InputUnit_d : public Consumer
{
  public:
    InputUnit_d(int id, Router_d *router);
    ~InputUnit_d();

	std::pair<VC_state_type, Time> get_vc_state(int invc){ return m_vcs[invc]->get_vc_state(); /* I/R/V/A/C*/}

    void wakeup();
    flitBuffer_d* getCreditQueue() { return creditQueue; }
    void print(std::ostream& out) const {};

    inline int get_inlink_id() { return m_in_link->get_id(); }

    NetworkLink_d* getInLink_d() { return m_in_link; }

    inline void
    set_vc_state(VC_state_type state, int vc, Time curTime)
    {
        m_vcs[vc]->set_state(state, curTime);
    }

    inline void
    set_enqueue_time(int invc, Time time)
    {
        m_vcs[invc]->set_enqueue_time(time);
    }

    inline Time
    get_enqueue_time(int invc)
    {
        return m_vcs[invc]->get_enqueue_time();
    }

    inline void
    update_credit(int in_vc, int credit)
    {
        m_vcs[in_vc]->update_credit(credit);
    }

    inline bool
    has_credits(int vc)
    {
        return m_vcs[vc]->has_credits();
    }

    void
    increment_credit(int in_vc, bool free_signal, bool free_signal_fast, Time curTime);

    inline int
    get_outvc(int invc)
    {
        return m_vcs.at(invc)->get_outvc();
    }

    inline void
    updateRoute(int vc, int outport, Time curTime)
    {
        m_vcs[vc]->set_outport(outport);
        #if USE_SPECULATIVE_VA_SA==1
		m_vcs[vc]->set_state(VC_SP_, curTime);
   		#else	
		m_vcs[vc]->set_state(VC_AB_, curTime);
		#endif 
   }

	// this is for VOQ 
    inline void
    updateNextRoute(int vc, int outport)
    {
        m_vcs[vc]->set_next_outport(outport);
    }
    
	inline void
    grant_vc(int in_vc, int out_vc, Time curTime)
    {
        m_vcs[in_vc]->grant_vc(out_vc, curTime);
    }

    inline flit_d*
    peekTopFlit(int vc)
    {
        return m_vcs[vc]->peekTopFlit();
    }

    inline flit_d*
    getTopFlit(int vc)
    {
        return m_vcs[vc]->getTopFlit();
    }

    inline bool
    need_stage(int vc, VC_state_type state, flit_stage stage, Time curTime)
    {
        return m_vcs[vc]->need_stage(state, stage, curTime);
    }

    inline bool
    need_stage_nextcycle(int vc, VC_state_type state, flit_stage stage,
                         Time curTime)
    {
        return m_vcs[vc]->need_stage_nextcycle(state, stage, curTime);
    }

    inline bool
    isReady(int invc, Time curTime)
    {
        return m_vcs[invc]->isReady(curTime);
    }

    inline int
    get_route(int vc)
    {
        return m_vcs[vc]->get_route();
    }
    inline int
    get_next_route(int vc)
    {
        return m_vcs[vc]->get_next_route();
    }
    inline void
    set_in_link(NetworkLink_d *link)
    {
        m_in_link = link;
    }

    inline void
    set_credit_link(CreditLink_d *credit_link)
    {
        m_credit_link = credit_link;
    }

    inline double
    get_buf_read_count_resettable(int vnet)
    {
        return m_num_buffer_reads_resettable[vnet];
    }
    
	inline double
    get_buf_write_count_resettable(int vnet)
    {
        return m_num_buffer_writes_resettable[vnet];
    }

    inline double
    get_buf_read_count(int vnet)
    {
        return m_num_buffer_reads[vnet];
    }

    inline double
    get_buf_write_count(int vnet)
    {
        return m_num_buffer_writes[vnet];
    }
    
    inline double
    get_resettable_buf_read_count(int vnet)
    {
        return m_num_buffer_reads_resettable[vnet];
    }

    inline double
    get_resettable_buf_write_count(int vnet)
    {
        return m_num_buffer_writes_resettable[vnet];
    }
    
    inline void
    reset_buf_count()
    {
        //for(auto&  i : m_num_buffer_reads_resettable) i=0;
        //for(auto&  i : m_num_buffer_writes_resettable) i=0;        
		
	for(int i=0;i<m_num_buffer_reads_resettable.size();i++) m_num_buffer_reads_resettable.at(i)=0;
        for(int i=0;i<m_num_buffer_writes_resettable.size();i++) m_num_buffer_writes_resettable.at(i)=0;
    }
    
    int
    getCongestion() const;

    inline int
    getCongestion(int invc) const
    {
        return m_vcs.at(invc)->getCongestion();
    }
	
	inline flit_d*
	peek2ndTopFlit(int invc) const
	{
		return m_vcs.at(invc)->peek2ndTopFlit();
	}

	inline bool isFirstTailLastFlit(int invc)
	{
		return m_vcs[invc]->isFirstTailLastFlit();
	}

   	int getBufferSize(int invc)
	{
		int bufsize=-1;
		if (m_router->get_net_ptr()->get_vnet_type(invc) == DATA_VNET_)
        	bufsize = m_router->get_net_ptr()->getBuffersPerDataVC();
	    else
    	    bufsize = m_router->get_net_ptr()->getBuffersPerCtrlVC();
		assert(bufsize!=-1);
		return bufsize;
	}
	CreditLink_d * getCreditLink_d(){return m_credit_link;}

    int get_num_vcs() const { return m_num_vcs; }
	int get_num_vc_per_vnet() const { return m_vc_per_vnet;}

	//for vnet reuse
	inline int get_real_vnet_used(int invc){return 	m_vcs[invc]->get_real_vnet_used(); }
	inline void set_real_vnet_used(int invc,int vnet){return m_vcs[invc]->set_real_vnet_used(vnet); }
 
   	// adaptive routing/////////////////////////////
	void setIsAdaptive(int vc,int isAdaptive)
	{
		m_vcs[vc]->set_is_adaptive(isAdaptive);
	}
	int getIsAdaptive(int vc)
	{
		return m_vcs[vc]->get_is_adaptive();
	}
	/////////////////////////////////////////////////
	int getID(){return m_id;}
	Router_d* get_router(){return m_router;}
  private:
    int m_id;
    int m_num_vcs;
    int m_vc_per_vnet;
    std::vector<double> m_num_buffer_writes;
    std::vector<double> m_num_buffer_reads;
    std::vector<double> m_num_buffer_writes_resettable;
    std::vector<double> m_num_buffer_reads_resettable;

    Router_d *m_router;
    NetworkLink_d *m_in_link;
    CreditLink_d *m_credit_link;
    flitBuffer_d *creditQueue;

    // Virtual channels
    std::vector<VirtualChannel_d *> m_vcs;

	///////////////////////////////////////////////////////////
	////// virtual channels remap logic and structures  ///////
	///////////////////////////////////////////////////////////
	/*
	 * NOTE: the SA reset a remap indirectly when it signal free the vc using
	 * the modified input_unit->increment_credit() function. 
	*/
	public:
	std::vector<bool>& get_reused_remap_vc_usable(){return m_reused_remap_vc_usable;};
	std::vector<bool>& get_reused_remap_vc_used(){return m_reused_remap_vc_used;};
	std::vector<int>& get_reused_remap_vc_outport(){return m_reused_remap_vc_outport;};	
	std::vector<int>& get_reused_remap_vc_outvc(){return m_reused_remap_vc_outvc;};		
	std::vector<Cycles>& get_reused_remap_vc_outport_rc_cycle(){return m_reused_remap_vc_outport_rc_cycle;};	
	std::vector<Cycles>& get_reused_remap_vc_outvc_va_cycle(){return m_reused_remap_vc_outvc_va_cycle;};

	std::vector<Time>& get_reused_remap_vc_fake_free_sig(){return m_reused_remap_vc_fake_free_sig;};

	private:
	// structures for buf reuse
	std::vector<bool> m_reused_remap_vc_usable;
	std::vector<bool> m_reused_remap_vc_used;
	std::vector<int> m_reused_remap_vc_outport;	
	std::vector<int> m_reused_remap_vc_outvc;
	std::vector<Cycles> m_reused_remap_vc_outport_rc_cycle;	
	std::vector<Cycles> m_reused_remap_vc_outvc_va_cycle;

	std::vector<Time> m_reused_remap_vc_fake_free_sig;

	std::vector<bool> m_buf_is_on;

	//Buffer Reuse statistics
	public:
	 inline void
	 setReuseCycleVc(int vc, Time curTime)
	 {
           m_vcs[vc]->setReuseCycle(curTime);
	 }
	 inline Time
	 getReuseCycleVc(int vc)
	 {
           return m_vcs[vc]->getReuseCycle();
	 }	
	////////////////////////////////////////
	std::map<int,int> m_vc_remap;
	std::map<int,std::list<int>> m_vc_remap_inverse; // for perf reasons when update credit
	int vcRemapPolicy(int vc);
	int vcRemapPolicyReuseVnet(int vc,flit_d*);

	void activeRemap(std::ostream& out)
	{
		out<<"\t@"<<curTick()<<"\tACTIVE REMAP"<<std::endl;
		for(int i=0;i<m_num_vcs;i++)
		{
			if(m_vc_remap[i]!=-1) 
				out<<"\t"<<i<<"->"<<m_vc_remap[i]<<std::endl;	
		}
		for(int i=0;i<m_vc_remap_inverse.size();i++)
		{
			out<<"\t\tclonedVC "<<i<<":\t";
			for(auto it=m_vc_remap_inverse[i].begin();it!=m_vc_remap_inverse[i].end();it++)
				out<<*it<<" ";
			out<<std::endl;
		}
	};
	std::vector<Tick> timestamp_last_used_buf;
	public:
		std::vector<Tick>& getTimestampLastUsedBuf(){return timestamp_last_used_buf;};

	/////////////////////////////////////
	// NON_ATOMIC_VC_ALLOC
	private:
	// used to test if a pkt is totally received in the inbuf,
	// then a non atomic allocation can happen. UPDATED for each received flit
	std::vector<bool> m_isLastFlitTail;
	std::vector<int> m_vnetIdLastFlitTail;
	std::vector<std::queue<int>* > m_outportTempQueue; // one queue per inVC to store the 
        int m_last_max_priority_vc;													 // outport for the non_atomic allocated pkts 
	public:

	int get_vc_priority(int vc); //added by panc for routing prioritization.
        
	int get_max_priority_vc()
	{
		return m_last_max_priority_vc;
	}
	
	void set_max_priority_vc(int invc)
	{
		m_last_max_priority_vc = invc;
	}	

	int get_vnetIdLastFlitTail(int invc)
	{
		assert(invc>=0&&invc<m_num_vcs);
		return m_vnetIdLastFlitTail[invc];
	}
	void set_vnetIdLastFlitTail(int invc,int vnet_id/*-1 is invalid, empty*/)
	{
		assert(invc>=0&&invc<m_num_vcs);
		//assert(vnet_id==-1 || (vnet_id>=0&&vnet_id<m_);
		m_vnetIdLastFlitTail[invc]=vnet_id;
	}
	std::queue<int>* getOutportTempQueue(int invc)
	{
		assert(invc>=0&&invc<m_num_vcs);
		return m_outportTempQueue.at(invc);
	}
#if USE_VICHAR==1
	private:
	/////////////////////////////////////
	///// VICHAR SUPPORT ////////////////
	//NOTE: vichar bufdepth is max pkt len since it does not matter, as the
	//number of VCs since it stops floding flits when no flit slots are
	//available downstream. Thus the only interesting value is the available
	//slots per vnet and the usedSlots per vnet passed by the router.
	std::vector<int> usedSlotPerVnet;
	public:
	int getUsedSlotPerVnet(int vnet)
	{	
		assert(vnet>=0 && vnet<usedSlotPerVnet.size());
		return usedSlotPerVnet.at(vnet);
	}
	void incrUsedSlotPerVnet(int vnet)
	{	
		assert(vnet>=0 && vnet<usedSlotPerVnet.size());
		usedSlotPerVnet.at(vnet)++;
		assert(usedSlotPerVnet.at(vnet)<=m_router->getTotVicharSlotPerVnet());
	}
	void decrUsedSlotPerVnet(int vnet)
	{	
		assert(vnet>=0 && vnet<usedSlotPerVnet.size());
		usedSlotPerVnet.at(vnet)--;
		assert(usedSlotPerVnet.at(vnet)>=0);
	}
	/////////////////////////////////////
#endif

#if USE_LRC == 1
public:
	void wakeup_LRC_BW();	//similar to wakeup but called in SA only

#endif
};

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_INPUT_UNIT_D_HH__
