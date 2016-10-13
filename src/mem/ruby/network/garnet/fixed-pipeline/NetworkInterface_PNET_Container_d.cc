#include <cassert>
#include <cmath>
#include <sstream>
#include <fstream>


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
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_PNET_Container_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_PNET_Container_d.hh"

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

NetworkInterface_PNET_Container_d::NetworkInterface_PNET_Container_d(int id, int
							virtual_networks, GarnetNetwork_d *network_ptr)
	: ClockedObject(makeParam(network_ptr)), Consumer(this)
{    
	m_id = id;
    m_net_ptr = network_ptr;
    m_virtual_networks  = virtual_networks;
    m_vc_per_vnet = m_net_ptr->getVCsPerVnet();
    m_num_vcs = m_vc_per_vnet*m_virtual_networks;
	
	std::ostringstream ss;
	ss << typeid(this).name() << m_id;
	m_objectId = ss.str();
	std::cout<<m_objectId<<std::endl;
	//create the contained NiC_d, one per VNET
	m_contained_nics.resize(m_virtual_networks);
	for(int i=0;i<m_virtual_networks;i++)
	{
		m_contained_nics[i]=new NetworkInterface_d(m_id, m_virtual_networks, m_net_ptr);
		m_contained_nics[i]->setMinorId(i);
		m_contained_nics[i]->setPnetContainerPtr(this);
		m_contained_nics[i]->setObjectId();
		
		std::cout<<"\t"<<m_contained_nics[i]->getObjectId()<<std::endl;
	}
}

NetworkInterface_PNET_Container_d::~NetworkInterface_PNET_Container_d()
{
	for(int i=0;i<m_virtual_networks;i++)
		delete m_contained_nics[i];
}

void
NetworkInterface_PNET_Container_d::addInPort(NetworkLink_PNET_Container_d *in_link,
                              CreditLink_PNET_Container_d *credit_link)
{
	assert(m_contained_nics.size()==m_virtual_networks);

	std::vector<NetworkLink_d*>& pnet_links = in_link->getContainedLinksRef();	
    std::vector<NetworkLink_d*>& pnet_credit_links = credit_link->getContainedLinksRef();	
	
	for(int i=0; i<m_virtual_networks; i++)
    {
		m_contained_nics[i]->addInPort(pnet_links[i],(CreditLink_d*)pnet_credit_links[i]);
	}
}

void
NetworkInterface_PNET_Container_d::addOutPort(NetworkLink_PNET_Container_d *out_link,
                               CreditLink_PNET_Container_d *credit_link)
{
	assert(m_contained_nics.size()==m_virtual_networks);

	std::vector<NetworkLink_d*>& pnet_links = out_link->getContainedLinksRef();	
    std::vector<NetworkLink_d*>& pnet_credit_links = credit_link->getContainedLinksRef();	
    
	for(int i=0; i<m_virtual_networks; i++)
    {
		m_contained_nics[i]->addOutPort(pnet_links[i],(CreditLink_d*)pnet_credit_links[i]);
	}
}

void
NetworkInterface_PNET_Container_d::addNode(vector<MessageBuffer *>& in,
                            vector<MessageBuffer *>& out)
{
    assert(in.size() == m_virtual_networks);
    for(auto it=in.begin();it!=in.end();++it)
        if(BaseCPU *p=(*it)->getCpuPointer()) p->addSlaveObject(this);
    for(auto it=out.begin();it!=out.end();++it)
        if(BaseCPU *p=(*it)->getCpuPointer()) p->addSlaveObject(this);

//TODO each NIC should have only a VNET implemented thus have only a portion fo
//the MessageBuffer. However, this limits the case of CatNap where there are different
//PNETs and each of them can support all the traffic types.
// The latter solution is implemented, thus the NIC_d has to implemente the
// minor id and check it before selecting which messagebuffer part to read, i.e.
// which vnet.
	for (int i = 0; i < m_virtual_networks; i++) 
	{

    	m_contained_nics[i]->getInNodePtrRef() = in;
    	m_contained_nics[i]->getOutNodePtrRef() = out;
        // the protocol injects messages into the NI
        (m_contained_nics[i]->getInNodePtrRef()[i])->setConsumer(m_contained_nics[i]);
        (m_contained_nics[i]->getInNodePtrRef()[i])->setClockObj(m_contained_nics[i]);
    }
}

void
NetworkInterface_PNET_Container_d::print(std::ostream& out) const
{
    out << "[Network Interface Container]";
}
