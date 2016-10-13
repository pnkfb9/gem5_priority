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

/*
 * This header file is used to define all configuration parameters
 * required by the interconnection network.
 */

#ifndef __MEM_RUBY_NETWORK_GARNET_BASEGARNETNETWORK_HH__
#define __MEM_RUBY_NETWORK_GARNET_BASEGARNETNETWORK_HH__

#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/network/Network.hh"
#include "mem/ruby/network/fault_model/FaultModel.hh"
#include "params/BaseGarnetNetwork.hh"
#include "math.h"
#include "base/callback.hh"
#include <fstream>
#include "mem/protocol/Types.hh"
#include "mem/ruby/system/MachineID.hh"
#include "mem/ruby/common/Consumer.hh"

class BaseGarnetNetwork : public Network , public Consumer
{
  public:
    typedef BaseGarnetNetworkParams Params;
    BaseGarnetNetwork(const Params *p);

    void init();
    void wakeup();
    int getNiFlitSize() {return m_ni_flit_size; }
    int getVCsPerVnet() {return m_vcs_per_vnet; }
    bool isFaultModelEnabled() {return m_enable_fault_model;}
    FaultModel* fault_model;

    void increment_injected_flits(int vnet) { m_flits_injected[vnet]++; }
    void increment_received_flits(int vnet) { m_flits_received[vnet]++; }
    void increment_injected_short_pkt(int vnet) { m_short_pkt_injected[vnet]++; }
    void increment_received_short_pkt(int vnet) { m_short_pkt_received[vnet]++; }
    void increment_injected_long_pkt(int vnet) { m_long_pkt_injected[vnet]++; }
    void increment_received_long_pkt(int vnet) { m_long_pkt_received[vnet]++; }
    long long int get_injected_long_pkt(int vnet) { return m_long_pkt_injected[vnet]; }
    long long int get_received_long_pkt(int vnet) { return m_long_pkt_received[vnet]; }
    long long int get_injected_short_pkt(int vnet) { return m_short_pkt_injected[vnet]; }
    long long int get_received_short_pkt(int vnet) { return m_short_pkt_received[vnet]; }


	
    void
    increment_network_latency(Time latency, int vnet)
    {
        m_network_latency[vnet] += latency;

		}

		//added tick tracker
    void
    increment_network_latency_tick(float latency, int vnet)
    {
        m_network_latency_tick[vnet] += latency;
    }

    void
    increment_queueing_latency(Time latency, int vnet)
    {
        m_queueing_latency[vnet] += latency;
    }

    // returns the queue requested for the given component
    MessageBuffer* getToNetQueue(NodeID id, bool ordered, int network_num,
                                 std::string vnet_type);
    MessageBuffer* getFromNetQueue(NodeID id, bool ordered, int network_num,
                                   std::string vnet_type);


    bool isVNetOrdered(int vnet) { return m_ordered[vnet]; }
    bool validVirtualNetwork(int vnet) { return m_in_use[vnet]; }
    virtual void checkNetworkAllocation(NodeID id, bool ordered,
        int network_num, std::string vnet_type) = 0;

    Time getRubyStartTime();
    virtual void clearStats();
    void printStats(std::ostream& out) const;

	double getNetworkLatency() const;
	double getQueueLatency() const;
	double getAverageLatency() const;
    void printPerformanceStats(std::ostream& out) const;

	virtual void printRouterBufferStats(std::ostream& out) const;//not pure but panic()
	virtual void printPipelineStats(std::ostream& out) const = 0;//not pure but panic()
    virtual void printLinkStats(std::ostream& out) const = 0;
    virtual void printPowerStats(std::ostream& out) const = 0;
    virtual void printDetailedRouterStats(std::ostream& totalPower,
                                          std::ostream& dynamicPower,
                                          std::ostream& staticPower,
                                          std::ostream& clockPower,
                                          std::ostream& congestion) const = 0;
    virtual void printDetailedLinkStats(std::ostream& totalPower,
                                        std::ostream& dynamicPower,
                                        std::ostream& staticPower) const = 0;

    virtual ~BaseGarnetNetwork();
    double getVirtualNetworkLatency(int vnet);
    double getVnetQueueingLatency(int vnet);
    long long int getVnetInjectedFlits(int vnet); //added by panc
    long long int getVnetReceivedFlits(int vnet);
    long long int getTotalInjectedFlits();//added by panc
    long long int getTotalReceivedFlits();//added by panc
    long long int getTotalDistance();//added by panc
    void incrementDistanceOnVnet(int vnet,int distance);//added by panc
    long long int getDistance(int vnet); //added by panc
    void resetDistances();//added by panc
    int calculateHops(int source, int destination, MachineType type, int rows, int cols,int num_cores);//added by panc
    int getDistanceFromDestination(MsgPtr message, NetworkInterface_d* nic, int rows, int cols);//added by panc
    void checkCorners(int destination, int &x,int &y, int rows, int cols);//added by panc
    double getAverageVnetLatency(int vnet);
  private:

    class DetailedStatsCallback : public Callback
    {
      public:
        DetailedStatsCallback(const BaseGarnetNetwork *bgn);
        void process();

      private:
        const BaseGarnetNetwork *m_bgn;

        std::ofstream m_routerTotalPower;
        std::ofstream m_routerDynamicPower;
        std::ofstream m_routerStaticPower;
        std::ofstream m_routerClockPower;
        std::ofstream m_routerCongestion;

        std::ofstream m_linkTotalPower;
        std::ofstream m_linkDynamicPower;
        std::ofstream m_linkStaticPower;
    };
	public:
	std::vector<std::vector<MessageBuffer*> >&	getToNetQueues(){return m_toNetQueues;}
  protected:
    int m_ni_flit_size;
    int m_vcs_per_vnet;
    bool m_enable_fault_model;

    std::vector<long long int> m_flits_received;
    std::vector<long long int> m_flits_injected;
    std::vector<long long int> m_short_pkt_received;
    std::vector<long long int> m_short_pkt_injected;
    std::vector<long long int> m_long_pkt_received;
    std::vector<long long int> m_long_pkt_injected;

	//long long int m_zero_compressions,m_delta_compressions; 
	//void increment_zero_compressions();
	//void increment_delta_compressions();
	//long long int get_zero_compressions();
	//long long int get_delta_compressions();
	
	std::vector<long long int> compressed_packets;
        std::vector<long long int> uncompressed_packets;
        public:
	
	long long int m_compression_cycles;	
	long long int m_zero_compressions,m_delta_compressions;
        void increment_zero_compressions();
        void increment_delta_compressions();
        long long int get_zero_compressions();
        long long int get_delta_compressions();

	void reset_zero_compressions(){ m_zero_compressions=0; }
	void reset_delta_compressions(){ m_delta_compressions=0; }
	
	void reset_compression_cycles(){ m_compression_cycles=0; }
	
	void reset_compressed_packets(int vnet){ compressed_packets[vnet]=0; }
	void reset_uncompressed_packets(int vnet){ uncompressed_packets[vnet]=0; }

        


	void increment_compressed_packet(int vnet)
        {
                compressed_packets[vnet]++;
        }
        void increment_uncompressed_packet(int vnet)
        {
                uncompressed_packets[vnet]++;
        }
        long long int get_compressed_packet(int vnet)
        {
                return compressed_packets[vnet];
        }
        long long int get_uncompressed_packet(int vnet)
        {
                return uncompressed_packets[vnet];
        }


    //Added network latency tick counter
		std::vector<double> m_network_latency_tick;

    std::vector<double> m_network_latency;
    std::vector<double> m_queueing_latency;

    std::vector<long long int>m_flits_distances;

    std::vector<bool> m_in_use;
    std::vector<bool> m_ordered;

    std::vector<std::vector<MessageBuffer*> > m_toNetQueues;
    std::vector<std::vector<MessageBuffer*> > m_fromNetQueues;

    Time m_ruby_start;
    DetailedStatsCallback *m_dsc;
  public:
    //Added by panc for traffic type tracing
    std::map<const CoherenceRequestType, int> requests, rec_requests, requests_type;
    std::map<const CoherenceResponseType, int> responses, rec_responses, responses_type;
    int dataPackets, rec_dataPackets,inj_flits,rec_flits;
    int m_compressed_flits,m_uncompressed_flits;
    int last_cycle;
    std::ofstream req_file_inj,req_file_rec,otf_req_file;
    std::ofstream res_file_inj,res_file_rec,otf_res_file;
    std::ofstream data_file_inj,data_file_rec, otf_data_file;
    std::ofstream traffic0_file,traffic1_file,traffic2_file;
    std::ofstream ratio_file,flits_file;
    
    void setupInjTrafficStats();
    void setupRecTrafficStats();
    long long int get_compression_cycles();
    void update_compression_cycles(int value);	 
    void categorizeInjRequestType(RequestMsg* message);
    void categorizeRecRequestType(RequestMsg* message);
    
    void categorizeInjResponseType(ResponseMsg* message);
    void categorizeRecResponseType(ResponseMsg* message);
    
    void incrementInjDataPackets();
    void incrementRecDataPackets();
    
    void incrementCompressedFlits(int num_flits);
    void incrementUncompressedFlits(int num_flits);
    
    void incrementInjFlits(int num_flits);
    void incrementRecFlits(int num_flits);

    void cleanInjRequests();
    void cleanRecRequests();
    
    void cleanInjResponses();
    void cleanRecResponses();
    
    void cleanInjDataPackets(); 
    void cleanRecDataPackets();

    void cleanCompressedFlits();
    void cleanUncompressedFlits();
    
    void cleanInjFlits();
    void cleanRecFlits();

    std::string dumpInjRequests();
    std::string dumpRecRequests();
    std::string dumpOTFRequests();

    std::string dumpInjResponses();
    std::string dumpRecResponses();
    std::string dumpOTFResponses();
   
    std::string dumpInjDataPackets();
    std::string dumpRecDataPackets();
    std::string dumpOTFDataPackets();
    
    std::string dumpFlits();
    
    std::string dumpTraffic_0();
    std::string dumpTraffic_1();
    std::string dumpTraffic_2();
    
   std::string dumpCompressionRatio();
};

#endif // __MEM_RUBY_NETWORK_GARNET_BASEGARNETNETWORK_HH__
