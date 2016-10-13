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

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_INTERFACE_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_INTERFACE_D_HH__

#include <iostream>
#include <vector>

#include <utility>
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutVcState_d.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/slicc_interface/Message.hh"
#include "mem/ruby/buffers/MessageBuffer.hh"

#define TRACER_PATH "m5out/nis/"
#define TRACER_CSV_PATH "cutbuf_config/cutbuf_on_off_buf_sensing.csv" //modify path
#define USE_NI_TRACER 1
#define USE_HOTSPOT 0
#define COMPRESSION_TRACING 1
#define PROFILER 1
#define USE_TRAFFIC_TRACER 1

#define USE_COMPRESSION_DEBUG 0
#define HOTSPOT_SRC "cutbuf_config/hotspot_src.csv"
#define HOTSPOT_DST "cutbuf_config/hotspot_dst.csv"

#include "config/use_vichar.hh"
#include "config/use_pnet.hh"

class NetworkMessage;
class MessageBuffer;
class flitBuffer_d;
class NetworkInterface_PNET_Container_d;
class Compressor_d;
class Decompressor_d;


class NetworkInterface_d :  public ClockedObject, public Consumer
{
  public:
    NetworkInterface_d(int id, int virtual_networks,
                       GarnetNetwork_d* network_ptr);

    ~NetworkInterface_d();

    void addInPort(NetworkLink_d *in_link, CreditLink_d *credit_link);
    void addOutPort(NetworkLink_d *out_link, CreditLink_d *credit_link);
    std::vector<MessageBuffer *> getOutNode(); //added by panc
    void wakeup();
    void addNode(std::vector<MessageBuffer *> &inNode,
                 std::vector<MessageBuffer *> &outNode);
    void print(std::ostream& out) const;
    int get_vnet(int vc);

	virtual std::string getObjectId() const { return m_objectId; }

    virtual void changePeriod(Tick period);

	NetworkLink_d* getOutNetLink(){return outNetLink;}

	public:
		class CongestionMetrics
		{
			public:
				CongestionMetrics():count(0),t_start(Cycles(-1)){};
				Tick count;
				Cycles t_start;
		};

	Tick getC1Cycles()
	{
		/* update in message congestion data
		 TODO TODO optimize TODO TODO
		 move it into slicc when each message is really added
		*/
		Tick extra_cycles=0;
		for(int i=0;i<inNode_ptr.size();i++)
		{
			for(int j=0;j<inNode_ptr[i]->getSize();j++)
			{
				extra_cycles+=curCycle()-inNode_ptr[i]->getMsgPtr(j).get()->getT_StartCycle();
			}
		}
		for(int i=0;i<c1_ni_matrix.size();i++)
		{
			extra_cycles=extra_cycles+c1_ni_matrix.at(i).count;
			c1_ni_matrix.at(i).count=Cycles(0);
		}


		old_get_curCycle=curCycle();
		return extra_cycles;
	}

  std::vector<flitBuffer_d*> getNIBuffers() //added by panc
	{
		return m_ni_buffers;
	}

  int getOutPkt()
	{
		//std::cout<<"getOutPkt()"<<std::endl;
		return c1_out_ni_pkt;
	}

	void incrementOutPkt(int increment)
	{
		//std::cout<<"incrementOutPkt()"<<std::endl;
		c1_out_ni_pkt+=increment;
	}

	void resetOutPkt()
	{
		//std::cout<<"resetOutPkt()"<<std::endl;
		c1_out_ni_pkt=0;
	}

	GarnetNetwork_d* getNetPtr()
	{
		return m_net_ptr;
	}

    std::vector<OutVcState_d *>& getOutVcState_d_ref(){return m_out_vc_state;}

    inline int
    getCongestion(int out_vc_ni_buffer)
    {
        return m_ni_buffers.at(out_vc_ni_buffer)->getCongestion();
    }

    Consumer *getConnectedComponent()
    {
        return outNode_ptr.at(0)->getConsumer();
    }

    Consumer *getNoCSideConnectedComponent()
    {
        return outNetLink->getLinkConsumer();
    }

    void incrementInjFlits() //added by panc
    {
    	m_ni_flits_injected++;
    }
	private:
	/*congestion metrics*/
	std::vector<CongestionMetrics> c1_ni_matrix;

	int  c1_out_ni_pkt;

	int rr_vnet_ins_msg;
   private:
  	Cycles old_get_curCycle;

    std::string m_objectId;

    GarnetNetwork_d *m_net_ptr;
    int m_virtual_networks, m_num_vcs, m_vc_per_vnet;
    NodeID m_id;
	public:
		int get_id(){return (int)m_id;}
     //#if USE_PNET==1
     private:
		int m_minor_id;
		NetworkInterface_PNET_Container_d* m_container_ptr;
     public:
         void setMinorId(int minor){m_minor_id=minor;}
         int getMinorId(){return m_minor_id;};
		 void setObjectId();
		 void setPnetContainerPtr(NetworkInterface_PNET_Container_d* ptr){m_container_ptr=ptr;}
     //#endif

	private:
    std::vector<OutVcState_d *> m_out_vc_state;
    std::vector<int> m_vc_allocator;
    int m_vc_round_robin; // For round robin scheduling

	flitBuffer_d *outSrcQueue; // For modelling link contention
    flitBuffer_d *creditQueue;

    NetworkLink_d *inNetLink;
    NetworkLink_d *outNetLink;
    CreditLink_d *m_credit_link;
    CreditLink_d *m_ni_credit_link;

    Compressor_d *m_compressor;       //added by panc
    Decompressor_d *m_decompressor;   //added by panc
public:
	std::vector<MessageBuffer *>& getInNodePtrRef(){return inNode_ptr;}
	std::vector<MessageBuffer *>& getOutNodePtrRef(){return outNode_ptr;}
	NetworkLink_d* getOutNetLinkPtr(){return outNetLink;}
	NetworkLink_d* getInNetLinkPtr(){return inNetLink;}

	flitBuffer_d* getOutSrcQueuePtr(){return outSrcQueue;}
	flitBuffer_d* getCreditQueuePtr(){return creditQueue;}
private:

    // Input Flit Buffers
    // The flit buffers which will serve the Consumer
    std::vector<flitBuffer_d *>   m_ni_buffers;
    std::vector<Time> m_ni_enqueue_time;

    // The Message buffers that takes messages from the protocol
    std::vector<MessageBuffer *> inNode_ptr;
    // The Message buffers that provides messages to the protocol
    std::vector<MessageBuffer *> outNode_ptr;

    bool flitisizeMessage(MsgPtr msg_ptr, int vnet);
    int calculateVC(int vnet);
    void scheduleOutputLink();
    void checkReschedule();

	// extended support for adaptive routing
	// one table to store seq_num per sent flits on destination basis,
	// one table to store seq_num per received flits on source basis,
	// then one more table to store a vector of incomoing unordered msges
	// waiting for previous to arrive.
	std::vector<uint64_t> m_reoreder_counter_out; // based on NodeID
	std::vector<uint64_t> m_reoreder_counter_in; // based on NodeID

	std::vector<std::vector<std::vector<flit_d*>>> m_reoreder_msg_in;


	bool send_dummy_flit;
	Tick start_tick_dummy;
	Tick end_tick_dummy;
	std::vector<uint64_t> sent_dummy_flit;
	double required_avg_dummy_flit;
	std::vector<MachineID> dest_dummy_traffic;

	std::vector<std::pair<int,double> > fake_traffic;

	// packet switching structures WH_VC_SA
	int m_registred_packet_switching_path_vc;

	// for the vnet reuse optimization
	// each vcs contains the vnet packet served. The value is meaningfull if the
	// vc is not idle
	std::vector<int> vnet_in_this_vc;
	int m_virtual_networks_protocol;
	int m_virtual_networks_spurious;
  bool is_tracer_active(int nic_id);
#if COMPRESSION_TRACING == 1
  // Output streams and selective tracing variable
  bool active_tracer;
  int m_tracer_last_tick;

  std::ofstream* m_ni_tracer;

  //value for distances from this ni
  int m_ni_distance;
  int m_ni_flits_injected;
  #endif
  #if USE_COMPRESSION_DEBUG == 1
  std::vector<int> m_vnet_delay;
  std::vector<int> m_vnet_total_delay;
  std::vector<bool> m_consecutives;
  #endif
#if USE_HOTSPOT == 1
  bool is_hotspot();
  bool is_hotspot_dst();
  bool hotspot_dest;
  bool hotspot_source;
#endif
  #if PROFILER == 1

  std::vector<int> m_ni_inj_flits;
  std::vector<int> m_ni_rec_flits;
  public:
  void printFinalStats();
  void resetFinalStats();
  #endif

  #if USE_NI_TRACER == 1

  std::vector<int> long_messages_delivered;
  std::vector<int> short_messages_delivered;
  std::vector<int> short_packet_flits;
  std::vector<int> long_packet_flits;
  std::vector<int> queue_congestion;
  void updateShortPacket(int vnet);
  void updateLongPacket(int vnet);
  void updateShortMessages(int vnet);
  void updateLongMessages(int vnet);
  void getTrafficMetrics(std::string filename);
  void resetShortPacket(int vnet);
  void resetLongPacket(int vnet);
  void resetShortMessages(int vnet);
  void resetLongMessages(int vnet);
  #endif
	
  std::vector<int> m_vcs_priority;		//structure to deal with priority also in NiC	
  
  std::deque<int> get_priority_vector();
  void set_vc_priority(int vc,int prio); 
  void reset_vcs_priority(); 

 #if USE_TRAFFIC_TRACER == 1

        std::ofstream* m_traffic_tracer;
        void updateTracer(int cycle,int num_flits);

        #endif

	int control_pkts_gone;
	bool m_is_reset;
	int getControlPktsGone(){	return control_pkts_gone;	}
	void incrementControlPktsGone(){	control_pkts_gone++;	}
	void resetControlPktsGone(){ control_pkts_gone=0;}
	std::ofstream* m_nic_compression_ratio;
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
		assert(availSlotPerVnet.at(vnet)<=m_net_ptr->getTotVicharSlotPerVnet());
	}
	void decrAvailSlotPerVnet(int vnet)
	{
		assert(vnet>=0 && vnet<availSlotPerVnet.size());
		availSlotPerVnet.at(vnet)--;
		assert(availSlotPerVnet.at(vnet)>=0);
	}

	/////////////////////////////////////
#endif

#if USE_APNEA_BASE==1
	private:
		int initVcOp;
		int finalVcOp;
		int initVcIp;
		int finalVcIp;
	public:
		int getInitVcOp(){return initVcOp;checkFeasibilityVcSetOp();};
		int getFinalVcOp(){return finalVcOp;checkFeasibilityVcSetOp();};
		int getInitVcIp(){return initVcIp;checkFeasibilityVcSetIp();};
		int getFinalVcIp(){return finalVcIp;checkFeasibilityVcSetIp();};

		void setInitVcOp(int val){initVcOp=val;checkFeasibilityVcSetOp();};
		void setFinalVcOp(int val){finalVcOp=val;checkFeasibilityVcSetOp();};
		void setInitVcIp(int val){initVcIp=val;checkFeasibilityVcSetIp();};
		void setFinalVcIp(int val){finalVcIp=val;checkFeasibilityVcSetIp();};

		void incInitVcOp(){initVcOp=initVcOp+1;checkFeasibilityVcSetOp();};
		void incFinalVcOp() {finalVcOp=finalVcOp+1;checkFeasibilityVcSetOp();};
		void incInitVcIp(){initVcIp=initVcIp+1;checkFeasibilityVcSetIp();};
		void incFinalVcIp(){finalVcIp=finalVcIp+1;checkFeasibilityVcSetIp();};

		void checkFeasibilityVcSetOp(){ assert(initVcOp>=0 && finalVcOp<=m_num_vcs);}
		void checkFeasibilityVcSetIp(){ assert(initVcIp>=0 && finalVcIp<=m_num_vcs);}

#endif

};

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_INTERFACE_D_HH__
