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

#include "mem/ruby/buffers/MessageBuffer.hh"
#include "mem/ruby/network/BasicLink.hh"
#include "mem/ruby/network/Topology.hh"
#include "mem/ruby/network/garnet/BaseGarnetNetwork.hh"
#include "base/statistics.hh"
#include <sys/stat.h>

#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"

#define DEBUG_NETWORK_TEST 1
#define NUM_CORES 1 //added by panc
#define TRACER_SAMPLE_CYCLES 50000
#define TRACK 0

using namespace std;

BaseGarnetNetwork::BaseGarnetNetwork(const Params *p)
    : Network(p),Consumer(this)
{
    m_ni_flit_size = p->ni_flit_size;
    m_vcs_per_vnet = p->vcs_per_vnet;
	m_delta_compressions=0;
	m_zero_compressions=0;   
    m_enable_fault_model = p->enable_fault_model;
    if (m_enable_fault_model)
        fault_model = p->fault_model;

    m_ruby_start = 0;

    // Currently Garnet only supports uniform bandwidth for all
    // links and network interfaces.
    for (std::vector<BasicExtLink*>::const_iterator i =
             m_topology_ptr->params()->ext_links.begin();
         i != m_topology_ptr->params()->ext_links.end(); ++i) {
        BasicExtLink* ext_link = (*i);
        if (ext_link->params()->bandwidth_factor != m_ni_flit_size) {
            fatal("Garnet only supports uniform bw across all links and NIs\n");
        }
    }
    for (std::vector<BasicIntLink*>::const_iterator i =
             m_topology_ptr->params()->int_links.begin();
         i != m_topology_ptr->params()->int_links.end(); ++i) {
        BasicIntLink* int_link = (*i);
        if (int_link->params()->bandwidth_factor != m_ni_flit_size) {
            fatal("Garnet only supports uniform bw across all links and NIs\n");
        }
    }

    // Allocate to and from queues

    // Queues that are getting messages from protocol
    m_toNetQueues.resize(m_nodes);

    // Queues that are feeding the protocol
    m_fromNetQueues.resize(m_nodes);

	std::cout<< m_virtual_networks<<std::endl;

	m_in_use.resize(m_virtual_networks);
    m_ordered.resize(m_virtual_networks);
    m_flits_received.resize(m_virtual_networks);
    m_flits_injected.resize(m_virtual_networks);
    compressed_packets.resize(m_virtual_networks);
    uncompressed_packets.resize(m_virtual_networks);
    m_short_pkt_received.resize(m_virtual_networks);
    m_short_pkt_injected.resize(m_virtual_networks);
    m_long_pkt_received.resize(m_virtual_networks);
    m_long_pkt_injected.resize(m_virtual_networks);

    m_network_latency.resize(m_virtual_networks);

    //added for tracker tick average latency
    m_network_latency_tick.resize(m_virtual_networks);

    m_queueing_latency.resize(m_virtual_networks);

      m_flits_distances.resize(m_virtual_networks); //added by panc
    for (int i = 0; i < m_virtual_networks; i++)
	{
        m_in_use[i] = false;
        m_ordered[i] = false;
        m_flits_received[i] = 0;
        m_flits_injected[i] = 0;
        compressed_packets[i]=0;
        uncompressed_packets[i]=0;
		m_short_pkt_received[i]=0;
		m_short_pkt_injected[i]=0;
		m_long_pkt_received[i]=0;
		m_long_pkt_injected[i]=0;

        m_network_latency[i] = 0.0;
        m_queueing_latency[i] = 0.0;

				m_network_latency_tick[i] = 0.0;
          m_flits_distances[i]=0;
    }

    for (int node = 0; node < m_nodes; node++) {
        // Setting number of virtual message buffers per Network Queue
        m_toNetQueues[node].resize(m_virtual_networks);
        m_fromNetQueues[node].resize(m_virtual_networks);

        // Instantiating the Message Buffers that
        // interact with the coherence protocol
        for (int j = 0; j < m_virtual_networks; j++) {
            m_toNetQueues[node][j] = new MessageBuffer();
            m_fromNetQueues[node][j] = new MessageBuffer();
        }
    }

    m_dsc=new DetailedStatsCallback(this);
    Stats::registerDumpCallback(m_dsc);


    //REQUESTS TYPES:               0: generano traffico             1:      hanno traffico        2: non hanno traffico
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_GETS,0));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_GETX,0));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTX,2));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTS,2));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTO,2));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTO_SHARERS,2));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_WB_ACK,2));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_WB_ACK_DATA,0));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_WB_NACK,2));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_INV,2));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_DMA_READ,0));
    requests_type.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_DMA_WRITE,1));




    responses_type.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_ACK,2));
    responses_type.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_DATA,1));
    responses_type.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_UNBLOCK,2));
    responses_type.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_UNBLOCK_EXCLUSIVE,2));
    responses_type.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_DATA_EXCLUSIVE,1));
    responses_type.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_WRITEBACK_CLEAN_ACK,2));
    responses_type.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_WRITEBACK_CLEAN_DATA,1));
    responses_type.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_WRITEBACK_DIRTY_DATA,1));
    responses_type.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_DMA_ACK,2));

    setupInjTrafficStats();
    setupRecTrafficStats();
    dataPackets=0;
    mkdir("m5out/traffic_distribution",0777);
    req_file_inj.open("m5out/traffic_distribution/inj_requests.csv",ios::out | ios::app);
    req_file_rec.open("m5out/traffic_distribution/rec_requests.csv",ios::out | ios::app );
    otf_req_file.open("m5out/traffic_distribution/otf_requests.csv",ios::out | ios::app);
    std::map<const CoherenceRequestType, int>::iterator it = requests.begin();
      while(it != requests.end())
      {
          req_file_inj<<it->first<<";";
          req_file_rec<<it->first<<";";
          otf_req_file<<it->first<<";";
          it++;
      }
      req_file_inj<<std::endl;
      req_file_inj.close();
      req_file_rec<<std::endl;
      req_file_rec.close();
      otf_req_file<<std::endl;
      otf_req_file.close();
    res_file_inj.open("m5out/traffic_distribution/inj_responses.csv",ios::out | ios::app);
    res_file_rec.open("m5out/traffic_distribution/rec_responses.csv",ios::out | ios::app);
    otf_res_file.open("m5out/traffic_distribution/otf_responses.csv",ios::out | ios::app);

    std::map<const CoherenceResponseType, int>::iterator it2 = responses.begin();
      while(it2 != responses.end())
      {
          res_file_inj<<it2->first<<";";
          res_file_rec<<it2->first<<";";
          otf_res_file<<it2->first<<";";
          it2++;
      }
      res_file_rec<<std::endl;
      res_file_rec.close();
      res_file_inj<<std::endl;
      res_file_inj.close();
      otf_res_file<<std::endl;
      otf_res_file.close();
    data_file_inj.open("m5out/traffic_distribution/inj_data.csv",ios::out | ios::app);
    data_file_inj.close();
    data_file_rec.open("m5out/traffic_distribution/rec_data.csv",ios::out | ios::app);
    data_file_rec.close();
    otf_data_file.open("m5out/traffic_distribution/otf_data.csv",ios::out | ios::app);
    otf_data_file.close();
    traffic0_file.open("m5out/traffic_distribution/traffic_0.csv",ios::out | ios::app);
    traffic0_file.close();
    traffic1_file.open("m5out/traffic_distribution/traffic_1.csv",ios::out | ios::app); 
    traffic1_file.close();
    traffic2_file.open("m5out/traffic_distribution/traffic_2.csv",ios::out | ios::app);
    traffic2_file.close();
    flits_file.open("m5out/traffic_distribution/flits.csv", ios::out | ios::app);
    flits_file.close();
    
    m_compressed_flits = 0;
    m_uncompressed_flits = 0;


}

void
BaseGarnetNetwork::init()
{
    Network::init();
    #if TRACK == 1
    last_cycle=curCycle();
    scheduleEvent(TRACER_SAMPLE_CYCLES);
    #endif
}
void
BaseGarnetNetwork::wakeup() {
#if TRACK == 1
  if(curCycle() - last_cycle >= TRACER_SAMPLE_CYCLES)
  {
      stringstream req_i,resp_i,data_i,req_otf,resp_otf,data_otf,traffic0,traffic1,traffic2;

      traffic0<<curCycle()<<dumpTraffic_0();
      traffic1<<curCycle()<<dumpTraffic_1();
      traffic2<<curCycle()<<dumpTraffic_2();

      traffic0_file.open("m5out/traffic_distribution/traffic_0.csv",ios::out | ios::app);
      traffic0_file<<traffic0.str()<<std::endl;
      traffic0_file.close();

      traffic1_file.open("m5out/traffic_distribution/traffic_1.csv",ios::out | ios::app);
      traffic1_file<<traffic1.str()<<std::endl;
      traffic1_file.close();

      traffic2_file.open("m5out/traffic_distribution/traffic_2.csv",ios::out | ios::app);
      traffic2_file<<traffic2.str()<<std::endl;
      traffic2_file.close();

      stringstream fl;
      fl<<curCycle()<<dumpFlits();
      flits_file.open("m5out/traffic_distribution/flits.csv", ios::out | ios::app);
      flits_file<<fl.str()<<std::endl;
      flits_file.close();
 


      req_otf<<curCycle()<<dumpOTFRequests();
      resp_otf<<curCycle()<<dumpOTFResponses();
      data_otf<<curCycle()<<dumpOTFDataPackets();

      otf_req_file.open("m5out/traffic_distribution/otf_requests.csv",ios::out | ios::app);
      otf_req_file<<req_otf.str()<<std::endl;
      otf_req_file.close();

      otf_res_file.open("m5out/traffic_distribution/otf_responses.csv",ios::out | ios::app);
      otf_res_file<<resp_otf.str()<<std::endl;
      otf_res_file.close();

      otf_data_file.open("m5out/traffic_distribution/otf_data.csv",ios::out | ios::app);
      otf_data_file<<data_otf.str()<<std::endl;
      otf_data_file.close();

      req_i<<curCycle()<<dumpInjRequests();
      resp_i<<curCycle()<<dumpInjResponses();
      data_i<<curCycle()<<dumpInjDataPackets();

      req_file_inj.open("m5out/traffic_distribution/inj_requests.csv",ios::out | ios::app);
      req_file_inj<<req_i.str()<<std::endl;
      req_file_inj.close();

      res_file_inj.open("m5out/traffic_distribution/inj_responses.csv",ios::out | ios::app);
      res_file_inj<<resp_i.str()<<std::endl;
      res_file_inj.close();

      data_file_inj.open("m5out/traffic_distribution/inj_data.csv",ios::out | ios::app);
      data_file_inj<<data_i.str()<<std::endl;
      data_file_inj.close();

      cleanInjRequests();
      cleanInjResponses();
      cleanInjDataPackets();

      stringstream req_r,resp_r,data_r;

      req_r<<curCycle()<<dumpRecRequests();
      resp_r<<curCycle()<<dumpRecResponses();
      data_r<<curCycle()<<dumpRecDataPackets();

      req_file_rec.open("m5out/traffic_distribution/rec_requests.csv",ios::out | ios::app);
      req_file_rec<<req_r.str()<<std::endl;
      req_file_rec.close();

      res_file_rec.open("m5out/traffic_distribution/rec_responses.csv",ios::out | ios::app);
      res_file_rec<<resp_r.str()<<std::endl;
      res_file_rec.close();

      data_file_rec.open("m5out/traffic_distribution/rec_data.csv",ios::out | ios::app);
      data_file_rec<<data_r.str()<<std::endl;
      data_file_rec.close();
      cleanRecRequests();
      cleanRecResponses();
      cleanRecDataPackets();
      //cleanInjFlits();
      //cleanRecFlits();
      #if USE_COMPRESSION == 1
      stringstream ratio;
      ratio<<curCycle()<<dumpCompressionRatio();
      ratio_file.open("m5out/traffic_distribution/compression_ratio.csv", ios::out | ios::app);
      ratio_file<<ratio.str()<<std::endl;
      ratio_file.close();
      cleanCompressedFlits();
      cleanUncompressedFlits();
      #endif

      last_cycle=curCycle();

      scheduleEvent(TRACER_SAMPLE_CYCLES);
  }
#endif
}
MessageBuffer*
BaseGarnetNetwork::getToNetQueue(NodeID id, bool ordered, int network_num,
                                 string vnet_type)
{
    checkNetworkAllocation(id, ordered, network_num, vnet_type);
    return m_toNetQueues[id][network_num];
}

MessageBuffer*
BaseGarnetNetwork::getFromNetQueue(NodeID id, bool ordered, int network_num,
                                   string vnet_type)
{
    checkNetworkAllocation(id, ordered, network_num, vnet_type);
    return m_fromNetQueues[id][network_num];
}

void
BaseGarnetNetwork::clearStats()
{
    m_ruby_start = curCycle();
    m_topology_ptr->clearStats();
}

Time
BaseGarnetNetwork::getRubyStartTime()
{
    //panic("This conflicts with DVFS");
    return m_ruby_start;
}

void
BaseGarnetNetwork::printStats(ostream& out) const
{
    out << endl;
    out << "Network Stats" << endl;
    out << "-------------" << endl;
    out << endl;
	out << endl;
    out << "-------------" << endl;
    out << "Per router buffer utilization stats" << endl;
	printRouterBufferStats(out);
    out << "-------------" << endl;
    out << endl;

    printPipelineStats(out);
    printPerformanceStats(out);
 #if DEBUG_NETWORK_TEST==1
     printPerformanceStats(std::cout);
 #endif

    printLinkStats(out);
    printPowerStats(out);
    m_topology_ptr->printStats(out);
}

double
BaseGarnetNetwork::getNetworkLatency() const
{
    long long int total_flits_injected = 0;
    long long int total_flits_received = 0;
    long long int total_network_latency = 0.0;
    long long int total_queueing_latency = 0.0;
	for (int i = 0; i < m_virtual_networks; i++)
	{
        if (!m_in_use[i])
            continue;

        total_flits_injected += m_flits_injected[i];
        total_flits_received += m_flits_received[i];
        total_network_latency += m_network_latency[i];
        total_queueing_latency += m_queueing_latency[i];
    }
	return  ((double) total_network_latency/ (double) total_flits_received);
}

double
BaseGarnetNetwork::getQueueLatency() const
{
    long long int total_flits_injected = 0;
    long long int total_flits_received = 0;
    long long int total_network_latency = 0.0;
    long long int total_queueing_latency = 0.0;
	for (int i = 0; i < m_virtual_networks; i++)
	{
        if (!m_in_use[i])
            continue;

        total_flits_injected += m_flits_injected[i];
        total_flits_received += m_flits_received[i];
        total_network_latency += m_network_latency[i];
        total_queueing_latency += m_queueing_latency[i];
    }
	return  ((double) total_network_latency/ (double) total_flits_received);
	return ((double) total_queueing_latency/ (double) total_flits_received);
    return ((double)  (total_queueing_latency + total_network_latency) / (double) total_flits_received);
}

double
BaseGarnetNetwork::getAverageLatency() const
{
    long long int total_flits_injected = 0;
    long long int total_flits_received = 0;
    long long int total_network_latency = 0.0;
    long long int total_queueing_latency = 0.0;
	for (int i = 0; i < m_virtual_networks; i++)
	{
        if (!m_in_use[i])
            continue;

        total_flits_injected += m_flits_injected[i];
        total_flits_received += m_flits_received[i];
        total_network_latency += m_network_latency[i];
        total_queueing_latency += m_queueing_latency[i];
    }
    return ((double)  (total_queueing_latency + total_network_latency) / (double) total_flits_received);
}

void
BaseGarnetNetwork::printPerformanceStats(ostream& out) const
{
     long long int total_flits_injected = 0;
     long long int total_flits_received = 0;
     double total_network_latency = 0.0;
     double total_network_latency_tick = 0.0;
     double total_queueing_latency = 0.0;

         int num_active_vnet=0;
     for (int i = 0; i < m_virtual_networks_protocol; i++)
         {
         if (!m_in_use[i])
             continue;
                 else
                         num_active_vnet++;

         out << "[Vnet " << i << "]: flits injected = "
             << m_flits_injected[i] << endl;
         out << "[Vnet " << i << "]: flits received = "
             << m_flits_received[i] << endl;
         out << "[Vnet " << i << "]: short pkt injected = "
             << m_short_pkt_injected[i] << endl;
         out << "[Vnet " << i << "]: short_pkt received = "
             << m_short_pkt_received[i] << endl;
         out << "[Vnet " << i << "]: long pkt injected = "
             << m_long_pkt_injected[i] << endl;
         out << "[Vnet " << i << "]: long pkt received = "
             << m_long_pkt_received[i] << endl;
         //out << "[Vnet " << i << "]: average network latency[Cycle] = "
         //    << ((double) m_network_latency[i] / (double) m_flits_received[i])
         //    << endl;
         out << "[Vnet " << i << "]: average network latency[Tick] = "
             << ((double) m_network_latency_tick[i] / (double) m_flits_received[i])
             << endl;
         out << "[Vnet " << i << "]: average queueing (at source NI) latency = "
             << ((double) m_queueing_latency[i] / (double) m_flits_received[i])
             << endl;

         out << endl;
 // doing it differently to avoid double saturation.
         total_flits_injected += m_flits_injected[i];
         total_flits_received += m_flits_received[i];
         total_network_latency += ((double) m_network_latency[i] / (double) m_flits_received[i]);
         total_network_latency_tick += ((double) m_network_latency_tick[i] / (double) m_flits_received[i]);
         total_queueing_latency += ((double) m_queueing_latency[i] / (double) m_flits_received[i]);


     }
         out << "Active vnets = "<<num_active_vnet<<std::endl;
     out << "Total flits injected = " << total_flits_injected << endl;
     out << "Total flits received = " << total_flits_received << endl;
   //  out << "Average network latency [Cycle] = "
   //      << ((double) total_network_latency/ (double) num_active_vnet) << endl;
     out << "Average network latency [Tick] = "
         << ((double) total_network_latency_tick/ (double) num_active_vnet) << endl;
     out << "Average queueing (at source NI) latency = "
         << ((double) total_queueing_latency/ (double) num_active_vnet) << endl;
     out << "Average latency = "
 		<< (double)(((double)  (total_queueing_latency) + (double) total_network_latency_tick)/ (double) num_active_vnet)<< endl;
     out << "-------------" << endl;
     out << endl;

}

void
BaseGarnetNetwork::printRouterBufferStats(std::ostream& out) const
{
	panic("Implements this please");
}


BaseGarnetNetwork::~BaseGarnetNetwork()
{
    delete m_dsc;
    req_file_inj.close();
    res_file_inj.close();
    data_file_inj.close();
    otf_res_file.close();
    otf_req_file.close();
    otf_data_file.close();

    req_file_rec.close();
    res_file_rec.close();
    data_file_rec.close();
}

BaseGarnetNetwork::
DetailedStatsCallback::DetailedStatsCallback(const BaseGarnetNetwork *bgn)
    : m_bgn(bgn)
{
    mkdir("m5out/ruby_detail",0777);

    m_routerTotalPower.open("m5out/ruby_detail/router_total_power.txt");
    m_routerDynamicPower.open("m5out/ruby_detail/router_dynamic_power.txt");
    m_routerStaticPower.open("m5out/ruby_detail/router_static_power.txt");
    m_routerClockPower.open("m5out/ruby_detail/router_clock_power.txt");
    m_routerCongestion.open("m5out/ruby_detail/router_congestion.txt");

    m_linkTotalPower.open("m5out/ruby_detail/link_total_power.txt");
    m_linkDynamicPower.open("m5out/ruby_detail/link_dynamic_power.txt");
    m_linkStaticPower.open("m5out/ruby_detail/link_static_power.txt");
}

void
BaseGarnetNetwork::DetailedStatsCallback::process()
{
    m_bgn->printDetailedRouterStats(m_routerTotalPower,m_routerDynamicPower,
                                    m_routerStaticPower,m_routerClockPower,
                                    m_routerCongestion);
    m_bgn->printDetailedLinkStats(m_linkTotalPower,m_linkDynamicPower,
                                  m_linkStaticPower);
}

double
BaseGarnetNetwork::getVnetQueueingLatency(int vnet)
{
    assert(vnet<m_virtual_networks&&vnet>=0);
    double queue_latency=0;
    queue_latency=((double)(m_queueing_latency[vnet])/(double)m_flits_received[vnet]);
    return queue_latency;
}
double
BaseGarnetNetwork::getVirtualNetworkLatency(int vnet)
{
    assert(vnet<m_virtual_networks&&vnet>=0);
    double vnet_latency=0;
    vnet_latency=((double)(m_network_latency[vnet])/(double)m_flits_received[vnet]);
    return vnet_latency;
}
double
BaseGarnetNetwork::getAverageVnetLatency(int vnet)
{
  assert(vnet<m_virtual_networks&&vnet>=0);
  double vnet_latency=0;
  vnet_latency=((double)(m_network_latency[vnet]+m_queueing_latency[vnet])/(double)m_flits_received[vnet]);
  return vnet_latency;
}

long long int
BaseGarnetNetwork::getVnetInjectedFlits(int vnet)
{
    assert(vnet<m_virtual_networks&&vnet>=0);
    long long int vnet_injected_flits=0;
    vnet_injected_flits=m_flits_injected[vnet];
    return vnet_injected_flits;
}

long long int
BaseGarnetNetwork::getVnetReceivedFlits(int vnet)
{
  assert(vnet<m_virtual_networks&&vnet>=0);
    long long int vnet_received_flits=0;
    vnet_received_flits=m_flits_received[vnet];
    return vnet_received_flits;
}

long long int
BaseGarnetNetwork::getTotalInjectedFlits()
{
    long long int total_inj_flits=0;

    for (int i = 0; i < m_virtual_networks; i++)
    {
        if (!m_in_use[i])
            continue;


        total_inj_flits += m_flits_injected[i];
    }
    return total_inj_flits;

}

long long int
BaseGarnetNetwork::getTotalReceivedFlits()
{
    long long int total_rec_flits=0;

    for (int i = 0; i < m_virtual_networks; i++)
    {
        if (!m_in_use[i])
            continue;


        total_rec_flits += m_flits_received[i];
    }
    return total_rec_flits;

}
void
BaseGarnetNetwork::incrementDistanceOnVnet(int vnet,int distance)
{
    assert(vnet>=0&&vnet<m_virtual_networks);
    m_flits_distances[vnet]+=distance;
}
long long int
BaseGarnetNetwork::getDistance(int vnet)
{
    assert(vnet>=0&&vnet<m_virtual_networks);
    return m_flits_distances[vnet];
}



int
BaseGarnetNetwork::getDistanceFromDestination(MsgPtr message,NetworkInterface_d* nic,int rows, int cols)
{
  NetworkLink_d* m_link=dynamic_cast<NetworkLink_d*>(nic->getOutNetLink());
  NetDest destination;
  NetworkMessage *net_msg_ptr = safe_cast<NetworkMessage*>(message.get());
  InputUnit_d* m_unit;
  Router_d* m_router;
  int m_router_id,hops=-1,dest;

  destination=net_msg_ptr->getDestination();

  if(m_link!=NULL)
  {
    m_unit=dynamic_cast<InputUnit_d*>(m_link->getLinkConsumer());

    if(m_unit!=NULL)
    {
      m_router=m_unit->get_router();
      m_router_id=m_router->get_id();

      MachineID mach=destination.smallestElement();

      dest=machineIDToNodeID(mach);

      if(dest>=0)
      {
        MachineType type=machineIDToMachineType(mach);

        hops=calculateHops(m_router_id,dest,type,rows,cols,NUM_CORES);
      }
      if(hops!=-1)
      {

        return hops;

      }
    }

  }
  return -1;
}

void
BaseGarnetNetwork::checkCorners(int destination, int &x,int &y,int rows,int cols)
{

            switch(destination)
                {
                  case 0:
                  x=0;
                  y=0;
                  break;
                  case 1:
                    x=0;
                    y=cols-1;
                  break;
                  case 2:
                    x=rows-1;
                    y=0;
                  break;
                  case 3:
                  x=rows-1;
                  y=cols-1;
                  break;

                }
}


int
BaseGarnetNetwork::calculateHops(int source, int destination,MachineType type, int rows, int cols,int num_cores)
{

int x=-1,x1=-1,y=-1,y1=-1,counter=0;
int distance=-1;
bool is_corner=false;
      for(int i=0;i<rows;i++)
      {
        for(int j=0;j<cols;j++)
        {

          for(int c=0;c<num_cores;c++)
          {
            if(source==counter)
              {
                x=i;
                y=j;
              }
            if(destination==counter)
            {
              if(type==MachineType_Directory)
              {
		is_corner=true;
                checkCorners(destination,x1,y1,rows,cols);
              }
              else
              {
                x1=i;
                y1=j;
              }

            }
            counter++;
          }

        }
      }

if(x!=-1&&x1!=-1&&y!=-1&&y1!=-1)
{

    distance=1+abs(x1-x)+abs(y1-y);
    if(is_corner&&distance>1)distance++;

    if(num_cores>1) distance+=1; //take in account the connection to the bus if num cores per tile >1

    return distance;
}
else
{
  return -1;
}

}

long long int
BaseGarnetNetwork::getTotalDistance()
{

  long long int total_distance=0;

  for(int i=0;i<m_virtual_networks;i++)total_distance+=m_flits_distances[i];

    return total_distance;
}
void
BaseGarnetNetwork::resetDistances()
{

  for(int i=0;i<m_virtual_networks;i++)m_flits_distances[i]=0;
}


void
BaseGarnetNetwork::incrementInjDataPackets()
{
  dataPackets++;
}
void
BaseGarnetNetwork::incrementRecDataPackets()
{
  rec_dataPackets++;
}

void 
BaseGarnetNetwork::incrementInjFlits(int num_flits)
{
inj_flits+=num_flits;
}
void 
BaseGarnetNetwork::incrementRecFlits(int num_flits)
{
rec_flits+=num_flits;
}

std::string
BaseGarnetNetwork::dumpFlits()
{
std::stringstream ss;
ss<<";"<<inj_flits<<";"<<rec_flits;
return ss.str();
}


std::string
BaseGarnetNetwork::dumpInjDataPackets()
{
  std::stringstream ss;
  ss<<";"<<dataPackets;
  return ss.str();
}

std::string
BaseGarnetNetwork::dumpOTFDataPackets()
{
  std::stringstream ss;
  ss<<";"<<dataPackets - rec_dataPackets;
  return ss.str();
}

std::string
BaseGarnetNetwork::dumpRecDataPackets()
{
  std::stringstream ss;
  ss<<";"<<rec_dataPackets;
  return ss.str();
}


std::string
BaseGarnetNetwork::dumpInjRequests()
{
  std::map<const CoherenceRequestType, int>::iterator it = requests.begin();
  stringstream ss;

    while(it != requests.end())
    {
        ss<<";"<<it->second;
        it++;
    }
    return ss.str();
}

std::string
BaseGarnetNetwork::dumpRecRequests()
{
  std::map<const CoherenceRequestType, int>::iterator it = rec_requests.begin();
  stringstream ss;

    while(it != rec_requests.end())
    {
        ss<<";"<<it->second;
        it++;
    }
    return ss.str();
}

std::string
BaseGarnetNetwork::dumpOTFRequests()
{
  std::map<const CoherenceRequestType, int>::iterator it = requests.begin();
  stringstream ss;

    while(it != requests.end())
    {
        ss<<";"<<requests[it->first]-rec_requests[it->first];
        it++;
    }
    return ss.str();
}

std::string
BaseGarnetNetwork::dumpInjResponses()
{
  std::map<const CoherenceResponseType, int>::iterator it = responses.begin();
  stringstream ss;

    while(it != responses.end())
    {
        ss<<";"<<it->second;
        it++;
    }
    return ss.str();
}

std::string
BaseGarnetNetwork::dumpRecResponses()
{
  std::map<const CoherenceResponseType, int>::iterator it = rec_responses.begin();
  stringstream ss;

    while(it != rec_responses.end())
    {
        ss<<";"<<it->second;
        it++;
    }
    return ss.str();
}
std::string
BaseGarnetNetwork::dumpOTFResponses()
{
  std::map<const CoherenceResponseType, int>::iterator it = responses.begin();
  stringstream ss;

    while(it != responses.end())
    {
        ss<<";"<<responses[it->first]-rec_responses[it->first];
        it++;
    }
    return ss.str();
}

std::string
BaseGarnetNetwork::dumpTraffic_0()            //richieste che genereranno traffico in futuro
{

  std::map<const CoherenceRequestType, int>::iterator it = requests_type.begin();
  stringstream ss;
 // int total=0;
  int total_inj=0;
  int total_rec=0;
    while(it != requests_type.end())
    {
        if(it->second == 0)
        {
   //       total+=requests[it->first] - rec_requests[it->first];
          total_inj+=requests[it->first];
          total_rec+=rec_requests[it->first];                  //richieste iniettate e ricevute al tempo X
        }
        it++;
    }
    ss<<";"<<total_inj<<";"<<total_rec;           //scrivo su file il tipo 0: inj,rec, otf requests
    return ss.str();
  }


std::string
BaseGarnetNetwork::dumpTraffic_1()            //richieste che genereranno traffico in futuro
{
  std::map<const CoherenceResponseType, int>::iterator it2 = responses_type.begin();
  std::map<const CoherenceRequestType, int>::iterator it = requests_type.begin();
  stringstream ss;
  //int total=dataPackets - rec_dataPackets;
  int total_inj=dataPackets;
  int total_rec=rec_dataPackets;
    while(it != requests_type.end())
    {
        if(it->second == 1)
        {
          //total += requests[it->first] - rec_requests[it->first];
          total_inj += requests[it->first];
          total_rec += rec_requests[it->first];                  //richieste iniettate e ricevute al tempo X
        }
        it++;
    }

    while(it2 != responses_type.end())
    {
        if(it2->second == 1)
        {
          //total+=responses[it2->first] - rec_responses[it2->first];
          total_inj+=responses[it2->first];
          total_rec+=rec_responses[it2->first];                  //richieste iniettate e ricevute al tempo X
        }
        it2++;
    }
    ss<<";"<<total_inj<<";"<<total_rec;           //scrivo su file il tipo 1: inj,rec,otf TRAFFICO 1(DATI)
    return ss.str();
  }



std::string
BaseGarnetNetwork::dumpTraffic_2()            //richieste che genereranno traffico in futuro
{
  std::map<const CoherenceResponseType, int>::iterator it2 = responses_type.begin();
  std::map<const CoherenceRequestType, int>::iterator it = requests_type.begin();
  stringstream ss;
  
  int total_inj=0;
  int total_rec=0;
    while(it != requests_type.end())
    {
        if(it->second == 2)
        {
          //total += requests[it->first] - rec_requests[it->first];
          total_inj += requests[it->first];
          total_rec += rec_requests[it->first];                  //richieste iniettate e ricevute al tempo X
        }
        it++;
    }

    while(it2 != responses_type.end())
    {
        if(it2->second == 2)
        {
          //total+=responses[it2->first] - rec_responses[it2->first];
          total_inj+=responses[it2->first];
          total_rec+=rec_responses[it2->first];                  //richieste iniettate e ricevute al tempo X
        }
        it2++;
    }
    ss<<";"<<total_inj<<";"<<total_rec;           //scrivo su file il tipo 1: inj,rec,otf TRAFFICO 1(DATI)
    return ss.str();
  }






void
BaseGarnetNetwork::categorizeInjResponseType(ResponseMsg* message) {
  CoherenceResponseType type = message->getType();
  responses[type]++;
}
void
BaseGarnetNetwork::categorizeRecResponseType(ResponseMsg* message) {
  CoherenceResponseType type = message->getType();
  rec_responses[type]++;
}

void
BaseGarnetNetwork::categorizeInjRequestType(RequestMsg* message) {
  CoherenceRequestType type = message->getType();
  requests[type]++;
}

void
BaseGarnetNetwork::categorizeRecRequestType(RequestMsg* message) {
  CoherenceRequestType type = message->getType();
  rec_requests[type]++;
}

void
BaseGarnetNetwork::cleanInjRequests()
{
  std::map<const CoherenceRequestType, int>::iterator it = requests.begin();
    while(it != requests.end())
    {
        it->second=0;
        it++;
    }
}

void
BaseGarnetNetwork::cleanRecRequests()
{
  std::map<const CoherenceRequestType, int>::iterator it = rec_requests.begin();
    while(it != rec_requests.end())
    {
        it->second=0;
        it++;
    }
}

void
BaseGarnetNetwork::cleanInjResponses()
{
  std::map<const CoherenceResponseType, int>::iterator it = responses.begin();
    while(it != responses.end())
    {
        it->second = 0;
        it++;
    }
}

void
BaseGarnetNetwork::cleanRecResponses()
{
  std::map<const CoherenceResponseType, int>::iterator it = rec_responses.begin();
    while(it != rec_responses.end())
    {
        it->second = 0;
        it++;
    }
}
void
BaseGarnetNetwork::cleanRecDataPackets()
{
  rec_dataPackets=0;
}
void
BaseGarnetNetwork::cleanInjDataPackets()
{
  dataPackets=0;
}
void
BaseGarnetNetwork::cleanInjFlits()
{
inj_flits=0;
}
void 
BaseGarnetNetwork::cleanRecFlits()
{
rec_flits=0;
}
void
BaseGarnetNetwork::incrementCompressedFlits(int num_flits)
{
  m_compressed_flits+=num_flits;
}

void
BaseGarnetNetwork::incrementUncompressedFlits(int num_flits)
{
  m_uncompressed_flits+=num_flits;
}

void
BaseGarnetNetwork::cleanCompressedFlits()
{
  m_compressed_flits=0;
}
void
BaseGarnetNetwork::cleanUncompressedFlits()
{
  m_uncompressed_flits=0;
}

std::string
BaseGarnetNetwork::dumpCompressionRatio()
{
  std::stringstream ss;
  double ratio=(double)m_compressed_flits/m_uncompressed_flits;
  ss<<";"<<ratio;
  return ss.str();
}



void
BaseGarnetNetwork::setupInjTrafficStats()
{
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_GETS,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_GETX,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTX,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTS,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTO,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTO_SHARERS,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_WB_ACK,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_WB_ACK_DATA,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_WB_NACK,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_INV,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_DMA_READ,0));
  requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_DMA_WRITE,0));

  responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_ACK,0));
  responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_DATA,0));
  responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_UNBLOCK,0));
  responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_UNBLOCK_EXCLUSIVE,0));
  responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_DATA_EXCLUSIVE,0));
  responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_WRITEBACK_CLEAN_ACK,0));
  responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_WRITEBACK_CLEAN_DATA,0));
  responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_WRITEBACK_DIRTY_DATA,0));
  responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_DMA_ACK,0));
}

void
BaseGarnetNetwork::setupRecTrafficStats()
{
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_GETS,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_GETX,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTX,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTS,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTO,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_PUTO_SHARERS,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_WB_ACK,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_WB_ACK_DATA,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_WB_NACK,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_INV,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_DMA_READ,0));
  rec_requests.insert(std::pair<const CoherenceRequestType,int>(CoherenceRequestType_DMA_WRITE,0));

  rec_responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_ACK,0));
  rec_responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_DATA,0));
  rec_responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_UNBLOCK,0));
  rec_responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_UNBLOCK_EXCLUSIVE,0));
  rec_responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_DATA_EXCLUSIVE,0));
  rec_responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_WRITEBACK_CLEAN_ACK,0));
  rec_responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_WRITEBACK_CLEAN_DATA,0));
  rec_responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_WRITEBACK_DIRTY_DATA,0));
  rec_responses.insert(std::pair<const CoherenceResponseType,int>(CoherenceResponseType_DMA_ACK,0));
}

void 
BaseGarnetNetwork::increment_zero_compressions()
{
	m_zero_compressions++;
}
void
BaseGarnetNetwork::increment_delta_compressions()
{
        m_delta_compressions++;
}



long long int
BaseGarnetNetwork::get_zero_compressions()
{
        return m_zero_compressions;
}
long long int
BaseGarnetNetwork::get_delta_compressions()
{
        return m_delta_compressions;
}

void
BaseGarnetNetwork::update_compression_cycles(int value)
{
	m_compression_cycles+=value;
}
long long int
BaseGarnetNetwork::get_compression_cycles()
{
	return m_compression_cycles;
}
