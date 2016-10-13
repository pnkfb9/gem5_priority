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

#include <cassert>
#include <cmath>
#include <sstream>
#include <fstream>
#include <deque>

#include "mem/protocol/Types.hh"
#include "mem/ruby/system/MachineID.hh"

#include "base/cast.hh"
#include "base/stl_helpers.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/buffers/MessageBuffer.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"
#include "mem/ruby/slicc_interface/NetworkMessage.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "cpu/base.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_PNET_Container_d.hh"
#include "config/use_spurious_vc_vnet_reuse.hh"
#include "config/debug_wh_vc_sa.hh"
#include "config/use_synthetic_traffic.hh"
#include "config/use_adaptive_routing.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Compressor_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Decompressor_d.hh"

#define DEBUG_PRINT_NOISE 0
#define MIX_REAL_NOISE_TRAFFIC 0
#define USE_COMPRESSION 0
#define USE_DECOMPRESSION 0
#define MAX_NUM_FLITS 1
#define MAX_HOPS 0
#define TRACER_SAMPLE_CYCLES 10
#define DISTANCE_STATS_GRANULARITY 2 //1 for vc granularity, 2 for vnet granularity, 3 for NIC granularity
#define USE_DEBUG 0
#define SLEEP_MODEL 1
#define COMPRESSION_TRACING 1
#define USE_COMPRESSION_DEBUG 0 //was USE_DEBUG
#define USE_HOTSPOT 0
#define USE_NI_TRACER 1
#define TRACK_TRAFFIC 0
//#define WH_VC_SA_NIC 1
#define PRIORITY_SCHED 1
static uint64_t num_ctrl_pkts=0;
static uint64_t num_data_pkts=0;

static uint64_t latency_ctrl_pkts_net=0;
static uint64_t latency_data_pkts_net=0;

static uint64_t latency_ctrl_pkts_queue=0;
static uint64_t latency_data_pkts_queue=0;

using namespace std;
using m5::stl_helpers::deletePointers;

static ClockedObjectParams *makeParam(const GarnetNetwork_d *network_ptr)
{
    //FIXME: verify!!
    ClockedObjectParams *result=new ClockedObjectParams;
    result->name=network_ptr->params()->name;
    result->pyobj=network_ptr->params()->pyobj;
    result->eventq=network_ptr->params()->eventq;
    result->clock=network_ptr->clockPeriod();
    return result;
}

NetworkInterface_d::NetworkInterface_d(int id, int virtual_networks,
                                       GarnetNetwork_d *network_ptr)
    : ClockedObject(makeParam(network_ptr)), Consumer(this)
{
    m_id = id;
    m_net_ptr = network_ptr;
    m_virtual_networks  = virtual_networks;
    m_vc_per_vnet = m_net_ptr->getVCsPerVnet();
    m_num_vcs = m_vc_per_vnet*m_virtual_networks;

    m_vc_round_robin = 0;
    m_ni_buffers.resize(m_num_vcs);
    m_ni_enqueue_time.resize(m_num_vcs);
    inNode_ptr.resize(m_virtual_networks);
    outNode_ptr.resize(m_virtual_networks);
    creditQueue = new flitBuffer_d();
	m_is_reset=false;
    // instantiating the NI flit buffers
    for (int i = 0; i < m_num_vcs; i++) {
        m_ni_buffers[i] = new flitBuffer_d();
        m_ni_enqueue_time[i] = INFINITE_;
    }
    m_vc_allocator.resize(m_virtual_networks); // 1 allocator per vnet
    for (int i = 0; i < m_virtual_networks; i++) {
        m_vc_allocator[i] = 0;
    }

    for (int i = 0; i < m_num_vcs; i++) {
        m_out_vc_state.push_back(new OutVcState_d(i, m_net_ptr));
        m_out_vc_state[i]->setState(IDLE_, /*m_net_ptr*/this->curCycle());
    }

    this->setEventPrio(Event::NOC_NetworkInterface_Pri);
#if USE_PNET==0
	// if PNET impl is used wait to assign an objid to get the PNET container ptr
	std::ostringstream ss;
    ss << typeid(this).name() << m_id;
    m_objectId = ss.str();
#endif
	/*structs to store congestion data*/
	c1_out_ni_pkt=0;
	c1_ni_matrix.resize(m_num_vcs);
	old_get_curCycle=Cycles(-1);
	/**/

// START extended support for adaptive routing
	assert((int) MachineType_NUM >1 && "We need at least L1 and L2 or MC");
	int numL1_L2_MC_UNITS=(int) MachineType_NUM *
					MachineType_base_number((MachineType) (1));

	m_reoreder_counter_in.resize( numL1_L2_MC_UNITS);
	m_reoreder_counter_out.resize(numL1_L2_MC_UNITS);

	for(int i=0;i<numL1_L2_MC_UNITS;i++)
	{
			m_reoreder_counter_in[i]=0;
      m_reoreder_msg_in.resize(numL1_L2_MC_UNITS);
			m_reoreder_counter_out[i]=0;
	}
// END extended support for adaptive routing
#if MIX_REAL_NOISE_TRAFFIC == 1
    sent_dummy_flit.resize(m_virtual_networks); // 1 allocator per vnet
    for (int i = 0; i < m_virtual_networks; i++)
	{
        sent_dummy_flit[i] = 0;
    }
	if(m_id==1 ||m_id==2||m_id==6)
	{
		send_dummy_flit=true;
		start_tick_dummy=10000;
		required_avg_dummy_flit=0.05;
	}
	else
	{
                end_tick_dummy=MaxTick; // MaxTick means until the end of the simulation
		send_dummy_flit=false;
		start_tick_dummy=10000;
		end_tick_dummy=0; // MaxTick means until the end of the simulation
		required_avg_dummy_flit=0;
	}

	// load the fake paths from file
	std::vector<int> fake_traffic_paths;

	std::ifstream f("./network_configs/fake_traffic_paths.txt");
	std::ifstream f_inj("./network_configs/fake_traffic_inj_rate.txt");

	assert(f.is_open());
	assert(f_inj.is_open());

	std::string s;
	std::string s_inj;

	int count=0;
	while(std::getline(f,s) && std::getline(f_inj,s_inj))
	{
		int count_dest=0;
		if(count==m_id)

		{
			std::stringstream ss(s);
			std::stringstream ss_inj(s_inj);
			std::cout<<s<<std::endl;
			std::cout<<s_inj<<std::endl;
			while(ss.good() && ss_inj.good())
			{
				int temp;
				double temp_inj;
				ss>>temp;
				ss_inj>>temp_inj;
				if(temp!=0)
					fake_traffic.push_back(std::make_pair(temp,temp_inj));
					//fake_traffic_paths.push_back(count_dest);
				count_dest++;
			}
		}
		count++;
	}
	std::cout<<"NI"<<m_id<<std::endl<<"\t";
	for(int i=0; i<fake_traffic_paths.size();i++)
	{
		std::cout<<fake_traffic[i].first<<"["<<fake_traffic[i].second <<"]  ";
	}
	std::cout<<std::endl;
#endif
	m_registred_packet_switching_path_vc=-1;

	// for vnet reuse and spurious vnet optimizations
	m_virtual_networks_protocol=network_ptr->getNumVnetProtocol();
	m_virtual_networks_spurious=network_ptr->getNumVnetSpurious();

	vnet_in_this_vc.resize(m_num_vcs);
	for(int i=0;i<vnet_in_this_vc.size();i++)
		vnet_in_this_vc[i]=-2;

	rr_vnet_ins_msg=0;

    #if USE_VICHAR==1
		//init total credit
		availSlotPerVnet.resize(m_virtual_networks_protocol);
		for(int i=0;i<m_virtual_networks_protocol;i++)
			availSlotPerVnet.at(i)=m_net_ptr->getTotVicharSlotPerVnet();
	#endif

	#if USE_APNEA_BASE==1
	//init as the baseline, eventually modified with apnea policy
		initVcOp=0;
		finalVcOp=m_num_vcs;
		initVcIp=0;
		finalVcIp=m_num_vcs;
	#endif

  #if USE_COMPRESSION == 1
    
    m_compressor=new Compressor_d(id,virtual_networks,network_ptr,this,"parallel_cmpr");
  #endif

  #if USE_DECOMPRESSION == 1
    m_decompressor=new Decompressor_d(id,virtual_networks,network_ptr,this);
  #endif
  #if COMPRESSION_TRACING == 1

  stringstream m_path;
  m_path << TRACER_PATH;
active_tracer = is_tracer_active(m_id);
  if (active_tracer)
  {

      stringstream cmd;

      m_path << "/ni" << m_id;
      cmd << "mkdir -p " << m_path.str();
      string cmd_str = cmd.str();
      assert(system(cmd_str.c_str()) != -1);

      stringstream ni_filename,ss;
      ni_filename << m_path.str() <<  "/ni"<<id<<".txt";
      m_ni_tracer=new ofstream(ni_filename.str());
      assert(m_ni_tracer->is_open());
    (*m_ni_tracer) <<"cycle;vnet latency;queueing latency;average latency;local inj;local rec;vnet inj. flits;vnet rec. flits;vnet hops"<< std::endl;

  }
  m_tracer_last_tick=0;
  m_ni_distance=0;
  m_ni_flits_injected=0;
  #endif

  #if USE_COMPRESSION_DEBUG == 1
    m_vnet_delay.resize(m_virtual_networks);
    m_vnet_total_delay.resize(m_virtual_networks);
    m_consecutives.resize(m_virtual_networks);

    for(int i=0;i<m_virtual_networks;i++)
    {
      m_vnet_delay[i]=0;
      m_vnet_total_delay[i]=0;
      m_ni_tracer=new ofstream(ni_filename.str());
      m_consecutives[i]=false;

    }
    #endif

    #if USE_HOTSPOT == 1
      hotspot_source= is_hotspot();
  	  hotspot_dest= is_hotspot_dst();
    #endif

    #if USE_NI_TRACER == 1

      long_messages_delivered.resize(m_virtual_networks);
      short_messages_delivered.resize(m_virtual_networks);
      long_packet_flits.resize(m_virtual_networks);
      short_packet_flits.resize(m_virtual_networks);
      queue_congestion.resize(m_virtual_networks);

    for(int i=0;i<m_virtual_networks;i++)
    {
      long_messages_delivered[i]=0;
      short_messages_delivered[i]=0;
      long_packet_flits[i]=0;
      short_packet_flits[i]=0;
      queue_congestion[i]=0;
    }

    #endif

    #if PROFILER == 1

   	  m_ni_inj_flits.resize(m_virtual_networks);
   	  m_ni_rec_flits.resize(m_virtual_networks);

      for(int i=0;i<m_virtual_networks;i++)
   	  {
   		    m_ni_rec_flits[i]=0;
   		    m_ni_inj_flits[i]=0;
   	  }

   	#endif
  #if USE_TRAFFIC_TRACER == 1

        stringstream traffic_filename;
        if(m_id == 0 || m_id == 5){
        traffic_filename<<"m5out/ni"<<m_id<<"_traffic.txt";
        m_traffic_tracer= new ofstream(traffic_filename.str());
        assert(m_traffic_tracer->is_open());
        }
        #endif

	#if PRIORITY_SCHED == 1
	
	m_vcs_priority.resize(m_num_vcs);
	
	for(int i=0; i<m_num_vcs;i++)
	{
		m_vcs_priority[i]=0;
	}

	#endif

}


#if PRIORITY_SCHED == 1

void
NetworkInterface_d::reset_vcs_priority()
{

 for(int i=0; i<m_num_vcs;i++)
        {
                m_vcs_priority[i]=0;
        }

}

std::deque<int>
NetworkInterface_d::get_priority_vector()
{
std::deque<int> priorities;
int max_prio=0;
        
        for(int invc=0;invc<m_num_vcs;invc++)
        {
        	if(m_vcs_priority[invc]>=max_prio) max_prio=m_vcs_priority[invc];
        }

	for(int i=max_prio;i>=0;i--)
	{
                for(int invc=0;invc<m_num_vcs;invc++)
                {
                        if(m_vcs_priority[invc]==i&&(std::find(priorities.begin(),priorities.end(),invc)==priorities.end()))
                        {
                        	priorities.push_back(invc);
                        }
                }
	}
	return priorities;

}

void
NetworkInterface_d::set_vc_priority(int vc, int prio)
{
	m_vcs_priority[vc]=prio;
}

#endif



void
NetworkInterface_d::setObjectId()
{
#if USE_PNET==1
	std::ostringstream ss;
    ss << typeid(m_container_ptr).name()<<m_container_ptr->get_id()<<"."<< typeid(this).name() << m_minor_id;
    m_objectId = ss.str();
#else
	assert(false && "\n\nImpossible to call this function when USE_PNET==0 !\n\n");
#endif

}

NetworkInterface_d::~NetworkInterface_d()
{
  #if COMPRESSION_TRACING == 1
  if (active_tracer)
      {
        (*m_ni_tracer) << 0 << " " << curCycle() << std::endl;
        m_ni_tracer->close();
        assert(!m_ni_tracer->is_open());
        delete m_ni_tracer;

      }
  #endif
    deletePointers(m_out_vc_state);
    deletePointers(m_ni_buffers);
    delete creditQueue;
    delete outSrcQueue;
    #if USE_COMPRESSION == 1
    delete m_compressor;
    #endif
    #if USE_DECOMPRESSION == 1
    delete m_decompressor;
    #endif
    #if USE_TRAFFIC_TRACER == 1
     if(m_id == 0 || m_id == 5) m_traffic_tracer->close();
	delete m_traffic_tracer;
	#endif

}

void
NetworkInterface_d::addInPort(NetworkLink_d *in_link,
                              CreditLink_d *credit_link)
{
    inNetLink = in_link;
    in_link->setLinkConsumer(this);
    m_ni_credit_link = credit_link;
    credit_link->setSourceQueue(creditQueue);
	credit_link->setLinkSource(this);
}

void
NetworkInterface_d::addOutPort(NetworkLink_d *out_link,
                               CreditLink_d *credit_link)
{
    m_credit_link = credit_link;
    credit_link->setLinkConsumer(this);

    outNetLink = out_link;
    outSrcQueue = new flitBuffer_d();
    out_link->setSourceQueue(outSrcQueue);
	out_link->setLinkSource(this);
}

void
NetworkInterface_d::addNode(vector<MessageBuffer *>& in,
                            vector<MessageBuffer *>& out)
{
    assert(in.size() == m_virtual_networks);
    for(auto it=in.begin();it!=in.end();++it)
        if(BaseCPU *p=(*it)->getCpuPointer()) p->addSlaveObject(this);
    for(auto it=out.begin();it!=out.end();++it)
        if(BaseCPU *p=(*it)->getCpuPointer()) p->addSlaveObject(this);
    inNode_ptr = in;
    outNode_ptr = out;
    for (int j = 0; j < m_virtual_networks; j++) {

        // the protocol injects messages into the NI
        inNode_ptr[j]->setConsumer(this);
        inNode_ptr[j]->setClockObj(this);
    }
}

std::vector<MessageBuffer *>
NetworkInterface_d::getOutNode()
{
	return outNode_ptr;
}

bool
NetworkInterface_d::flitisizeMessage(MsgPtr msg_ptr, int vnet)
{
    NetworkMessage *net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
    NetDest net_msg_dest = net_msg_ptr->getInternalDestination();

    // gets all the destinations associated with this message.
    vector<NodeID> dest_nodes = net_msg_dest.getAllDest();

    // Number of flits is dependent on the link bandwidth available.
    // This is expressed in terms of bytes/cycle or the flit size
    int num_flits = (int) ceil((double) m_net_ptr->MessageSizeType_to_int(
        net_msg_ptr->getMessageSize())/m_net_ptr->getNiFlitSize());
    #if USE_COMPRESSION == 1
    m_net_ptr->incrementUncompressedFlits(num_flits);
    #endif
    // loop to convert all multicast messages into unicast messages
//	if(dest_nodes.size()>1)
//	{
//		std::cout<<dest_nodes.size()<<std::endl;
//		assert(dest_nodes.size()<=1);
//	}
    for (int ctr = 0; ctr < dest_nodes.size(); ctr++)
	{
        // this will return a free output virtual channel
        int vc = calculateVC(vnet);

		//std::cout <<"vc"<<vc<<std::endl;
        if (vc == -1)
		    {
          #if USE_COMPRESSION_DEBUG == 1

          if(!m_consecutives[vnet])
            {
              m_vnet_delay[vnet]=(int)curCycle();

              m_consecutives[vnet] = true;
            }

          #endif
			//std::cout<< "NI flitisize blocked"  <<std::endl;
            return false ;
        }
        else
        {
        	#if USE_COMPRESSION_DEBUG == 1

            if(m_vnet_delay[vnet] != 0)
        	  {
        		      int distance=((int)curCycle() - m_vnet_delay[vnet]);
        		      m_vnet_total_delay[vnet]=m_vnet_total_delay[vnet]+distance;
        		      m_vnet_delay[vnet] = 0;
        		      m_consecutives[vnet] = false;
        	  }

        	#endif
        }

		#if USE_APNEA_BASE==1
		assert(vc>=initVcOp&&vc<finalVcOp);
		#endif

    #if USE_COMPRESSION == 1
        if(!m_compressor->isIdle(vc))
        {

        	return false;
        }
    #endif

        MsgPtr new_msg_ptr = msg_ptr->clone();
        NodeID destID = dest_nodes[ctr];

		//std::cout<<"\tSENDING --- Source Node: "<<m_id
		//					<<" Dest Node: "<<destID
		//					<<" Seq_id: "<<m_reoreder_counter_out[destID]
		//					<<std::endl;

        NetworkMessage *new_net_msg_ptr =
            safe_cast<NetworkMessage *>(new_msg_ptr.get());
        if (dest_nodes.size() > 1) {
            NetDest personal_dest;
            for (int m = 0; m < (int) MachineType_NUM; m++) {
                if ((destID >= MachineType_base_number((MachineType) m)) &&
                    destID < MachineType_base_number((MachineType) (m+1))) {
                    // calculating the NetDest associated with this destID
                    personal_dest.clear();
                    personal_dest.add((MachineID) {(MachineType) m, (destID -
                        MachineType_base_number((MachineType) m))});
                    new_net_msg_ptr->getInternalDestination() = personal_dest;
                    break;
                }
            }
            net_msg_dest.removeNetDest(personal_dest);
            // removing the destination from the original message to reflect
            // that a message with this particular destination has been
            // flitisized and an output vc is acquired
            net_msg_ptr->getInternalDestination().removeNetDest(personal_dest);
        }
	// START add adaptive routing reordering flits information
		// single increase for a complete message, eventually multiflits
		int this_seq_id=m_reoreder_counter_out[destID]++;
	// END add adaptive routing reordering flits information
//////////////////////////////////////////////////////////////////////
	#if USE_SYNTHETIC_TRAFFIC==1
		// SYNTHTIC TRAFFIC MODIFY LENGHT OF DATA PACKETS FOR VNET 2, VNET-0,1 HAVE 1FLIT PACKETS
		if(vnet==2)//data vnet
			num_flits=m_net_ptr->get_synthetic_data_pkt_size();
	#endif

  int hops=m_net_ptr->getDistanceFromDestination(new_msg_ptr,this, m_net_ptr->get_num_rows(),m_net_ptr->get_num_cols()); //calculate distance from source (this) to destination.
	//m_net_ptr->incrementDistanceOnVnet(vnet,hops);

  #if USE_COMPRESSION == 1

		bool compressible=false;
		 #if TRAFFIC_TRACER == 1

    if(m_id == 0 || m_id == 5)
    {
      if(vnet ==2 )
      {
      updateTracer(curCycle(),num_flits);
      }
    }
    #endif

		 
		
			if(num_flits>1) //se non è un head tail
			{

				
				if(hops>MAX_HOPS)//m_compressor->getPacketSlack(vc,hops)>=3) //if my packet has a slack of about 5 cycles (time to perform compression, i'll deliver it compressed)
				{
				m_net_ptr->increment_compressed_packet(vnet);
				int saves=0;
				assert(new_msg_ptr.get()!=NULL);
        			m_net_ptr->incrementUncompressedFlits(num_flits);
	#if USE_SYNTHETIC_TRAFFIC == 0


        //qui inizia la compressione
        saves=m_compressor->analyzeMessage(new_msg_ptr);
	//std::cout<<"saves:"<<saves<<std::endl;
	        
        		
			if(m_compressor->isZCReady())
			{
			m_net_ptr->update_compression_cycles(1);
			}
			else
			{
        			m_net_ptr->update_compression_cycles(m_compressor->getCompressionCycles());
			}
       		

        #else

				bool control=true;
      			while(control)
      			{
      				saves=m_compressor->getStatisticSaves();
      				if(saves>5||saves<0)control=true;
      				else
      					control=false;
      			}

	#endif

      			num_flits=num_flits-saves;
      			m_net_ptr->incrementCompressedFlits(num_flits);
      			compressible=true;
				}
				else
				{
				m_net_ptr->increment_uncompressed_packet(vnet);
				}
			}
		

	 #endif

   #if TRACK_TRAFFIC == 1
         
   if(num_flits > 1)
   {	m_net_ptr->incrementInjFlits(num_flits);
        m_net_ptr->incrementInjDataPackets();
     //pacchetti data
   }else
   {	
         RequestMsg *traffic_msg_ptr =
         dynamic_cast<RequestMsg *>(new_msg_ptr.get());
         if(traffic_msg_ptr != NULL)
         {
          m_net_ptr->categorizeInjRequestType(traffic_msg_ptr);
         }
         else
         {
           ResponseMsg* resp_msg=dynamic_cast<ResponseMsg*>(new_msg_ptr.get());
           m_net_ptr->categorizeInjResponseType(resp_msg);
         }
   }

   #endif

	#if USE_NI_TRACER == 1
        if(num_flits>1)
        {
        updateLongMessages(vnet);
        updateLongPacket(vnet);
        }
        else
        {
        updateShortMessages(vnet);
        updateShortPacket(vnet);
        }
        #endif
	#if PRIORITY_SCHED == 1
	set_vc_priority(vc,hops);
	#endif
	//////////////std::cout<<"num_flits="<<num_flits<<std::endl;
        for (int i = 0; i < num_flits; i++)
		      {
			
			m_net_ptr->incrementDistanceOnVnet(vnet,hops);
			
			m_net_ptr->increment_injected_flits(vnet);
            flit_d *fl = new flit_d(i, vc, vnet, num_flits, new_msg_ptr,
                /*m_net_ptr*/this->curCycle());
			
			fl->set_priority(hops);
		
		// START add adaptive routing reordering flits information
			fl->setSourceSeqID(this_seq_id);
			fl->setSourceNodeID(m_id);
		// END add adaptive routing reordering flits information
			//std::cout<<"NI:"<<m_id<<" vnet:"<<vnet<<" Q-delay:"<< this->curCycle() - msg_ptr->getTime()<<std::endl;
            fl->set_delay(/*m_net_ptr*/this->curCycle() - msg_ptr->getTime());

			fl->set_vnet(vnet);

			//allocated on an adaptive vc or escaped one
			if	((vc % m_vc_per_vnet) <  m_vc_per_vnet -m_net_ptr->getAdaptiveRoutingNumEscapePaths() )
				fl->setIsAdaptive(1);//set adaptivity at the beginning
			else
				fl->setIsAdaptive(0);

        #if USE_COMPRESSION == 1

          if(compressible)
          {

            if(fl->get_type()==TAIL_)
            {
                fl->set_compressed(true);
			if(m_compressor->isZCReady())fl->set_zero_comp();
            }

            m_compressor->fillCompressor(vc,fl);


          }
          else
          {

            //m_net_ptr->increment_injected_flits(vnet);//FIXME
            m_ni_flits_injected++;
            m_ni_buffers[vc]->insert(fl);
          }

      #else
            //m_net_ptr->increment_injected_flits(vnet);//FIXME
            m_ni_flits_injected++;
            m_ni_buffers[vc]->insert(fl);

        #endif
        }
   
        #if USE_COMPRESSION == 1
	        
          if(compressible){

           	    compressible=false;
		if(m_compressor->isZCReady())
		{
		 Tick busy_time=curTick()+1000;
		m_compressor->reset_ready_bit();
                m_compressor->prepareForCompression(vc,busy_time,vc);
                m_compressor->scheduleEvent(1);//schedulo tra 1 cicl0
		
		}
		else
            	{
		Tick busy_time=curTick()+(m_compressor->getCompressionCycles())*clockPeriod();
            	m_compressor->prepareForCompression(vc,busy_time,vc);
            	m_compressor->scheduleEvent(m_compressor->getCompressionCycles());//schedulo tra 5 cicli
		}
    		}
		m_compressor->updateSlackQueue(vc,hops);
    	#endif


		if(num_flits==1)
			m_net_ptr->increment_injected_short_pkt(vnet);
		else
			m_net_ptr->increment_injected_long_pkt(vnet);

		// queue dealy before insertion in the NoC as flits
		if(msg_ptr->getT_StartCycle()<old_get_curCycle)
            /* It seems to never get here */
			c1_ni_matrix.at(vc).t_start=old_get_curCycle;
		else
			c1_ni_matrix.at(vc).t_start=msg_ptr->getT_StartCycle();

        m_ni_enqueue_time[vc] = /*m_net_ptr*/this->curCycle();

		vnet_in_this_vc[vc]=vnet;
        m_out_vc_state[vc]->setState(ACTIVE_, /*m_net_ptr*/this->curCycle());
    }
    return true ;
}

// Looking for a free output vc
int
NetworkInterface_d::calculateVC(int vnet)
{
	//if(m_id==0)
	//	std::cout<<"@"<<curTick()<<" NIC"<<m_id<<" vnet"<<vnet<<std::endl;
#if USE_ADAPTIVE_ROUTING == 0
//do not use adaptive routing
		int delta = 0;//m_vc_allocator[vnet];
		int outvc_base = vnet*m_vc_per_vnet;
		int num_vcs_to_analyze = m_vc_per_vnet;

		//#if USE_WH_VC_SA==1
	#if USE_VNET_REUSE==1
		/*
		* check if the used vcs for the vnet packets are used. Suppose 2vcs per
 		* vnet, with vnet_reuse_opt. This means you can use up to 2 vcs
 		* regardless the vnet they own (pay attention if the vnet is ordered or
 		* not). In this way you are fair and avoid deadlock. If you want to be
 		* more aggressive, you can use for packet from a specific vnet all the
 		* available vcs leaving at least 1 vc per each vnet other than the
 		* considered vnet. This enhance flexibility while reduce fairness.*/

	#if USE_APNEA_BASE==1
		//used only with vnet_reuse and use a subset of vcs depending on the
		//state of the apnea policy
		outvc_base=initVcOp;
		num_vcs_to_analyze=finalVcOp;
	#else
		outvc_base=0; // the offset_vc will be added later
		num_vcs_to_analyze=m_num_vcs;
	#endif

		std::vector<int> count_vc_used_vnet;
		count_vc_used_vnet.resize(m_virtual_networks_protocol);
		int idle_vcs=0;
	#if USE_APNEA_BASE==1
		for (int outvc_offset_iter = initVcOp; outvc_offset_iter < finalVcOp; outvc_offset_iter++)
		{
			if(m_out_vc_state[outvc_offset_iter]->isInState(IDLE_, this->curCycle()))
				idle_vcs++;
			else
			{
				assert(vnet_in_this_vc[outvc_offset_iter]!=-2);
				count_vc_used_vnet[vnet_in_this_vc[outvc_offset_iter]]++;
			}
		}
	#else

		for (int outvc_offset_iter = 0; outvc_offset_iter < num_vcs_to_analyze; outvc_offset_iter++)
		{
			if(m_out_vc_state[outvc_offset_iter]->isInState(IDLE_, this->curCycle()))
				idle_vcs++;
			else
			{
				assert(vnet_in_this_vc[outvc_offset_iter]!=-2);
				count_vc_used_vnet[vnet_in_this_vc[outvc_offset_iter]]++;
			}
		}
	#endif
		//policy is aggressive use of vcs. At least 1vc used/usable per protocol vnet
		for(int qiter=0; qiter<m_virtual_networks_protocol;qiter++)
		{// check the requiring vnet only, since it is the only that can acquire a vc
			if (vnet==qiter)
			{
				if(count_vc_used_vnet[qiter]<m_vc_per_vnet+m_virtual_networks_spurious)
				{//at least one vc could be allocated if the other vnets agreed
					assert(count_vc_used_vnet[qiter]>=0);
					continue;
				}
				else
					return -1;// this vnet reached the maximum number of allowable vcs
			}
			else
			{// other vnet at least 1
				if(count_vc_used_vnet[qiter]>0)
				{
					assert(count_vc_used_vnet[qiter]<=m_vc_per_vnet+m_virtual_networks_spurious);
					continue;
				}
				else if (idle_vcs>1)
					idle_vcs--; // 1 idle vc for this vnet +1 for the eventually stored in this round by vnet
				else
					return -1; //impossible to assign at least 1 vc to each vnet
			}
		}
		#endif
		//#endif
		for (int outvc_offset_iter = 0; outvc_offset_iter < num_vcs_to_analyze;outvc_offset_iter++)
		{
			if (m_out_vc_state[ outvc_base + delta]->isInState(IDLE_, /*m_net_ptr*/this->curCycle()))
			{
			#if USE_VICHAR==1
				if(!(getAvailSlotPerVnet(vnet)>0))
					continue;
			#endif
				m_vc_allocator[vnet]=delta+1;
				if(m_vc_allocator[vnet] >= num_vcs_to_analyze)
			        m_vc_allocator[vnet] = 0;

				//vnet_in_this_vc[outvc_offset_iter]=vnet;
				#if USE_APNEA_BASE==1
				assert((outvc_base+delta)>=initVcOp && (outvc_base+delta)<finalVcOp);
				#endif
				return (outvc_base + delta);
			}
			delta++;
			if(delta>=num_vcs_to_analyze)
				delta=0;
        }
        return -1;
#else
	int low_vc,high_vc;
	int num_escape_path_vcs=m_net_ptr->getAdaptiveRoutingNumEscapePaths(); // FIXME
	// set the vc available if the flit is adaptive or deterministic routing //
	//if(curTick()<1000000/*always adaptive FOR TEST /t_flit->getIsAdaptive()*/)
	{
		low_vc=0;
		high_vc=m_vc_per_vnet;
		//high_vc=m_vc_per_vnet-num_escape_path_vcs;

	}

	int num_vcs_per_vnet = high_vc-low_vc;//m_vc_per_vnet;
  	assert(low_vc>=0 && high_vc>low_vc);
  //std::cout<<"low_vc: "<<low_vc<<" high_vc: "<<high_vc<<std::endl;
///////////////////////////////////////////////////////////////////////////
    for (int i = 0; i < num_vcs_per_vnet; i++)
	{
    	int delta = m_vc_allocator[vnet];
		if(delta >= high_vc || delta<low_vc/*num_vcs_per_vnet*/)
			        delta = low_vc;
    	m_vc_allocator[vnet]++;
		if(m_vc_allocator[vnet] >= high_vc || m_vc_allocator[vnet]<low_vc/*num_vcs_per_vnet*/)
			        m_vc_allocator[vnet] = low_vc;
		if (m_out_vc_state[(vnet*m_vc_per_vnet) + delta]->isInState(
		    IDLE_, /*m_net_ptr*/this->curCycle()))
		{
				//std::cout<<	"NetworkInterface"<<delta<<std::endl;
		        return ((vnet*m_vc_per_vnet) + delta);
		}
    }
	return -1;
#endif

}

/*
 * The NI wakeup checks whether there are any ready messages in the protocol
 * buffer. If yes, it picks that up, flitisizes it into a number of flits and
 * puts it into an output buffer and schedules the output link. On a wakeup
 * it also checks whether there are flits in the input link. If yes, it picks
 * them up and if the flit is a tail, the NI inserts the corresponding message
 * into the protocol buffer. It also checks for credits being sent by the
 * downstream router.
 */

void
NetworkInterface_d::wakeup()
{

	//std::cout<<"wake up ni "<<m_id<<std::endl;

    DPRINTF(RubyNetwork, "m_id: %d woke up at time: %lld\n",
            m_id, /*m_net_ptr*/this->curCycle());

    MsgPtr msg_ptr;

    // Checking for messages coming from the protocol
    // can pick up a message/cycle for each virtual net

	// old version not fair vnet 0 has always priority using cutbuf
	//for (int vnet = 0; vnet < m_virtual_networks; vnet++)
	//for (int vnet = m_virtual_networks-1; vnet >=0; vnet--)

	rr_vnet_ins_msg++;
	if(rr_vnet_ins_msg>=m_virtual_networks)
		rr_vnet_ins_msg=0;

	int vnet=rr_vnet_ins_msg;
    for (int iter_vnet = 0; iter_vnet < m_virtual_networks_protocol; iter_vnet++)
	{
		vnet++;
		if(vnet>=m_virtual_networks_protocol)
			vnet=0;

#if MIX_REAL_NOISE_TRAFFIC == 1
		double actual_avg_dummy_flit=(double)1000*sent_dummy_flit[vnet]/(double)(curTick()-start_tick_dummy);
#endif
		if(inNode_ptr[vnet]->isReady())
		{
			//if(m_id==0)
			//	std::cout<<"\t\t@"<<curTick()<<" NI"<<m_id<<" VNET"<<vnet<<" SHOULD BE READY"<<std::endl;

			#if DEBUG_PRINT_NOISE==1
			if((m_id==1 || m_id==2 ||m_id==6))
				std::cout<<"@"<<curTick()<<"NI"<<m_id
						<<" actual_avg_dummy_flit"<<actual_avg_dummy_flit
						<<" VNET"<<vnet<<" REAL traffic"<<std::endl;
			#endif
			while (inNode_ptr[vnet]->isReady())
			{ // Is there a message waiting
				msg_ptr = inNode_ptr[vnet]->peekMsgPtr();
    	        if (flitisizeMessage(msg_ptr, vnet))
				{
    	        	inNode_ptr[vnet]->pop();
					break; //at most one msg/cycle/vnet
					//if(m_id==0)
					//std::cout<<"\t@"<<curTick()<<" NI"<<m_id<<" inj-msg to VNET"<<vnet<<std::endl;

    	        }
				else
				{
    	            break;
    	        }
    	    }
		}
#if MIX_REAL_NOISE_TRAFFIC == 1
		assert(false && "trying to mix noise traffic");
		else if(send_dummy_flit==1 && vnet>=0 && vnet <3)
		{// inject dummy traffic
			#if DEBUG_PRINT_NOISE==1
			if((m_id==1 || m_id==2 ||m_id==6))
				std::cout<<"@"<<curTick()<<"NI"<<m_id
						<<" actual_avg_dummy_flit"<<actual_avg_dummy_flit
						<<" VNET"<<vnet<<" NOISE traffic"<<std::endl;
			#endif
			if(!(m_id==1 || m_id==2 ||m_id==6))
				continue;

			if(curTick()>end_tick_dummy)
				send_dummy_flit=false;
			if(curTick()<start_tick_dummy)
				continue;

			scheduleEvent(1);
			//enforce the dumy injection behavior
			//std::cout<<"@"<<curTick()<<" required_avg_dummy_flit:"<<required_avg_dummy_flit
			//			<<" actual_avg_dummy_flit:"<<actual_avg_dummy_flit;
			if(required_avg_dummy_flit<actual_avg_dummy_flit)
			{
				//std::cout<<" skip insertion"<<std::endl;
				continue;
			}
			//std::cout<<" try to insert dummy flit";
			MachineID m_machineID;
			m_machineID.type = MachineType_L1Cache;
	  		m_machineID.num = m_id;


			MachineID mach = {MachineType_L1Cache, 14};
 			RequestMsg *out_msg = new RequestMsg(/*m_net_ptr*/this->curCycle());
			//(*out_msg).m_Address = addr;
			//(*out_msg).m_Type = CoherenceRequestType_GETS;
			(*out_msg).m_Requestor = m_machineID;
			((*out_msg).m_Destination).add(mach);
			//(*out_msg).m_MessageSize = MessageSizeType_Control;
			//(*out_msg).m_Prefetch = ((*in_msg_ptr)).m_Prefetch;
			//(*out_msg).m_AccessMode = ((*in_msg_ptr)).m_AccessMode;
			//((*m_L1Cache_requestFromL1Cache_ptr)).enqueue(out_msg, m_l1_request_latency);
			msg_ptr = out_msg;
			NetworkMessage *net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
			int vc = calculateVC(vnet);
			if(vc==-1)
			{
			//	delete out_msg;
				//std::cout<<" NO FREE VCs TO INSERT"<<std::endl;
				continue;
			}
			else
			{

				//std::cout<<"\tINSERTED @"<<curTick()<<" NI"<<m_id
				//		<<" VNET"<<vnet
				//		<<" VC"<<vc
				//		<<" sending dummy flit"<<std::endl;
				int num_flits=2;
				sent_dummy_flit[vnet]+=num_flits;


		        for (int i = 0; i < num_flits; i++)
				{
	           		m_net_ptr->increment_injected_flits(vnet);
	           		flit_d *fl = new flit_d(i, vc, vnet, num_flits, net_msg_ptr,/*m_net_ptr*/this->curCycle());
			   		 fl->setSourceSeqID(-1);
			   		 fl->setSourceNodeID(m_id);
	           		 fl->set_delay(/*m_net_ptr*/this->curCycle() - msg_ptr->getTime());
	           		 m_ni_buffers[vc]->insert(fl);
	        	}
			    m_ni_enqueue_time[vc] = /*m_net_ptr*/this->curCycle();
	        	m_out_vc_state[vc]->setState(ACTIVE_, /*m_net_ptr*/this->curCycle());
			}
		}
#endif // MIX_REAL_NOISE_TRAFFIC

    }

    scheduleOutputLink();
    checkReschedule();
    
    /*********** Picking messages destined for this NI **********/
    Resynchronizer *ra=m_ni_credit_link->getReshnchronizer();
    if(ra->isAcknowledgeAvailable()) ra->clearAcknowledge();
    if(ra->isAcknowkedgePending())
	{
        //reschedule itself waiting for credit link to be free
		// this is mandatory, since I can read iff the credit link is available,
		// otherwise I cannot signal the credit update
        ra->advanceTransmitSide();
		scheduleEvent(1);
	}
	else
	{
       Resynchronizer *rb=inNetLink->getReshnchronizer();
 	   if (inNetLink->isReady(/*m_net_ptr*/this->curCycle()) && rb->isRequestAvailable())
 	   {
            rb->clearRequest();
            rb->sendAcknowledge();
			// get data/ctrl flit [not credit]
            flit_d *t_flit = inNetLink->consumeLink();

			assert(t_flit->get_vnet()==t_flit->get_vnet());

            bool free_signal = false;
            if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_)
			{
               free_signal = true;
				// added to check pkt_switching with packet long and short
				// degrades performance
				NetworkMessage *net_msg_ptr_temp = safe_cast<NetworkMessage *>(t_flit->get_msg_ptr().get());
			    int num_flits_temp = (int) ceil((double) m_net_ptr->MessageSizeType_to_int(
        				( net_msg_ptr_temp->getMessageSize()))/m_net_ptr->getNiFlitSize());

				int network_delay_temp = /*m_net_ptr*/this->curCycle() - //FIXME: check
                               t_flit->get_enqueue_time();

				//assert(this->curCycle()>t_flit->get_enqueue_time());
            	int queueing_delay_temp = t_flit->get_delay();
				if(num_flits_temp==m_net_ptr->MessageSizeType_to_int(MessageSizeType_Control)/m_net_ptr->getNiFlitSize())
				{
					num_ctrl_pkts++;
					latency_ctrl_pkts_net+=network_delay_temp;
					latency_ctrl_pkts_queue+=queueing_delay_temp;
				}
				else if(num_flits_temp==m_net_ptr->MessageSizeType_to_int(MessageSizeType_Data)/m_net_ptr->getNiFlitSize())
				{
					num_data_pkts++;
					latency_data_pkts_net+=network_delay_temp;
					latency_data_pkts_queue+=queueing_delay_temp;
				}
				else
					panic("ops");
				//std::cout<<num_data_pkts<<" "
				//		<<latency_data_pkts_net<<" "
				//		<<latency_data_pkts_queue<<"\t"
				//		<<num_ctrl_pkts<<" "
				//		<<latency_ctrl_pkts_net<<" "
				//		<<latency_ctrl_pkts_queue<<std::endl;

				//////////////////////////////////////////////////////////////////////////////////////
        #if TRACK_TRAFFIC == 1
        NetworkMessage *net_msg_ptr_temp_2 = safe_cast<NetworkMessage *>(t_flit->get_msg_ptr().get());
          int num_flits_temp2 = (int) ceil((double) m_net_ptr->MessageSizeType_to_int(
                ( net_msg_ptr_temp_2->getMessageSize()))/m_net_ptr->getNiFlitSize());
        
	if(num_flits_temp2 > 1)
        {
        m_net_ptr->incrementRecFlits(num_flits_temp2);     
	m_net_ptr->incrementRecDataPackets();
          //pacchetti data
        }else
        {
              RequestMsg *traffic_msg_ptr =
              dynamic_cast<RequestMsg *>(t_flit->get_msg_ptr().get());
              if(traffic_msg_ptr != NULL)
              {
               m_net_ptr->categorizeRecRequestType(traffic_msg_ptr);
              }
              else
              {
                ResponseMsg* resp_msg=dynamic_cast<ResponseMsg*>(t_flit->get_msg_ptr().get());
                m_net_ptr->categorizeRecResponseType(resp_msg);
              }
        }

        #endif

		#if USE_ADAPTIVE_ROUTING==1
			// START extended for adaptive routing support check seq_id
			//	std::cout<<"This Node: "<<m_id
			//			<<" Source Node: "<<t_flit->getSourceNodeID()
			//			<<" Expected seq_id:"<<m_reoreder_counter_in[t_flit->getSourceNodeID()]
			//			<<" flit seq_id: "<<t_flit->getSourceSeqID()
			//			<<" isStrictFIFO: "<<outNode_ptr[t_flit->get_vnet()]->getIsStrictFifo()
			//			<<" isOrderingSet: "<<outNode_ptr[t_flit->get_vnet()]->getIsOrderingSet()
			//			<<std::endl;
				// check outoforder msges if this protocol queue is ordered

			//	FIXME assert(m_reoreder_counter_in[t_flit->getSourceNodeID()]==t_flit->getSourceSeqID()
			//				|| outNode_ptr[t_flit->get_vnet()]->getIsStrictFifo()==false);

				m_reoreder_counter_in[t_flit->getSourceNodeID()]++;
		#endif
			// END extended for adaptive routing support check seq_id

				if(t_flit->getSourceSeqID()!=-1)
				{
          #if USE_SPURIOUS_VC_VNET_REUSE == 0

						#if USE_DECOMPRESSION == 1

								if(t_flit->get_compressed())
									{
								        m_decompressor->fillDecompressor(t_flit->get_vc(),t_flit);

									}
									else
									{
										outNode_ptr[t_flit->get_vnet()]->enqueue(t_flit->get_msg_ptr(), 1);
									}

						#else

							outNode_ptr[t_flit->get_vnet()]->enqueue(t_flit->get_msg_ptr(), 1);

						#endif

					#else
					outNode_ptr[t_flit->get_vnet()]->enqueue(t_flit->get_msg_ptr(), 1);
					#endif
				}
				//else
				//	std::cout<<"@"<<curTick()<<" NI"<<m_id<<" Arrived dummy flit"<<std::endl;
            }
            // Simply send a credit back since we are not buffering
            // this flit in the NI

            m_ni_credit_link->getReshnchronizer()->sendRequest();

            flit_d *credit_flit = new flit_d(t_flit->get_vc(), free_signal,
                                            /*m_net_ptr*/this->curCycle());
            creditQueue->insert(credit_flit);
            m_ni_credit_link->scheduleEvent(1);

        #if USE_DECOMPRESSION == 1 //era 0 prima

                        	if(!t_flit->get_compressed())
                        	{

            					      int vnet = t_flit->get_vnet();
            					      m_net_ptr->increment_received_flits(vnet);

                            #if PROFILER == 1
                        		m_ni_rec_flits[vnet]++;
                        		#endif
					 if(t_flit->get_type() == HEAD_TAIL_)
                                m_net_ptr->increment_received_short_pkt(vnet);
	                        else if(t_flit->get_type() == TAIL_)
                                m_net_ptr->increment_received_long_pkt(vnet);

                            int network_delay = /*m_net_ptr*/this->curCycle() - t_flit->get_enqueue_time();
                        		int queueing_delay = t_flit->get_delay();
                        		m_net_ptr->increment_network_latency(network_delay, vnet);
                        		m_net_ptr->increment_queueing_latency(queueing_delay, vnet);
					
                        	delete t_flit;

            				}

            #else
            int vnet = t_flit->get_vnet();
            m_net_ptr->increment_received_flits(vnet);



			if(t_flit->get_type() == HEAD_TAIL_)
				m_net_ptr->increment_received_short_pkt(vnet);
			else if(t_flit->get_type() == TAIL_)
				m_net_ptr->increment_received_long_pkt(vnet);

            int network_delay = /*m_net_ptr*/this->curCycle() - //FIXME: check
                               t_flit->get_enqueue_time();
            int queueing_delay = t_flit->get_delay();
            m_net_ptr->increment_network_latency(network_delay, vnet);
            m_net_ptr->increment_queueing_latency(queueing_delay, vnet);
            delete t_flit;
      #endif
 	   }
       //Retry, you may have better luck next time
       if(rb->isRequestPending())
       {
           rb->advanceReceiveSide();
           scheduleEvent(1);
       }
	}
    /****************** Checking for credit link *******/

    Resynchronizer *rc=m_credit_link->getReshnchronizer();
    if (m_credit_link->isReady(/*m_net_ptr*/this->curCycle()) && rc->isRequestAvailable())
	{
        rc->clearRequest();
        rc->sendAcknowledge();
        flit_d *t_flit = m_credit_link->consumeLink();
	
        m_out_vc_state[t_flit->get_vc()]->increment_credit();
		#if USE_VICHAR==1
			incrAvailSlotPerVnet(t_flit->get_vc()/m_vc_per_vnet);
		#endif
        if (t_flit->is_free_signal())
		{
            m_out_vc_state[t_flit->get_vc()]->setState(IDLE_, /*m_net_ptr*/this->curCycle());
		//forse va messo qui il set priority 0
		
			/*c1 final time stamp*/
			//assert(c1_ni_matrix.at(t_flit->get_vc()).t_start!=Cycles(0));
			//c1_ni_matrix.at(t_flit->get_vc()).count
			//	+=m_net_ptr->curCycle()-c1_ni_matrix.at(t_flit->get_vc()).t_start;
			//c1_ni_matrix.at(t_flit->get_vc()).t_start=Cycles(0);

        }

        delete t_flit;
    }
    //Retry, you may have better luck next time
    if(rc->isRequestPending())
    {
        rc->advanceReceiveSide();
        scheduleEvent(1);
    }

#if COMPRESSION_TRACING == 1

   if(active_tracer)
   {
      if(m_tracer_last_tick==0)
      {
        scheduleEvent(TRACER_SAMPLE_CYCLES);
        m_tracer_last_tick=curCycle();
      }
      else
      {
        if((curCycle()-m_tracer_last_tick)>=TRACER_SAMPLE_CYCLES)
        {
          m_tracer_last_tick=curCycle();


        scheduleEvent(TRACER_SAMPLE_CYCLES);
        }
      }
    }
#endif
}
/** This function looks at the NI buffers
 *  if some buffer has flits which are ready to traverse the link in the next
 *  cycle, and the downstream output vc associated with this flit has buffers
 *  left, the link is scheduled for the next cycle
 */
void
NetworkInterface_d::scheduleOutputLink()
{
  #if PRIORITY_SCHED == 0 
    int vc = m_vc_round_robin;
	#else
	int vc=0;
	#endif
  // int vc = 0;	//in this case, I'll give priority to control packets, so as to cover compression delay
	//// m_vc_round_robin++;
   //// if (m_vc_round_robin == m_num_vcs)
   ////     m_vc_round_robin = 0;

	//std::cout<<"NI"<<m_id<<" vc"<<vc<<std::endl;
	/*set link busy until here to prevent processing if link busy*/
    Resynchronizer *r=outNetLink->getReshnchronizer();
    if(r->isAcknowledgeAvailable()) r->clearAcknowledge();
	if(r->isAcknowkedgePending())
	{
        r->advanceTransmitSide();
        //reschedule itself waiting for link to be free
		scheduleEvent(1);
		return;
	}
//////// packet switching /////////////
	bool flag_pkt_sw=false;
#if USE_WH_VC_SA == 1
	if(m_registred_packet_switching_path_vc!=-1) //we have a reserved vc path
	{
        if (m_ni_buffers[m_registred_packet_switching_path_vc]->isReady(this->curCycle()) &&
            m_out_vc_state[m_registred_packet_switching_path_vc]->has_credits())
        {
            flag_pkt_sw=true;
        }
        else
        {
            m_registred_packet_switching_path_vc = -1;
        }
		//if(m_id==32)
		//{
		//	std::cout<<"@"<<curTick()<<" NIC"<<m_id<<"USING \tresVC"<<m_registred_packet_switching_path_vc<<std::endl;
		//}
	}
#endif
////////////////////////////////////////
	if(flag_pkt_sw==false)
	{
	#if PRIORITY_SCHED == 1
	std::deque<int> prio=get_priority_vector();


	for (int p = 0; p < prio.size(); p++)
                {
            vc=prio[p];
	    
            // model buffer backpressure
            if (m_ni_buffers[vc]->isReady(/*m_net_ptr*/this->curCycle()) && m_out_vc_state[vc]->has_credits())
                        {
			
                bool is_candidate_vc = true;
                int t_vnet = get_vnet(vc);
                int vc_base = t_vnet * m_vc_per_vnet;

                    for (int vc_offset = 0; vc_offset < m_vc_per_vnet;vc_offset++)
                                        {
                        int t_vc = vc_base + vc_offset;
                        if (m_ni_buffers[t_vc]->isReady(this->curCycle()))
                                                {
                            if (m_ni_enqueue_time[t_vc] < m_ni_enqueue_time[vc])
                                                        {
                                is_candidate_vc = false;
				
                                break;
                            }
                        }
                    }
                
                if (!is_candidate_vc)
                        {
                    continue;
                        }
                outNetLink->getReshnchronizer()->sendRequest();
                m_out_vc_state[vc]->decrement_credit();
                //if(m_id==0) std::cout<<"\nwinner vc for output link:"<<vc<<std::endl;             
                // Just removing the flit
                flit_d *t_flit = m_ni_buffers[vc]->getTopFlit();
		//if(m_id==0)std::cout<<"removed flit"<<std::endl;
                                // +1 not delay since networklink must update this value for the linkconsumer
                t_flit->set_time(/*m_net_ptr*/this->curCycle() + 1);

                outSrcQueue->insert(t_flit);
             
                // schedule the out link
                outNetLink->scheduleEvent(1); //we fire to source freq to the link while we set linkbusy using resyn to

                if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_)
                                          {
                    m_ni_enqueue_time[vc] = INFINITE_;
                    m_vcs_priority[vc] = 0;                  
                }
                return;
            }
        }

	#else
    	for (int i = 0; i < m_num_vcs; i++)
		{
    	    vc++;
    	    if (vc == m_num_vcs)
    	        vc = 0;
			//std::cout<<"NI"<<m_id<<" vc"<<vc<<std::endl;

    	    // model buffer backpressure
		#if USE_VICHAR==1
    	    if (m_ni_buffers[vc]->isReady(/*m_net_ptr*/this->curCycle()) && getAvailSlotPerVnet(vc/m_vc_per_vnet)>0)
		#else
    	    if (m_ni_buffers[vc]->isReady(/*m_net_ptr*/this->curCycle()) && m_out_vc_state[vc]->has_credits())
		#endif
			{
	//	if(m_id==0)std::cout<<" vc:"<<vc<<std::endl;
    	        bool is_candidate_vc = true;
    	        int t_vnet = get_vnet(vc);
    	        int vc_base = t_vnet * m_vc_per_vnet;

    	        //if (m_net_ptr->isVNetOrdered(t_vnet))
			{				
    	            for (int vc_offset = 0; vc_offset < m_vc_per_vnet;vc_offset++)
					{
    	                int t_vc = vc_base + vc_offset;
    	                if (m_ni_buffers[t_vc]->isReady(/*m_net_ptr*/this->curCycle()))
						{
    	                    if (m_ni_enqueue_time[t_vc] < m_ni_enqueue_time[vc])
							{
    	                        is_candidate_vc = false;
    	                        break;
    	                    }
    	                }
    	            }
    	        }
    	        if (!is_candidate_vc)
		    	{
    	            continue;
		    	}
				#if USE_WH_VC_SA == 1
					m_registred_packet_switching_path_vc = vc;
					//if(m_id==32)
 					//	std::cout<<"@"<<curTick()<<" NIC"<<m_id<<" REGISTERING \tresVC"<<m_registred_packet_switching_path_vc<<std::endl;
				#endif
				outNetLink->getReshnchronizer()->sendRequest();


    	        m_out_vc_state[vc]->decrement_credit();
				#if USE_VICHAR==1
					decrAvailSlotPerVnet(vc/m_vc_per_vnet);
				#endif

				m_vc_round_robin=vc;

    	        // Just removing the flit
    	        flit_d *t_flit = m_ni_buffers[vc]->getTopFlit();
				// +1 not delay since networklink must update this value for the linkconsumer
    	        t_flit->set_time(/*m_net_ptr*/this->curCycle() + 1);

    	        outSrcQueue->insert(t_flit);
              #if PROFILER == 1

                m_ni_inj_flits[get_vnet(vc)]++;

              #endif
    	        // schedule the out link
    	        outNetLink->scheduleEvent(1); //we fire to source freq to the link while we set linkbusy using resyn to

    	        if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_)
				          {
    	            m_ni_enqueue_time[vc] = INFINITE_;
			

					// packet switching WH_VC_SA
				#if USE_WH_VC_SA == 1
					//if(m_id==32)
 					//	std::cout<<"@"<<curTick()<<" NIC"<<m_id<<" RELEASING1 \tresVC"<<m_registred_packet_switching_path_vc<<std::endl;
					m_registred_packet_switching_path_vc=-1;
				#endif


    	        }
    	        return;
    	    }
 	}
#endif
	}//end if(flag_pkt_sw==0)
	else
	{
	std::cout<<"pkt false"<<std::endl;
		//use the saved vc for the registered path
		vc=m_registred_packet_switching_path_vc;

		if (m_ni_buffers[vc]->isReady(/*m_net_ptr*/this->curCycle()) && m_out_vc_state[vc]->has_credits())
		{
			outNetLink->getReshnchronizer()->sendRequest();

        	m_out_vc_state[vc]->decrement_credit();
        	// Just removing the flit
        	flit_d *t_flit = m_ni_buffers[vc]->getTopFlit();
			// +1 not delay since networklink must update this value for the linkconsumer
        	t_flit->set_time(/*m_net_ptr*/this->curCycle() + 1);

        	outSrcQueue->insert(t_flit);
				#if USE_VICHAR==1
					decrAvailSlotPerVnet(vc/m_vc_per_vnet);
				#endif

        	// schedule the out link
        	outNetLink->scheduleEvent(1); //we fire to source freq to the link while we set linkbusy using resyn to

			m_vc_round_robin = vc;
        	if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_)
			{
        	    m_ni_enqueue_time[vc] = INFINITE_;
				// packet switching WH_VC_SA
				//if(m_id==32)
				//	std::cout<<"@"<<curTick()<<" NIC"<<m_id<<" RELEASING2 \tresVC"<<m_registred_packet_switching_path_vc<<std::endl;
				m_registred_packet_switching_path_vc=-1;

				//update vc round_robin information

        	}
        	return;
		}
	}
}

int
NetworkInterface_d::get_vnet(int vc)
{
    for (int i = 0; i < m_net_ptr->getNumberOfVirtualNetworks(); i++) {
        if (vc >= (i*m_vc_per_vnet) && vc < ((i+1)*m_vc_per_vnet)) {
            return i;
        }
    }
    fatal("Could not determine vc");
}

void NetworkInterface_d::changePeriod(Tick period)
{
    ClockedObject::changePeriod(period);

    /*
     * We can't really change our period if not on a clock edge.
     * We will be called again by a PeriodChangeEvent generated
     * by the clocked object.
     */
    if(curTick()!=clockEdge()) return;

    outNetLink->changePeriod(period);
    m_ni_credit_link->changePeriod(period);
}

void
NetworkInterface_d::checkReschedule()
{
    for (int vnet = 0; vnet < m_virtual_networks; vnet++) {
        if (inNode_ptr[vnet]->isReady()) { // Is there a message waiting
            scheduleEvent(1);
            return;
        }
    }
    for (int vc = 0; vc < m_num_vcs; vc++) {
        if (m_ni_buffers[vc]->isReadyForNext(/*m_net_ptr*/this->curCycle())) {
            scheduleEvent(1);
            return;
        }
    }

}

void
NetworkInterface_d::print(std::ostream& out) const
{
    out << "[Network Interface]";
}
#if COMPRESSION_TRACING == 1
bool
NetworkInterface_d::is_tracer_active(int nic_id)
{
	string line;
	int x;
	int count = 0;
	bool found = false;
	ifstream buf_usage (TRACER_CSV_PATH);
	assert(buf_usage.is_open());
	while (getline (buf_usage,line) && !found)
	{
		std::istringstream stream(line);
		while(stream >> x)
		{
			if (x == 1 && count == nic_id)
			{
				found = true;
				break;
			}
			count ++;
		}
	}
	buf_usage.close();
	return found;
}
#endif

#if USE_HOTSPOT == 1
bool
NetworkInterface_d::is_hotspot() //tests if this node is a source hotspot
{
string line;
    int x;

    int count = 0;
    bool found = false;
    ifstream buf_usage (HOTSPOT_SRC);
    assert(buf_usage.is_open());
    while (getline (buf_usage,line) && !found)
    {
        std::istringstream stream(line);
        while(stream >> x)
        {
            if (x && count == m_id) //REMEMBER THAT X RANGE IS FROM 1 TO N, NOT FROM 0 TO N-1
            {
                found = true;
                break;
            }
            count ++;
        }
    }
    buf_usage.close();

    return found;

}

bool
NetworkInterface_d::is_hotspot_dst() //tests if this node is a source hotspot
{
string line;
    int x;

    int count = 0;
    bool found = false;
    ifstream buf_usage (HOTSPOT_DST);
    assert(buf_usage.is_open());
    while (getline (buf_usage,line) && !found)
    {
        std::istringstream stream(line);
        while(stream >> x)
        {
            if (x!=0 && count == m_id) //REMEMBER THAT X RANGE IS FROM 1 TO N, NOT FROM 0 TO N-1
            {
                found = true;
                break;
            }
            count ++;
        }
    }
    buf_usage.close();

    return found;

}
#endif
#if USE_NI_TRACER == 1
void
NetworkInterface_d::updateShortPacket(int vnet)
{
	short_packet_flits[vnet]++;
}
void
NetworkInterface_d::updateLongPacket(int vnet)
{
	long_packet_flits[vnet]+=9;
}
void
NetworkInterface_d::updateShortMessages(int vnet)
{
	short_messages_delivered[vnet]++;
}
void
NetworkInterface_d::updateLongMessages(int vnet)
{
	long_messages_delivered[vnet]++;
}
void
NetworkInterface_d::resetShortPacket(int vnet)
{
  	short_packet_flits[vnet]= 0;
}
void
NetworkInterface_d::resetLongPacket(int vnet)
{
	long_packet_flits[vnet] = 0;
}

void
NetworkInterface_d::resetShortMessages(int vnet)
{
	short_messages_delivered[vnet]= 0;
}
void
NetworkInterface_d::resetLongMessages(int vnet)
{
	short_messages_delivered[vnet]= 0;

}

void
NetworkInterface_d::getTrafficMetrics(std::string filename)
{
   std::ofstream writer;
        writer.open(filename);
        assert(writer.is_open());
        int cycle = curCycle();
        for(int i = 0; i<m_virtual_networks;i++)
        {
        writer <<"vnet"<<i<<":"<<cycle<<";"<<short_messages_delivered[i]<<";"<<long_messages_delivered[i]<<";"<<short_packet_flits[i]<<";"<<long_packet_flits[i]<<";"<< std::endl;

        }
        writer.close();

}
#endif

#if USE_TRAFFIC_TRACER == 1

void
NetworkInterface_d::updateTracer(int cycle,int num_flits)
{
        if(m_id == 0 || m_id == 5)
        {
        (*m_traffic_tracer)<<cycle<<";"<<num_flits<<";"<<inNode_ptr[2]->getSize()<<std::endl;
        }
}

#endif


#if PROFILER == 1
void
NetworkInterface_d::printFinalStats()
{
	for(int i = 0; i< m_virtual_networks;i++)
	{
	  
		*m_ni_tracer <<i<<":"
			     << m_net_ptr->getVirtualNetworkLatency(i) <<";"
			     << m_net_ptr->getVnetQueueingLatency(i) <<";"
			     << m_net_ptr->getAverageVnetLatency(i) <<";"
			     << m_net_ptr->getVnetInjectedFlits(i)<<";"
			     << m_net_ptr->getVnetReceivedFlits(i)<<";"	
			     << m_net_ptr->get_injected_long_pkt(i)<<";"	//rec short
			     << m_net_ptr->get_received_long_pkt(i)<<";"	//rec long
			     << m_net_ptr->get_injected_short_pkt(i)<<";"	//inj short
                             << m_net_ptr->get_received_short_pkt(i)<<";"	//rec. short	
			     << m_net_ptr->getDistance(i)<<std::endl;
	
		#if USE_COMPRESSION == 1
			if(i==2)
			{
			*m_ni_tracer <<"comp_pkt:"<<m_net_ptr->get_compressed_packet(i)<<std::endl;
			*m_ni_tracer <<"nocomp_pkt:"<<m_net_ptr->get_uncompressed_packet(i)<<std::endl;
			*m_ni_tracer <<"total_comp_cycles:"<<m_net_ptr->get_compression_cycles()<<std::endl;
			*m_ni_tracer <<"delta:"<<m_net_ptr->get_delta_compressions()<<std::endl;
			*m_ni_tracer <<"zero:"<<m_net_ptr->get_zero_compressions()<<std::endl;
			}	 
		#endif
	}


        #if USE_NI_TRACER == 1
        getTrafficMetrics("m5out/traffic_distrib.txt");
        #endif

}

void
NetworkInterface_d::resetFinalStats()
{
   m_net_ptr->resetDistances();
    m_net_ptr->reset_zero_compressions();
    m_net_ptr->reset_delta_compressions();
    m_net_ptr->reset_compression_cycles();
    for(int i=0;i<m_virtual_networks;i++)
        {

                m_net_ptr->reset_compressed_packets(i);
                m_net_ptr->reset_uncompressed_packets(i);

        }

}

#endif
