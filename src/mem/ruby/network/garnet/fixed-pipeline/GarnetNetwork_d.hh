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

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_GARNETNETWORK_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_GARNETNETWORK_D_HH__

#include <iostream>
#include <vector>

#include "mem/ruby/network/garnet/BaseGarnetNetwork.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/network/Network.hh"
#include "params/GarnetNetwork_d.hh"
#include "config/use_pnet.hh"

#define TOT_ATOMIC_PKT 2 

class FaultModel;
class NetworkInterface_d;
class MessageBuffer;
class Router_d;
class Topology;
class NetDest;
class NetworkLink_d;
class CreditLink_d;

class Router_PNET_Container_d;
class NetworkLink_PNET_Container_d;
class NetworkInterface_PNET_Container_d;
class CreditLink_PNET_Container_d;

class GarnetNetwork_d : public BaseGarnetNetwork
{
  public:
    typedef GarnetNetwork_dParams Params;
    GarnetNetwork_d(const Params *p);

    ~GarnetNetwork_d();

    void init();

    int getNumNodes() { return m_nodes; }

    int getBuffersPerDataVC() {return m_buffers_per_data_vc; }
    int getBuffersPerCtrlVC() {return m_buffers_per_ctrl_vc; }

    void printPipelineStats(std::ostream& out) const;

    virtual void printRouterBufferStats(std::ostream& out) const;
    void printLinkStats(std::ostream& out) const;
    void printPowerStats(std::ostream& out) const;
    void printDetailedRouterStats(std::ostream& totalPower,
                                  std::ostream& dynamicPower,
                                  std::ostream& staticPower,
                                  std::ostream& clockPower,
                                  std::ostream& congestion) const;
    void printDetailedLinkStats(std::ostream& totalPower,
                                std::ostream& dynamicPower,
                                std::ostream& staticPower) const;
    void print(std::ostream& out) const;

    VNET_type
    get_vnet_type(int vc)
    {
        int vnet = vc/getVCsPerVnet();
        return m_vnet_type[vnet];
    }

    void reset();
    
    void resetRouterPowerStats();
    
    virtual void clearStats();

    // Methods used by Topology to setup the network
    void makeOutLink(SwitchID src, NodeID dest, BasicLink* link, 
                     LinkDirection direction,
                     const NetDest& routing_table_entry,
                     bool isReconfiguration);
    void makeInLink(NodeID src, SwitchID dest, BasicLink* link,
                    LinkDirection direction,
                    const NetDest& routing_table_entry,
                    bool isReconfiguration);
    void makeInternalLink(SwitchID src, SwitchID dest, BasicLink* link,
                          LinkDirection direction,
                          const NetDest& routing_table_entry,
                          bool isReconfiguration);

#if USE_PNET==0
	std::vector<Router_d *>& getRouters_d_ref(){return m_router_ptr_vector;}
	std::vector<NetworkInterface_d*>& getNetworkInterface_d_ref(){return m_ni_ptr_vector;}
#else // USE_PNET==1
	std::vector<Router_PNET_Container_d *>& getRouters_d_ref(){return m_router_ptr_vector;}
	std::vector<NetworkInterface_PNET_Container_d*>& getNetworkInterface_d_ref(){return m_ni_ptr_vector;}
#endif
	public:
		int getAdaptiveRoutingNumEscapePaths(){return adaptive_routing_num_escape_paths;}

		int getVirtualNetworkProtocol(){return m_virtual_networks_protocol;}
		int getVirtualNetworkSpurious(){return m_virtual_networks_protocol;}

  private:
    void checkNetworkAllocation(NodeID id, bool ordered, int network_num,
                                std::string vnet_type);

    GarnetNetwork_d(const GarnetNetwork_d& obj);
    GarnetNetwork_d& operator=(const GarnetNetwork_d& obj);

    std::vector<VNET_type > m_vnet_type;

	Tick lastTimeRubyDetailPrinted;
#if USE_PNET==0
    std::vector<Router_d *> m_router_ptr_vector;   // All Routers in Network
    std::vector<NetworkLink_d *> m_link_ptr_vector; // All links in the network
    std::vector<CreditLink_d *> m_creditlink_ptr_vector; // All links in net
    std::vector<NetworkInterface_d *> m_ni_ptr_vector;   // All NI's in Network
#else // USE_PNET==1
    std::vector<Router_PNET_Container_d *> m_router_ptr_vector;   // All Routers in Network
    std::vector<NetworkLink_PNET_Container_d *> m_link_ptr_vector; // All links in the network
    std::vector<CreditLink_PNET_Container_d *> m_creditlink_ptr_vector; // All links in net
    std::vector<NetworkInterface_PNET_Container_d *> m_ni_ptr_vector;   // All NI's in Network
#endif
    int m_buffers_per_data_vc;
    int m_buffers_per_ctrl_vc;

	/* NON_ATOMIC_VC_ALLOC*/
    public:
		int get_tot_atomic_pkt() { return m_tot_atomic_pkt;}
	private:
		int m_tot_atomic_pkt;

	int adaptive_routing_num_escape_paths;

	int m_virtual_networks_protocol;
	int m_virtual_networks_spurious;

	int m_synthetic_data_pkt_size;
	int m_num_rows;
	int m_num_cols;

	public:
		int get_synthetic_data_pkt_size(){return m_synthetic_data_pkt_size;};
		int get_num_rows(){return m_num_rows;}
		int get_num_cols(){return m_num_cols;}

	public:
		int	getTotVicharSlotPerVnet(){return m_totVicharSlotPerVnet;};
	private:
		int m_totVicharSlotPerVnet;
};

inline std::ostream&
operator<<(std::ostream& out, const GarnetNetwork_d& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_GARNETNETWORK_D_HH__
