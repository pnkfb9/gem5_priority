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

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_FLIT_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_FLIT_D_HH__

#include <cassert>
#include <iostream>
#include <sstream>

#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/slicc_interface/Message.hh"

class flit_d
{
  public:
    flit_d(int id, int vc, int vnet, int size, MsgPtr msg_ptr, Time curTime);
    flit_d(int vc, bool is_free_signal, Time curTime);
    void set_outport(int port) { m_outport = port; }
    int get_outport() {return m_outport; }
    void print(std::ostream& out) const;
    bool m_is_zero_comp;
    bool is_free_signal() { return m_is_free_signal; }
    int get_size() { return m_size; }
    Time get_enqueue_time() { return m_enqueue_time; }
    int get_id() { return m_id; }
    Time get_time() { return m_time; }
    void set_time(Time time) { m_time = time; }
	
    void set_priority(int priority){ m_flit_priority = priority; }	
    int get_priority(){ return m_flit_priority; }	
	  
    void set_vnet(int vnet){m_vnet=vnet;}
    int get_vnet() { return m_vnet; }

    int get_vc() { return m_vc; }
    void set_vc(int vc) { m_vc = vc; }
    MsgPtr& get_msg_ptr() { return m_msg_ptr; }
    flit_type get_type() { return m_type; }

    bool
    is_stage(flit_stage t_stage, Time curTime)
    {
        return (m_stage.first == t_stage &&
                curTime >= m_stage.second);
    }

    bool
    is_next_stage(flit_stage t_stage, Time curTime)
    {
        return (m_stage.first == t_stage &&
                (curTime + 1) >= m_stage.second);
    }

    void
    advance_stage(flit_stage t_stage, Time curTime)
    {
        m_stage.first = t_stage;
        m_stage.second = curTime + 1;
    }

    std::pair<flit_stage, Time>
    get_stage()
    {
        return m_stage;
    }

    void
    set_delay(int delay)
    {
        src_delay = delay;
    }

    int
    get_delay()
    {
        return src_delay;
    }

    static bool
    greater(flit_d* n1, flit_d* n2)
    {
        if (n1->get_time() == n2->get_time()) {
            //assert(n1->flit_id != n2->flit_id);
            return (n1->get_id() > n2->get_id());
        } else {
            return (n1->get_time() > n2->get_time());
        }
    }
    void
    set_compressed(bool compressed) //added by panc
    {
        m_is_compressed=compressed;
    }
    bool
    get_compressed()  //added by panc
    {
       return  m_is_compressed;
    }
	bool
	is_zero_comp()
	{
	return m_is_zero_comp;
		}
	void
	set_zero_comp()
	{
		m_is_zero_comp = true;
	}

  public:
  	// START extended flit class to support adaptive routing
  	NodeID getSourceNodeID(){return m_source_id;};
	void setSourceNodeID(NodeID source){m_source_id=source;}
	uint64_t getSourceSeqID(){return seq_id;}
	void setSourceSeqID(uint64_t sequence_id){seq_id=sequence_id;}
	void setIsAdaptive(int new_value){isAdaptive=new_value;}
	int getIsAdaptive(){return isAdaptive;}
	// END extended flit class to support adaptive routing

	// START extended flit for speculative pipeline
	void addHop(){num_hop=num_hop+1;}
	float getHop(){return num_hop;}
  void initHop(){num_hop=0;}
	//END extendend flit for speculative pipeline

	//START extended flit class for tick latency traker
  void startTracker(int curTick){	enqueue_tick=curTick; }
	void stopTracker(int curTick){tick_latency=(curTick)-enqueue_tick;}
	float getTickLatency(){return tick_latency;}
	//END extended flit class for latency traker
  private:
    int m_id;
    int m_vnet;

    int m_vc;
    int m_size;
    bool m_is_free_signal;
      bool m_is_compressed;
    Time m_enqueue_time, m_time;
    flit_type m_type;
    MsgPtr m_msg_ptr;
    int m_outport;
    int src_delay;
    int m_flit_priority;	//added by panc

    std::pair<flit_stage, Time> m_stage;
	// added for adaptive routing
	NodeID m_source_id;
	uint64_t seq_id;

	// added for speculative pipeline
	uint64_t num_hop;
  static uint64_t id_all;

	//added for tick tracker network latency
  uint64_t enqueue_tick;
	uint64_t tick_latency;

int isAdaptive; //0 det,1 adaptive,2 adaptive->det

	uint64_t unique_id;
	std::stringstream ss_traversed_routers;	//keep track of all the traversed routers
  public:
	void setUniqueId(){
					unique_id=id_all;
					id_all=id_all+1;
	}
	uint64_t getUniqueId(){return unique_id;}

	std::string getTraversedRouters(){ return ss_traversed_routers.str();}
	void addTraversedRouter(std::string r_temp){ ss_traversed_routers<<r_temp<<"-";}

};



inline std::ostream&
operator<<(std::ostream& out, const flit_d& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_FLIT_D_HH__
