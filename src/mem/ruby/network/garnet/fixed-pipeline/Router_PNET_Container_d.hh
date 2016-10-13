/* Router_PNET_d implements multiple routers, i.e. the number of the physical
 * vnets (PVNETs). It overloads all the Router_d functions to be used seemlessly
 * in the GarnetNetwork_d class. However, per each function that deals
 * explicitly on vnets an additional parameter has to be specified: the vnet.
 * Thus the Router_PNET_d can route the function calling the appropriate
 * router_d contained. It pairs with NetworkInterface_PNET, NetworkLink_PNET_d and
 * CreditLink_PNET_d.
 * 
 * NOTE: this is only a container, without any possibility to schedule events or
 * wakeup to answer to them. However, it has to inherit from GarnetRouter
 *
 * Author Davide Zoni
 * email: davide.zoni@polimi.it
 *
*/
#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_ROUTER_PNET_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_ROUTER_PNET_D_HH__

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"

#include "config/use_vnet_reuse.hh"
#include "config/use_spurious_vc_vnet_reuse.hh"
#include "params/GarnetRouter_PNET_Container_d.hh"

class NetworkLink_PNET_Container_d;
class CreditLink_PNET_Container_d;

class Router_PNET_Container_d: public BasicRouter
{
	public:
		
	typedef GarnetRouter_PNET_Container_dParams Params; 

    Router_PNET_Container_d(const Params *p);
    ~Router_PNET_Container_d();

    void init();
    void addInPort(NetworkLink_PNET_Container_d *link, CreditLink_PNET_Container_d *credit_link);
    void addOutPort(NetworkLink_PNET_Container_d *link, const NetDest& routing_table_entry,
                    int link_weight, CreditLink_PNET_Container_d *credit_link);


    int get_num_vcs()       { return m_num_vcs; }
    int get_num_vnets()     { return m_virtual_networks; }
    int get_vc_per_vnet()   { return m_vc_per_vnet; }

    int get_num_inports()   
	{// all PNETs has to have the same number of outports
		for(int i=0;i<m_virtual_networks;i++)
		{
			assert( (m_contained_routers[i]->get_inputUnit_ref()).size()==
						(m_contained_routers[i+1]->get_inputUnit_ref()).size() );
		} 
		return (m_contained_routers[0]->get_inputUnit_ref()).size(); 
	}
    
	int get_num_outports()  
	{// all PNETs has to have the same number of outports
		for(int i=0;i<m_virtual_networks;i++)
		{
			assert( (m_contained_routers[i]->get_outputUnit_ref()).size()==
						(m_contained_routers[i+1]->get_outputUnit_ref()).size() );
		} 
		return (m_contained_routers[0]->get_outputUnit_ref()).size(); 
	}

    int get_id(){ return m_id; }
    virtual std::string getObjectId() const { return m_objectId; }

    void init_net_ptr(GarnetNetwork_d* net_ptr) 
    {
    	if(m_contained_routers.size()!=m_virtual_networks)
    		std::cout<<"contained_r: "<<m_contained_routers.size()
    					<<" vnets: "<<m_virtual_networks<<std::endl;
    	assert(m_contained_routers.size()==m_virtual_networks);
        m_network_ptr = net_ptr;
    	for(int i=0;i<m_virtual_networks;i++)
    	{
    		m_contained_routers[i]->init_net_ptr(net_ptr);
    	} 
    }

	std::vector<Router_d*>& getContainedRoutersRef(){return m_contained_routers;}
	private:
	int m_virtual_networks;// the actual number of physical network to be implemented, since we suppose one PNET per VNET for now
	int m_vc_per_vnet;
	int m_num_vcs;
    std::string m_objectId;
	
	std::vector<Router_d*> m_contained_routers;
	GarnetNetwork_d* m_network_ptr;

	public:
		// power methods for garnetnetwork_d
		double calculate_power_resettable();
		double calculate_power();
		double get_dynamic_power();
    	double get_static_power();
    	double get_clk_power();
		int getCongestion();
		void reset_performance_numbers();
};




#endif

