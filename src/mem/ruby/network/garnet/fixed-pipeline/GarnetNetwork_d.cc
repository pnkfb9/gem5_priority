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
#include <fstream>
#include <sstream>

#include "base/cast.hh"
#include "base/stl_helpers.hh"
#include "mem/protocol/MachineType.hh"
#include "mem/ruby/buffers/MessageBuffer.hh"
#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/network/garnet/BaseGarnetNetwork.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/GarnetLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"

#include "config/use_pnet.hh"
#if USE_PNET==1
	#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_PNET_Container_d.hh"
	#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_PNET_Container_d.hh"
	#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_PNET_Container_d.hh"
	#include "mem/ruby/network/garnet/fixed-pipeline/Router_PNET_Container_d.hh"
	#include "mem/ruby/network/garnet/fixed-pipeline/GarnetLink_PNET_Container_d.hh"
#endif

#include "mem/ruby/network/Topology.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"

#include "mem/ruby/network/orion/sleep_model/sleep_transistor_model.hh"

#include "config/use_spurious_vc_vnet_reuse.hh"
#include "config/use_pnet.hh"
#include "sim/dvfs.hh"
#include "base/statistics.hh"
#define TRACK_TRAFFIC 1

using namespace std;
using m5::stl_helpers::deletePointers;

class ResetRouterPowerStats : public Callback
{
public:
    ResetRouterPowerStats(GarnetNetwork_d *garnet) : garnet(garnet) {}

    void process()
    {
        garnet->resetRouterPowerStats();
    }
private:
    GarnetNetwork_d *garnet;
};

GarnetNetwork_d::GarnetNetwork_d(const Params *p)
    : BaseGarnetNetwork(p)
{

	lastTimeRubyDetailPrinted=0;
    m_buffers_per_data_vc = p->buffers_per_data_vc;
    m_buffers_per_ctrl_vc = p->buffers_per_ctrl_vc;
	m_totVicharSlotPerVnet = p->totVicharSlotPerVnet;


 	m_virtual_networks_protocol = p->number_of_virtual_networks;
	#if USE_SPURIOUS_VC_VNET_REUSE == 1
	m_virtual_networks_spurious = p->number_of_virtual_networks_spurious;
	for( int i=0;i<m_virtual_networks_spurious;i++ )
	{
		m_in_use[m_virtual_networks_protocol+i]=true;
	}

	for( int i=0;i<m_virtual_networks;i++ )
		std::cout<<	m_in_use[i]<<std::endl;
	#else
	m_virtual_networks_spurious = 0;
	#endif

    m_vnet_type.resize(m_virtual_networks);
    for (int i = 0; i < m_vnet_type.size(); i++)
	{
        m_vnet_type[i] = NULL_VNET_; // default
		//#if USE_WH_VC_SA==1
		#if USE_SPURIOUS_VC_VNET_REUSE == 1
		if(m_virtual_networks_protocol<=i)
				m_vnet_type[i] = DATA_VNET_;
		#endif
		//#endif
    }

    // record the routers
    for (vector<BasicRouter*>::const_iterator i =
             m_topology_ptr->params()->routers.begin();
         i != m_topology_ptr->params()->routers.end(); ++i)
	{
		#if USE_PNET==0
        Router_d* router = safe_cast<Router_d*>(*i);
		#else //USE_PNET==1
		Router_PNET_Container_d* router = safe_cast<Router_PNET_Container_d*>(*i);
		#endif
        m_router_ptr_vector.push_back(router);
    }

    adaptive_routing_num_escape_paths=p->adaptive_routing_num_escape_paths;

	//NON_ATOMIC_VC_ALLOC per buffer
	#if USE_NON_ATOMIC_VC_ALLOC==1
		m_tot_atomic_pkt=p->max_non_atomic_pkt_per_vc;
	#else
		m_tot_atomic_pkt=1;
	#endif
	m_synthetic_data_pkt_size=p->synthetic_data_pkt_size;
	m_num_rows = p->num_rows;
	m_num_cols = p->num_cols;

}

void
GarnetNetwork_d::init()
{


	Dvfs *dvfs=Dvfs::instance();
    if(dvfs) dvfs->configure(m_router_ptr_vector.size());
    BaseGarnetNetwork::init();

	/*open octave interpreter*/
	Sleep_transistor_model::open_embedded_octave();
#if USE_PNET==0
    // initialize the router's network pointers
    for (vector<Router_d*>::const_iterator i = m_router_ptr_vector.begin();
         i != m_router_ptr_vector.end(); ++i) {
        Router_d* router = safe_cast<Router_d*>(*i);
        router->init_net_ptr(this);
    }
#else //USE_PNET==1
    for (vector<Router_PNET_Container_d*>::const_iterator i = m_router_ptr_vector.begin();
         i != m_router_ptr_vector.end(); ++i) {
        Router_PNET_Container_d* router = safe_cast<Router_PNET_Container_d*>(*i);
        router->init_net_ptr(this);
    }
#endif

    // The topology pointer should have already been initialized in the
    // parent network constructor
    assert(m_topology_ptr != NULL);
#if USE_PNET==0
    for (int i=0; i < m_nodes; i++) {
        NetworkInterface_d *ni = new NetworkInterface_d(i, m_virtual_networks,
                                                        this);
        ni->addNode(m_toNetQueues[i], m_fromNetQueues[i]);
				if(i == 0 || i == 5)
				{
						ni->scheduleEvent(10);
				}
        m_ni_ptr_vector.push_back(ni);
    }
#else
    for (int i=0; i < m_nodes; i++)
	{

		std::cout<<"ANDP GarnetNetwork.cc Costruttore init newtwork interface["<<i<<"] virtual network["<< m_virtual_networks <<"]"<<std::endl;
        NetworkInterface_PNET_Container_d *ni = new NetworkInterface_PNET_Container_d(i, m_virtual_networks,this);
        ni->addNode(m_toNetQueues[i], m_fromNetQueues[i]);
        m_ni_ptr_vector.push_back(ni);
    }
#endif
    // false because this isn't a reconfiguration
    m_topology_ptr->createLinks(this, false);

    // initialize the link's network pointers
#if USE_PNET==0
   for (vector<NetworkLink_d*>::const_iterator i = m_link_ptr_vector.begin();
         i != m_link_ptr_vector.end(); ++i) {
        NetworkLink_d* net_link = safe_cast<NetworkLink_d*>(*i);
        net_link->init_net_ptr(this);
    }
#else
   for (vector<NetworkLink_PNET_Container_d*>::const_iterator i = m_link_ptr_vector.begin();
         i != m_link_ptr_vector.end(); ++i) {
        NetworkLink_PNET_Container_d* net_link = safe_cast<NetworkLink_PNET_Container_d*>(*i);
        net_link->init_net_ptr(this);
    }
#endif

    // FaultModel: declare each router to the fault model
    if(isFaultModelEnabled()){
		#if USE_PNET==1
			assert("false"&&"\n\nFaulty Router Model is not supported for PNET implementation\n\n");
		#else
        for (vector<Router_d*>::const_iterator i= m_router_ptr_vector.begin();
             i != m_router_ptr_vector.end(); ++i) {
            Router_d* router = safe_cast<Router_d*>(*i);
            int router_id M5_VAR_USED =
                fault_model->declare_router(router->get_num_inports(),
                                            router->get_num_outports(),
                                            router->get_vc_per_vnet(),
                                            getBuffersPerDataVC(),
                                            getBuffersPerCtrlVC());
            assert(router_id == router->get_id());
            router->printAggregateFaultProbability(cout);
            router->printFaultVector(cout);
        }
		#endif
    }

	Stats::registerDumpCallback(new ResetRouterPowerStats(this));
}

GarnetNetwork_d::~GarnetNetwork_d()
{
    for (int i = 0; i < m_nodes; i++) {
        deletePointers(m_toNetQueues[i]);
        deletePointers(m_fromNetQueues[i]);
    }
    deletePointers(m_router_ptr_vector);
    deletePointers(m_ni_ptr_vector);
    deletePointers(m_link_ptr_vector);
    deletePointers(m_creditlink_ptr_vector);
    delete m_topology_ptr;
}

void
GarnetNetwork_d::reset()
{
    for (int node = 0; node < m_nodes; node++) {
        for (int j = 0; j < m_virtual_networks; j++) {
            m_toNetQueues[node][j]->clear();
            m_fromNetQueues[node][j]->clear();
        }
    }
}

void
GarnetNetwork_d::resetRouterPowerStats()
{
    for(int i=0;i<m_router_ptr_vector.size();i++)
		m_router_ptr_vector.at(i)->reset_performance_numbers();
}

void
GarnetNetwork_d::clearStats()
{
    BaseGarnetNetwork::clearStats();
    for (int i = 0; i < m_link_ptr_vector.size(); i++)
        m_link_ptr_vector[i]->clearStats();
}

/*
 * This function creates a link from the Network Interface (NI)
 * into the Network.
 * It creates a Network Link from the NI to a Router and a Credit Link from
 * the Router to the NI
*/

void
GarnetNetwork_d::makeInLink(NodeID src, SwitchID dest, BasicLink* link,
                            LinkDirection direction,
                            const NetDest& routing_table_entry,
                            bool isReconfiguration)
{
    assert(src < m_nodes);
#if USE_PNET==0
    GarnetExtLink_d* garnet_link = safe_cast<GarnetExtLink_d*>(link);
#else
	GarnetExtLink_PNET_Container_d* garnet_link = safe_cast<GarnetExtLink_PNET_Container_d*>(link);
#endif

    if (!isReconfiguration)
	{
#if USE_PNET==0
        NetworkLink_d* net_link = garnet_link->m_network_links[direction];
        CreditLink_d* credit_link = garnet_link->m_credit_links[direction];
#else
		NetworkLink_PNET_Container_d* net_link = garnet_link->m_network_links[direction];
        CreditLink_PNET_Container_d* credit_link = garnet_link->m_credit_links[direction];
#endif
        m_link_ptr_vector.push_back(net_link);
        m_creditlink_ptr_vector.push_back(credit_link);

        m_router_ptr_vector.at(dest)->addInPort(net_link, credit_link);
        m_ni_ptr_vector.at(src)->addOutPort(net_link, credit_link);

    } else {
        panic("Fatal Error:: Reconfiguration not allowed here");
        // do nothing
    }

}

/*
 * This function creates a link from the Network to a NI.
 * It creates a Network Link from a Router to the NI and
 * a Credit Link from NI to the Router
*/

void
GarnetNetwork_d::makeOutLink(SwitchID src, NodeID dest, BasicLink* link,
                             LinkDirection direction,
                             const NetDest& routing_table_entry,
                             bool isReconfiguration)
{

    assert(dest < m_nodes);
    assert(src < m_router_ptr_vector.size());
    assert(m_router_ptr_vector[src] != NULL);
#if USE_PNET==0
    GarnetExtLink_d* garnet_link = safe_cast<GarnetExtLink_d*>(link);
#else
	GarnetExtLink_PNET_Container_d* garnet_link = safe_cast<GarnetExtLink_PNET_Container_d*>(link);
#endif

    if (!isReconfiguration)
	{
	#if USE_PNET==0
        NetworkLink_d* net_link = garnet_link->m_network_links[direction];
        CreditLink_d* credit_link = garnet_link->m_credit_links[direction];
    #else
		NetworkLink_PNET_Container_d* net_link = garnet_link->m_network_links[direction];
        CreditLink_PNET_Container_d* credit_link = garnet_link->m_credit_links[direction];
	#endif
        m_link_ptr_vector.push_back(net_link);
        m_creditlink_ptr_vector.push_back(credit_link);

        m_router_ptr_vector[src]->addOutPort(net_link, routing_table_entry,
                                             link->m_weight,
                                             credit_link);
        m_ni_ptr_vector[dest]->addInPort(net_link, credit_link);
    } else {
        fatal("Fatal Error:: Reconfiguration not allowed here");
        // do nothing
    }

}

/*
 * This function creates an internal network link
*/

void
GarnetNetwork_d::makeInternalLink(SwitchID src, SwitchID dest, BasicLink* link,
                                  LinkDirection direction,
                                  const NetDest& routing_table_entry,
                                  bool isReconfiguration)
{

#if USE_PNET==0
    GarnetIntLink_d* garnet_link = safe_cast<GarnetIntLink_d*>(link);
#else
	GarnetIntLink_PNET_Container_d* garnet_link = safe_cast<GarnetIntLink_PNET_Container_d*>(link);
#endif

    if (!isReconfiguration)
	{
	#if USE_PNET==0
        NetworkLink_d* net_link = garnet_link->m_network_links[direction];
        CreditLink_d* credit_link = garnet_link->m_credit_links[direction];
	#else
		NetworkLink_PNET_Container_d* net_link = garnet_link->m_network_links[direction];
        CreditLink_PNET_Container_d* credit_link = garnet_link->m_credit_links[direction];
	#endif
        m_link_ptr_vector.push_back(net_link);
        m_creditlink_ptr_vector.push_back(credit_link);

        m_router_ptr_vector[dest]->addInPort(net_link, credit_link);
        m_router_ptr_vector[src]->addOutPort(net_link, routing_table_entry,
                                             link->m_weight,
                                             credit_link);
    } else {
        fatal("Fatal Error:: Reconfiguration not allowed here");
        // do nothing
    }

}

void
GarnetNetwork_d::checkNetworkAllocation(NodeID id, bool ordered,
                                        int network_num,
                                        string vnet_type)
{
    assert(id < m_nodes);
    assert(network_num < m_virtual_networks);

    if (ordered) {
        m_ordered[network_num] = true;
    }
    m_in_use[network_num] = true;

    if (vnet_type == "response")
        m_vnet_type[network_num] = DATA_VNET_; // carries data (and ctrl) packets
    else
        m_vnet_type[network_num] = CTRL_VNET_; // carries only ctrl packets

}

void
GarnetNetwork_d::printRouterBufferStats(ostream& out) const
{
#if USE_PNET==0
	ofstream f_buf("./m5out/buf_usage.txt");
	double average_buf_utilization = 0;

	for (int i = 0; i < m_router_ptr_vector.size(); i++)
	{
		double per_router_average_buf_utilization = 0;
		f_buf<<"R"<<i;
		for(int j=0; j<m_router_ptr_vector[i]->get_num_inports();j++)
		{
			Tick finalTick = curTick();//( (m_router_ptr_vector[i]->get_inputUnit_ref()).at(j) )->getTimestampLastUsedBuf();
			for(int q=0; q<m_router_ptr_vector[i]->get_num_vcs();q++)
			{
				per_router_average_buf_utilization+= (double) (m_router_ptr_vector[i]->getUsedBufInport())[j][q]/(finalTick);
				f_buf<<","<<(double) (m_router_ptr_vector[i]->getUsedBufInport())[j][q]/(finalTick);
			}
		}
		f_buf<<std::endl;
	//	out<<"R"<<i<<" avgBufUsage: "<< per_router_average_buf_utilization
	//			<<" #router_#inport: "<<m_router_ptr_vector[i]->get_num_inports()
	//			<<" #router_#vcs: "<<m_router_ptr_vector[i]->get_num_vcs()
	//			<<" #router_bufs: "<<m_router_ptr_vector[i]->get_num_inports()*m_router_ptr_vector[i]->get_num_vcs()
	//			<<std::endl;
		average_buf_utilization += per_router_average_buf_utilization/m_router_ptr_vector[i]->get_num_inports();
	}
	out<<std::endl<<"avgBuf_ALL_ROUTERS: "<<average_buf_utilization/m_router_ptr_vector.size()
			<<" #router_#vcs: "<<m_router_ptr_vector[0]->get_num_vcs()<<std::endl;

	int tot_buff;
	int tot_headplustail;
	int tot_headtail;
	tot_buff=0;
	tot_headplustail=0;
	tot_headtail=0;
	//Buffer Reuse statistics
	for (int i = 0; i < m_router_ptr_vector.size(); i++)
	{
		int buff_count;
		int headplustail_count;
		int headtail_count;

		buff_count = m_router_ptr_vector[i]->get_buffer_reused_count();
		headplustail_count = m_router_ptr_vector[i]->get_head_and_tail_count();
		headtail_count = m_router_ptr_vector[i]->get_only_headtail_count();

		tot_buff=tot_buff+buff_count;
		tot_headplustail=tot_headplustail+headplustail_count;
		tot_headtail=tot_headtail+headtail_count;

//		std::cout<<"ROUTER: ["<<i
//		<<"] #buffer reused: "<<buff_count
//		<<" headtail and head pck: "<<headplustail_count
//		<<" only headtail pck: "<<headtail_count<<std::endl;
	}
//	std::cout<<"TOT buffer reused: "<<tot_buff
//		<<" TOT  headtail and head pck: "<<tot_headplustail
//		<<" TOT  only headtail pck: "<<tot_headtail<<std::endl;
#endif
}

void
GarnetNetwork_d::printLinkStats(ostream& out) const
{
    double average_link_utilization = 0;
    vector<double> average_vc_load;
    average_vc_load.resize(m_virtual_networks*m_vcs_per_vnet);

    for (int i = 0; i < m_virtual_networks*m_vcs_per_vnet; i++) {
        average_vc_load[i] = 0;
    }

    out << endl;
    for (int i = 0; i < m_link_ptr_vector.size(); i++) {
        average_link_utilization +=
            (double(m_link_ptr_vector[i]->getLinkUtilization())) /
            (double(m_link_ptr_vector[i]->curCycle() - m_link_ptr_vector[i]->getStatStartTime()));

        vector<int> vc_load = m_link_ptr_vector[i]->getVcLoad();
        for (int j = 0; j < vc_load.size(); j++) {
            assert(vc_load.size() == m_vcs_per_vnet*m_virtual_networks);
            average_vc_load[j] += vc_load[j];
        }
    }
    average_link_utilization =
        average_link_utilization/m_link_ptr_vector.size();
    out << "Average Link Utilization :: " << average_link_utilization
        << " flits/cycle" << endl;
    out << "-------------" << endl;

    for (int i = 0; i < m_vcs_per_vnet*m_virtual_networks; i++) {
        if (!m_in_use[i/m_vcs_per_vnet])
            continue;

//        average_vc_load[i] = (double(average_vc_load[i])) /
//            (double(curCycle() - m_ruby_start));
        out << "Average VC Load [" << i << "] = " << "not available due to DVFS"//average_vc_load[i]
            << " flits/cycle " << endl;
    }
    out << "-------------" << endl;
    out << endl;
}

void
GarnetNetwork_d::printPowerStats(ostream& out) const
{
#if USE_PNET==0
    out << "Network Power" << endl;
    out << "-------------" << endl;
    double m_total_link_power = 0.0;
    double m_dynamic_link_power = 0.0;
    double m_static_link_power = 0.0;
    double m_total_router_power = 0.0;
    double m_dynamic_router_power = 0.0;
    double m_static_router_power = 0.0;
    double m_clk_power = 0.0;

    for (int i = 0; i < m_link_ptr_vector.size(); i++) {
        m_total_link_power += m_link_ptr_vector[i]->calculate_power();
        m_dynamic_link_power += m_link_ptr_vector[i]->get_dynamic_power();
        m_static_link_power += m_link_ptr_vector[i]->get_static_power();
    }

    Dvfs *dvfs=Dvfs::instance();
    for (int i = 0; i < m_router_ptr_vector.size(); i++)
	{
        double total=m_router_ptr_vector[i]->calculate_power_resettable();
        double dynamic=m_router_ptr_vector[i]->get_dynamic_power();
        double staticp=m_router_ptr_vector[i]->get_static_power();
		double clock=m_router_ptr_vector[i]->get_clk_power();

        m_total_router_power += total;
        m_dynamic_router_power += dynamic;
        m_static_router_power += staticp;
        m_clk_power += clock;
        if(dvfs==0) continue;
        dvfs->setTotalRouterPower(i,total);
        dvfs->setDynamicRouterPower(i,dynamic);
        dvfs->setStaticRouterPower(i,staticp);
        dvfs->setClockRouterPower(i,clock);
        dvfs->setAverageRouterFrequency(i,m_router_ptr_vector[i]->averageFrequency());
        dvfs->setAverageRouterVoltage(i,m_router_ptr_vector[i]->averageVoltage());
    }
    if(dvfs) dvfs->updateCompleted();
    out << "Link Dynamic Power = " << m_dynamic_link_power << " W" << endl;
    out << "Link Static Power = " << m_static_link_power << " W" << endl;
    out << "Total Link Power = " << m_total_link_power << " W " << endl;
    out << "Router Dynamic Power = " << m_dynamic_router_power << " W" << endl;
    out << "Router Clock Power = " << m_clk_power << " W" << endl;
    out << "Router Static Power = " << m_static_router_power << " W" << endl;
    out << "Total Router Power = " << m_total_router_power << " W " <<endl;
    out << "-------------" << endl;

	out << "------- ADDED FOR RUNTIME ----------" << endl;

	double dvfsTotalPower=0;
	double dvfsStaticPower=0;
	double dvfsDynamicPower=0;
	double dvfsClockPower=0;

	int numRouterCount=0;
	for (int i = 0; i < m_router_ptr_vector.size(); i++)
	{
		// from last call to rt_pwr computation to end pwr not taken!!!!
		if(m_router_ptr_vector[i]->getAvgPwrTickElapsed()==0)
			continue;
		numRouterCount++;
		dvfsTotalPower+=m_router_ptr_vector[i]->getFinalAvgTotalPower();
		dvfsStaticPower+=m_router_ptr_vector[i]->getFinalAvgStaticPower();
		dvfsDynamicPower+=m_router_ptr_vector[i]->getFinalAvgDynamicPower();
		dvfsClockPower+=m_router_ptr_vector[i]->getFinalAvgClockPower();

	}
    std::cout << "DVFS on "<<numRouterCount<<" Routers Dynamic Power = "
					<< dvfsDynamicPower/numRouterCount << " W" << endl;
    std::cout << "DVFS "<<numRouterCount<<" Routers Clock Power = "
					<< dvfsClockPower/numRouterCount << " W" << endl;
    std::cout << "DVFS on "<<numRouterCount<<" Routers Static Power = "
					<< dvfsStaticPower/numRouterCount << " W" << endl;
    std::cout << "DVFS on "<<numRouterCount<<" Totals Router Power = "
					<< dvfsTotalPower/numRouterCount << " W " <<endl;
    out << endl;
#else //USE_PNET==1
	std::vector<double> dvfsTotalPower;
	std::vector<double> dvfsStaticPower;
	std::vector<double> dvfsDynamicPower;
	std::vector<double> dvfsClockPower;
	std::vector<int> numRouterCount;

	dvfsTotalPower.resize( m_virtual_networks_protocol);
	dvfsStaticPower.resize( m_virtual_networks_protocol);
	dvfsDynamicPower.resize( m_virtual_networks_protocol);
	dvfsClockPower.resize( m_virtual_networks_protocol);
	numRouterCount.resize( m_virtual_networks_protocol);

	for(int j=0;j<m_virtual_networks_protocol;j++)
	{
		dvfsTotalPower.at(j)=0;
		dvfsStaticPower.at(j)=0;
		dvfsDynamicPower.at(j)=0;
		dvfsClockPower.at(j)=0;
		numRouterCount.at(j)=0;
	}
	for (int i = 0; i < m_router_ptr_vector.size(); i++)
	{
		std::vector<Router_d*>& r_pnet = m_router_ptr_vector[i]->getContainedRoutersRef();
		for(int j=0;j<m_virtual_networks_protocol;j++)
		{
			if(r_pnet[j]->getAvgPwrTickElapsed()==0)
				continue;
			numRouterCount[j]++;
			dvfsTotalPower[j]+=r_pnet[j]->getFinalAvgTotalPower();
			dvfsStaticPower[j]+=r_pnet[j]->getFinalAvgStaticPower();
			dvfsDynamicPower[j]+=r_pnet[j]->getFinalAvgDynamicPower();
			dvfsClockPower[j]+=r_pnet[j]->getFinalAvgClockPower();
		}
	}
//dynamic
    std::cout << "DVFS on [";
	for(int j=0;j<m_virtual_networks_protocol;j++)
		std::cout<<numRouterCount[j]<<",";
	std::cout<<"] Routers Dynamic Power [W] = [";
	for(int j=0;j<m_virtual_networks_protocol;j++)
		std::cout<<dvfsDynamicPower[j]/numRouterCount[j] << ", ";
	std::cout<<"]"<<std::endl;
//clocking
    std::cout << "DVFS on [";
	for(int j=0;j<m_virtual_networks_protocol;j++)
		std::cout<<numRouterCount[j]<<",";
    std::cout <<"] Routers Clock Power [W] = [";
	for(int j=0;j<m_virtual_networks_protocol;j++)
		std::cout<< dvfsClockPower[j]/numRouterCount[j] << ", ";
	std::cout<<"]"<<std::endl;
//static
	std::cout << "DVFS on [";
	for(int j=0;j<m_virtual_networks_protocol;j++)
		std::cout<<numRouterCount[j]<<",";
    std::cout <<"] Routers Static Power [W] = [";
	for(int j=0;j<m_virtual_networks_protocol;j++)
		std::cout<< dvfsStaticPower[j]/numRouterCount[j] << ", ";
	std::cout<<"]"<<std::endl;
//total
	std::cout << "DVFS on [";
	for(int j=0;j<m_virtual_networks_protocol;j++)
		std::cout<<numRouterCount[j]<<",";
    std::cout <<"] Totals Router Power [W] = ";
	for(int j=0;j<m_virtual_networks_protocol;j++)
		std::cout<< ((dvfsTotalPower[j]+dvfsClockPower[j])/numRouterCount[j]) << ", ";
    std::cout<<"]"<< endl;

#endif

m_ni_ptr_vector[0]->printFinalStats(); //added by panc
m_ni_ptr_vector[5]->printFinalStats(); //added by panc
m_ni_ptr_vector[0]->resetFinalStats(); //added by panc
m_ni_ptr_vector[5]->resetFinalStats();

}

void
GarnetNetwork_d::printDetailedRouterStats(std::ostream& totalPower,
                                          std::ostream& dynamicPower,
                                          std::ostream& staticPower,
                                          std::ostream& clockPower,
                                          std::ostream& congestion) const

{
	//if( !(lastTimeRubyDetailPrinted==0 || lastTimeRubyDetailPrinted==curTick()) )
	//	return;
	//else
	//	lastTimeRubyDetailPrinted=curTick();

		totalPower   << "router_object_id\t router power [W] (@"<<curTick()<<")"<<std::endl;
		dynamicPower <<"router_object_id\t router power [W] (@"<<curTick()<<")"<<std::endl;
		staticPower	 <<"router_object_id\t router power [W] (@"<<curTick()<<")"<<std::endl;
		clockPower   <<"router_object_id\t router power [W] (@"<<curTick()<<")"<<std::endl;
		congestion   <<"router_object_id\t router power [W] (@"<<curTick()<<")"<<std::endl;

    for (int i = 0; i < m_router_ptr_vector.size(); i++)
	{
	#if USE_PNET==0
        totalPower   << m_router_ptr_vector[i]->getObjectId()<<" "<<m_router_ptr_vector[i]->calculate_power_resettable() << std::endl;
        dynamicPower << m_router_ptr_vector[i]->getObjectId()<<" "<< m_router_ptr_vector[i]->get_dynamic_power() << std::endl;
        staticPower  << m_router_ptr_vector[i]->getObjectId()<<" "<< m_router_ptr_vector[i]->get_static_power() << std::endl;
        clockPower   << m_router_ptr_vector[i]->getObjectId()<<" "<< m_router_ptr_vector[i]->get_clk_power() << std::endl;
        congestion   << m_router_ptr_vector[i]->getObjectId()<<" "<< m_router_ptr_vector[i]->getCongestion()<< std::endl;
	#else
		totalPower   << m_router_ptr_vector[i]->getObjectId() << std::endl;
        dynamicPower << m_router_ptr_vector[i]->getObjectId() << std::endl;
        staticPower  << m_router_ptr_vector[i]->getObjectId() << std::endl;
        clockPower   << m_router_ptr_vector[i]->getObjectId() << std::endl;
        congestion   << m_router_ptr_vector[i]->getObjectId()<< std::endl;
		std::vector<Router_d*>& r_pnet = m_router_ptr_vector[i]->getContainedRoutersRef();
		for (int j=0;j<r_pnet.size();j++)
		{
			totalPower   << r_pnet.at(j)->getObjectId()<<" "<<r_pnet.at(j)->calculate_power_resettable()<<std::endl;
			dynamicPower   << r_pnet.at(j)->getObjectId()<<" "<<r_pnet.at(j)->get_dynamic_power()<<std::endl;
			staticPower   << r_pnet.at(j)->getObjectId()<<" "<<r_pnet.at(j)->get_static_power()<<std::endl;
			clockPower   << r_pnet.at(j)->getObjectId()<<" "<<r_pnet.at(j)->get_clk_power()<<std::endl;
			congestion   << r_pnet.at(j)->getObjectId()<<" "<<r_pnet.at(j)->getCongestion()<<std::endl;
		}
	#endif
    }
    totalPower   << endl;
    dynamicPower << endl;
    staticPower  << endl;
    clockPower   << endl;
    congestion   << endl;
}

void
GarnetNetwork_d::printDetailedLinkStats(std::ostream& totalPower,
                                        std::ostream& dynamicPower,
                                        std::ostream& staticPower) const
{
	totalPower   << "link_obj_id\tlink_src\tlink_dst\tlink_total_pwr [W]"<<std::endl;
	dynamicPower << "link_obj_id\tlink_src\tlink_dst\tlink_dynamic_pwr [W]"<<std::endl;
	staticPower  << "link_obj_id\tlink_src\tlink_dst\tlink_static_pwr [W]"<<std::endl;

    for (int i = 0; i < m_link_ptr_vector.size(); i++)
	{
	#if USE_PNET==0
        totalPower  << m_link_ptr_vector[i]->getObjectId()<<" "
					<< m_link_ptr_vector[i]->getLinkSource()->getObjectId()<<" -> "
					<< m_link_ptr_vector[i]->getLinkConsumer()->getObjectId()<<" "
					<< m_link_ptr_vector[i]->calculate_power() << std::endl;

        dynamicPower<< m_link_ptr_vector[i]->getObjectId()<<" "
					<< m_link_ptr_vector[i]->getLinkSource()->getObjectId()<<" -> "
					<< m_link_ptr_vector[i]->getLinkConsumer()->getObjectId()<<" "
					<< m_link_ptr_vector[i]->get_dynamic_power() << std::endl;


        staticPower << m_link_ptr_vector[i]->getObjectId()<<" "
					<< m_link_ptr_vector[i]->getLinkSource()->getObjectId()<<" -> "
					<< m_link_ptr_vector[i]->getLinkConsumer()->getObjectId()<<" "
					<< m_link_ptr_vector[i]->get_static_power() <<std::endl;
	#else
		//totalPower   << m_link_ptr_vector[i]->getObjectId() << std::endl;
        //dynamicPower << m_link_ptr_vector[i]->getObjectId() << std::endl;
        //staticPower  << m_link_ptr_vector[i]->getObjectId() << std::endl;

		std::vector<NetworkLink_d*>& l_pnet = m_link_ptr_vector[i]->getContainedLinksRef();

		for (int j=0;j<l_pnet.size();j++)
		{
			totalPower  << l_pnet[j]->getObjectId()<<" "
					<< l_pnet[j]->getLinkSource()->getObjectId()<<" -> "
					<< l_pnet[j]->getLinkConsumer()->getObjectId()<<" "
					<< l_pnet[j]->calculate_power() << std::endl<< std::endl;

        	dynamicPower<< l_pnet[j]->getObjectId()<<" "
					<< l_pnet[j]->getLinkSource()->getObjectId()<<" -> "
					<< l_pnet[j]->getLinkConsumer()->getObjectId()<<" "
					<< l_pnet[j]->get_dynamic_power() << std::endl<< std::endl;


        	staticPower << l_pnet[j]->getObjectId()<<" "
					<< l_pnet[j]->getLinkSource()->getObjectId()<<" -> "
					<< l_pnet[j]->getLinkConsumer()->getObjectId()<<" "
					<< l_pnet[j]->get_static_power() <<std::endl<< std::endl;
		}
	#endif
    }
    totalPower   << endl;
    dynamicPower << endl;
    staticPower  << endl;
}

void
GarnetNetwork_d::printPipelineStats(std::ostream& out) const
{
//   	for (int i = 0; i < m_router_ptr_vector.size(); i++){
//	 std::cout << "Router[" << i << "]" <<
//	   "Num VA req: "<< m_router_ptr_vector[i]->getVaLocalCount() << "|" <<
//	   "Num SA req: "<< m_router_ptr_vector[i]->getSaLocalCount() << "|" <<
//	   "Num SA Speculative req: "<< m_router_ptr_vector[i]->getSaSpecLocalCount() << "|" <<
//	   "Num SA rejected no credit: "<< m_router_ptr_vector[i]->getSaNoCredLocalCount() << "|" <<
//	   "Num VA rejected no buff: "<< m_router_ptr_vector[i]->getVaNoBuffLocalCount() << "|" << std::endl;
//    }
//	std::cout << "TOTAL " <<
//	   "Num VA req: "<< Router_d::va_global_arbit_count << "|" <<
//	   "Num SA req: "<< Router_d::sa_global_arbit_count << "|" <<
//	   "Num SA Speculative req: "<< Router_d::saspec_global_arbit_count  << "|" <<
//	   "Num SA rejected no credit: "<< Router_d::sa_nocred_global_arbit_count   << "|" <<
//	   "Num VA rejected no buff: "<< Router_d::va_nobuff_global_arbit_count  << "|" << std::endl;

//Print Bubble Stats


	//std::cout << "[BUBBLE NAVCA STATS]" <<
		//		"Num total checks: "<< Router_d::bubble_total_count << "|" <<
			//	"Num Degradation Bubble: "<< Router_d::bubble_degradation << "|" <<
				//"Num Bubble not found: "<< Router_d::bubble_no_degradation << "|" << std::endl;

}


void
GarnetNetwork_d::print(ostream& out) const
{
    out << "[GarnetNetwork_d]";
}

GarnetNetwork_d *
GarnetNetwork_dParams::create()
{
    return new GarnetNetwork_d(this);
}
