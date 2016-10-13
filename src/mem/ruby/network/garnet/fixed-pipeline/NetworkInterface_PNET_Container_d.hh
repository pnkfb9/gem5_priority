/* Politecnico di Milano 2015
 * NetworkInterface_PNET_Container_d implements the container to the
 * NetworkInterface_d objects to allow the simulation of PNET NoCs instead of
 * VNET NoCs.
 * 
 * Author: Davide Zoni
 * Created on: May 21 2015
 * Last Changes on: May 21 2015
*/
#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_INTERFACE_PNET_CONTAINER_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_INTERFACE_PNET_CONTAINER_D_HH__

#include <iostream>
#include <vector>
#include <cassert>

#include <utility>
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutVcState_d.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/slicc_interface/Message.hh"
#include "mem/ruby/buffers/MessageBuffer.hh"

class CreditLink_PNET_Container_d;
class NetworkLink_PNET_Container_d;

class NetworkInterface_PNET_Container_d :  public ClockedObject, public Consumer
{
public:
	NetworkInterface_PNET_Container_d(int id, int virtual_networks,
                       GarnetNetwork_d* network_ptr);

    ~NetworkInterface_PNET_Container_d();
	
	void addInPort(NetworkLink_PNET_Container_d *in_link, CreditLink_PNET_Container_d *credit_link);
    void addOutPort(NetworkLink_PNET_Container_d *out_link, CreditLink_PNET_Container_d *credit_link);

	void wakeup(){assert(false&&"\n\nImpossible to call wakeup() on a container\n\n");};
    void addNode(std::vector<MessageBuffer *> &inNode,
                 std::vector<MessageBuffer *> &outNode);
    
	void print(std::ostream& out) const;
    int get_vnet(int vc);
	
	virtual std::string getObjectId() const { return m_objectId; }
    
    virtual void changePeriod(Tick period){assert(false && "\n\nWARNING CPU DVFS is not supported for PNETs\n\n");};

	int getOutPkt()
	{
		//std::cout<<"getOutPkt()"<<std::endl;
		return c1_out_ni_pkt;
	}

	void incrementOutPkt(int increment)
	{
		//std::cout<<"incrementOutPkt()"<<std::endl;
		assert(false&& "\n\nTODO implemented in the NIC_CONTAINER_d\n\n");
		c1_out_ni_pkt+=increment;
	}

	void resetOutPkt()
	{
		//std::cout<<"resetOutPkt()"<<std::endl;
		assert(false&& "\n\nTODO implemented in the NIC_CONTAINER_d\n\n");
		c1_out_ni_pkt=0;
	}

	GarnetNetwork_d* getNetPtr() { 	assert(m_net_ptr!=NULL);  return m_net_ptr; }

    //std::vector<OutVcState_d *>& getOutVcState_d_ref(){return m_out_vc_state;}
    
    inline int
    getCongestion(int out_vc_ni_buffer)
    {
		assert(false && "\n\nTODO implemented in the NIC_CONTAINER_d\n\n");
		//TODO loop on all the nic_d to get thir congestion
	//	int tempVal=0;
	//	for(int i=0;i<m_virtual_networks;i++)
	//		
    //    return m_ni_buffers.at(out_vc_ni_buffer)->getCongestion();
    }
    
	// called by Router_d
    //Consumer *getConnectedComponent() { return outNode_ptr.at(0)->getConsumer(); }
    //Consumer *getNoCSideConnectedComponent() { return outNetLink->getLinkConsumer(); }
    
	private:	
	/*congestion metrics*/
	//std::vector<CongestionMetrics> c1_ni_matrix;

	int  c1_out_ni_pkt;

	//int rr_vnet_ins_msg;
  	//Cycles old_get_curCycle;

    std::string m_objectId;

    GarnetNetwork_d *m_net_ptr;
    int m_virtual_networks, m_num_vcs, m_vc_per_vnet;
    NodeID m_id;
	public:
		int get_id(){return (int)m_id;}
	private:
	//varible of the container
	std::vector<NetworkInterface_d*> m_contained_nics;

	// vc is not idle
	std::vector<int> vnet_in_this_vc;
	int m_virtual_networks_protocol;
	int m_virtual_networks_spurious;

};
#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_INTERFACE_PNET_CONTAINER_D_HH__
