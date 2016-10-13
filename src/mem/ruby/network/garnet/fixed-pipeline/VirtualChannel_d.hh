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

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_VIRTUAL_CHANNEL_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_VIRTUAL_CHANNEL_D_HH__

#include <utility>

#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"

class VirtualChannel_d
{
  public:
    VirtualChannel_d(int id, Time curTime);
    ~VirtualChannel_d();

    bool need_stage(VC_state_type state, flit_stage stage, Time curTime);
    bool need_stage_nextcycle(VC_state_type state, flit_stage stage,
                              Time curTime);
    void set_outport(int outport);
	void set_next_outport(int next_outport);
    void grant_vc(int out_vc, Time curTime);

    inline Time get_enqueue_time()          { return m_enqueue_time; }
    inline void set_enqueue_time(Time time) { m_enqueue_time = time; }
    inline VC_state_type get_state()        { return m_vc_state.first; }
    inline int get_outvc()                  { return m_output_vc; }
    inline bool has_credits()               { return (m_credit_count > 0); }
    inline int get_route()                  { return route; }
    inline int get_prio_latency()	    { return prio_latency; }	//Added by Panc to add priority scheduling to VA and SA stages.
    inline void set_prio_latency(int priority) { prio_latency = priority; } //Added by Panc to priority scheduling to VA and SA stages.
	inline int get_next_route()             { return next_route; }
    inline void update_credit(int credit)   { m_credit_count = credit; }
    inline void increment_credit()          { m_credit_count++; }

    inline bool isReady(Time curTime)
    {
        return m_input_buffer->isReady(curTime);
    }
    inline void
    insertFlit(flit_d *t_flit)
    {
		//// update the real vnet of the stored packet if this flit is an head ////
        //if ((t_flit->get_type() == HEAD_) || (t_flit->get_type() == HEAD_TAIL_)) 
		//{
		//	assert( (t_flit->get_vnet()==-1 && m_vc_state.first==IDLE_) || (t_flit->get_vnet()!=-1 && m_vc_state.first!=IDLE_));
		//	real_vnet=t_flit->get_vnet();
		//}
		//////////////////////////////////////////////////////////////////////////
		m_input_buffer->insert(t_flit);
    }

    inline void
    set_state(VC_state_type m_state, Time curTime)
    {
		//if(m_state==IDLE_)
		//{//used for vnet reuse optimization in pkt sw uarch
		//	assert(real_vnet!=-1);
		//	real_vnet=-1;
		//}
        m_vc_state.first = m_state;
        m_vc_state.second = curTime + 1;
    }

    inline flit_d*
    peekTopFlit()
    {
        return m_input_buffer->peekTopFlit();
    }

    inline flit_d*
    getTopFlit()
    {
        return m_input_buffer->getTopFlit();
    }
    
    inline int
    getCongestion() const
    {
        return m_input_buffer->getCongestion();
    }
	inline flit_d*
	peek2ndTopFlit() const
	{
		return m_input_buffer->peek2ndTopFlit();
	}
	
	inline bool isFirstTailLastFlit(){return m_input_buffer->isFirstTailLastFlit();}
	
	public:
	void set_is_adaptive(int isAdaptive){is_adaptive=isAdaptive;};
	int get_is_adaptive(){return is_adaptive;};

	int get_real_vnet_used(){return real_vnet;}
	void set_real_vnet_used(int vnet){real_vnet=vnet;}

	std::pair<VC_state_type, Time> get_vc_state(){ return m_vc_state; /* I/R/V/A/C*/}

  private:
    int m_id;
    flitBuffer_d *m_input_buffer;
    std::pair<VC_state_type, Time> m_vc_state; // I/R/V/A/C
    int route;

    int prio_latency;		//variable to store the priority of the packet in the current vc, if any. 0 = no priority

    Time m_enqueue_time;
    int m_output_vc;
    int m_credit_count;

	int next_route;

	int is_adaptive;

	int real_vnet;

  //VC reuse statistics
  private:
    Time reuseCycle;
  public:
    void setReuseCycle(Time tick){reuseCycle=tick;}
    Time getReuseCycle(){return reuseCycle;}

};

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_VIRTUAL_CHANNEL_D_HH__
