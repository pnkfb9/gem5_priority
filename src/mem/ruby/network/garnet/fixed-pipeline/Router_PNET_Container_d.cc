
#include "config/use_boost_cpp2011.hh"

#include <typeinfo>
#include <fstream>
#include "base/stl_helpers.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_PNET_Container_d.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_PNET_Container_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_PNET_Container_d.hh"
#include "sim/periodchangeable.hh"
#include "DVFSPolicyPNET.hh"
#include "PowerGatingPolicy.hh"

using namespace std;
using m5::stl_helpers::deletePointers;

Router_PNET_Container_d::Router_PNET_Container_d(const Params *p)
    : BasicRouter(p)
{
	//assert(false);
	#if USE_VNET_REUSE==1 || USE_SPURIOUS_VC_VNET_REUSE==1
	assert(false && "\n\nERROR - Impossible to use VNET-reuse optimization in presence of Physical Network Implementation (PNET)\n\n");
	#endif	

	m_virtual_networks = p->virt_nets;
	assert(m_virtual_networks>0);
	m_vc_per_vnet = p->vcs_per_vnet;
    m_num_vcs = m_virtual_networks * m_vc_per_vnet;

	// set the objid for this Router_PNET_Container
	std::ostringstream ss;
	ss << typeid(this).name() << get_id();
	m_objectId = ss.str();
	// create the contained routers, one per vnet/pnet
	m_contained_routers.resize(m_virtual_networks);
	std::cout<<m_objectId<<std::endl;
	for(int i=0;i<m_virtual_networks;i++)
	{
		m_contained_routers[i] = new Router_d((GarnetRouter_dParams*)p);
		// for the garnet is a clone of an already created router the
		// container!!! Let's change the m_minor_id accordingly
		m_contained_routers[i]->setMinorId(i);
		m_contained_routers[i]->setPnetContainerPtr(this);
		m_contained_routers[i]->setObjectId();
		
		std::cout<<"\t"<<m_contained_routers[i]->getObjectId()<<std::endl;
	}
	

#if USE_PNET==1
    DVFSPolicyPNET::addRouter(this,this->_params->eventq,m_virtual_networks);
#endif

	// FIXME check for the vreg, dvfs and power gating stuff if it is call outside the
	// router by the GarnetNetwork. Since it will be called on the
	// router_container
}

Router_PNET_Container_d::~Router_PNET_Container_d()
{
	for(int i=0;i<m_virtual_networks;i++)
		delete m_contained_routers[i];
}

void
Router_PNET_Container_d::init()
{	//note this function should be called after link each router_d with the
	//required links since they use them in their init funtion
	BasicRouter::init();
	
	//init all the routers, one basicRouter per each router_d
	for(int i=0;i<m_virtual_networks;i++)
	{
		m_contained_routers[i]->init();
	}
	//dvfs and power gating stuff should be regulated in the routers and a new
	//policy should be written to keep track of the pnets
}

/* This function link the network and credit link containers to the approriate
 * routers in the router_container
 */

void
Router_PNET_Container_d::addInPort(NetworkLink_PNET_Container_d *in_link, CreditLink_PNET_Container_d *credit_link)
{
	std::vector<NetworkLink_d*>& pnet_links = in_link->getContainedLinksRef();
	std::vector<NetworkLink_d*>& pnet_credit_links = credit_link->getContainedLinksRef();

	for(int i=0; i<m_virtual_networks;i++)
	{
		m_contained_routers[i]->addInPort(pnet_links[i],(CreditLink_d*)pnet_credit_links[i]);
	}
}

void
Router_PNET_Container_d::addOutPort(NetworkLink_PNET_Container_d *out_link,
    const NetDest& routing_table_entry, int link_weight,
    CreditLink_PNET_Container_d *credit_link)
{
	std::vector<NetworkLink_d*>& pnet_links = out_link->getContainedLinksRef();
	std::vector<NetworkLink_d*>& pnet_credit_links = credit_link->getContainedLinksRef();
	for(int i=0; i<m_virtual_networks;i++)
	{
		m_contained_routers[i]->addOutPort(pnet_links[i],routing_table_entry,link_weight,(CreditLink_d*)pnet_credit_links[i]);
	}
}

Router_PNET_Container_d *
GarnetRouter_PNET_Container_dParams::create()
{
    return new Router_PNET_Container_d(this);
}

double 
Router_PNET_Container_d::calculate_power_resettable()
{
	double pwr_rst=0;
	for(int i=0;i<m_contained_routers.size();i++)
		pwr_rst+=m_contained_routers.at(i)->calculate_power_resettable();
	return pwr_rst;
}
double 
Router_PNET_Container_d::calculate_power()
{
	double pwr=0;
	for(int i=0;i<m_contained_routers.size();i++)
		pwr+=m_contained_routers.at(i)->calculate_power();
	return pwr;
}

double 
Router_PNET_Container_d::get_dynamic_power()
{
	double dyn_pwr=0;
	for(int i=0;i<m_contained_routers.size();i++)
		dyn_pwr+=m_contained_routers.at(i)->get_dynamic_power();
	return dyn_pwr;
}

double 
Router_PNET_Container_d::get_static_power()
{
	double sta_pwr=0;
	for(int i=0;i<m_contained_routers.size();i++)
		sta_pwr+=m_contained_routers.at(i)->get_static_power();
	return sta_pwr;
}

double 
Router_PNET_Container_d::get_clk_power()
{
	double clk_pwr=0;
	for(int i=0;i<m_contained_routers.size();i++)
		clk_pwr+=m_contained_routers.at(i)->get_clk_power();
	return clk_pwr;
}

int 
Router_PNET_Container_d::getCongestion()
{
	int congestion=0;
	for(int i=0;i<m_contained_routers.size();i++)
		congestion+=m_contained_routers.at(i)->getCongestion();
	return congestion;
}

void 
Router_PNET_Container_d::reset_performance_numbers()
{
	for(int i=0;i<m_contained_routers.size();i++)
		m_contained_routers.at(i)->reset_performance_numbers();
}
