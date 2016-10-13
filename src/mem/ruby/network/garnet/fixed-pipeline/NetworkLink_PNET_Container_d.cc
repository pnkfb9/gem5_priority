#include <iostream>

#include <typeinfo>
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_PNET_Container_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_PNET_Container_d.hh"

int NetworkLink_PNET_Container_d::objidGenerator_PNET=0;

NetworkLink_PNET_Container_d::NetworkLink_PNET_Container_d(const Params *p): ClockedObject(p), Consumer(this)
{
	m_virtual_networks=p->virt_nets;
//set objectid for this component
	objid=objidGenerator_PNET++;
	m_id=objid;
	std::ostringstream ss;
    ss << typeid(this).name() << objid;
    m_objectId = ss.str();
	std::cout<<m_objectId<<std::endl;
//create all the internal link_d
	m_net_link.resize(m_virtual_networks);
	for(int i=0;i<m_virtual_networks;i++)
	{
		m_net_link[i]=new NetworkLink_d((NetworkLink_dParams*)p);
		m_net_link[i]->setMinorId(i);
		m_net_link[i]->setPnetContainerPtr(this);
		m_net_link[i]->setObjectId();

		std::cout<<"\t"<<m_net_link[i]->getObjectId()<<std::endl;
	}
}

NetworkLink_PNET_Container_d::~NetworkLink_PNET_Container_d()
{
	for(int i=0;i<m_virtual_networks;i++)
	{
		delete m_net_link[i];
	}

}
void 
NetworkLink_PNET_Container_d::init_net_ptr(GarnetNetwork_d* net_ptr)
{
	assert(net_ptr!=NULL);
	m_net_ptr = net_ptr;
	for(int i=0;i<m_virtual_networks;i++)
	{
		m_net_link[i]->init_net_ptr(net_ptr);
	}
}
double NetworkLink_PNET_Container_d::get_dynamic_power()
{
	double m_power_dyn=0;
	for(int i=0;i<m_net_link.size();i++)
		m_power_dyn+=m_net_link[i]->get_dynamic_power();
	return m_power_dyn;
}
double NetworkLink_PNET_Container_d::get_static_power()
{
	double m_power_sta=0;
	for(int i=0;i<m_net_link.size();i++)
		m_power_sta+=m_net_link[i]->get_static_power();
	return m_power_sta;
}
double NetworkLink_PNET_Container_d::calculate_power()
{
	double m_power=0;
	for(int i=0;i<m_net_link.size();i++)
		m_power+=m_net_link[i]->calculate_power();
	return m_power;
}

Time NetworkLink_PNET_Container_d::getStatStartTime() const 
{ 	
	return m_net_link[0]->getStatStartTime(); 
}

int NetworkLink_PNET_Container_d::getLinkUtilization()
{
	int link_usage=0;
	for(int i=0;i<m_net_link.size();i++)
		link_usage+=m_net_link[i]->getLinkUtilization();
	return link_usage;

}

void NetworkLink_PNET_Container_d::clearStats()
{
	for(int i=0;i<m_net_link.size();i++)
		m_net_link[i]->clearStats();
}
std::vector<int>
NetworkLink_PNET_Container_d::getVcLoad()
{
	std::vector<int> vc_load;
	vc_load.resize( m_net_ptr->getVCsPerVnet()*m_virtual_networks);
	for(int i=0;i<vc_load.size();i++)
		vc_load[i]=0;
	
	for(int i=0;i<m_net_link.size();i++)
	{
		std::vector<int> temp_vect = m_net_link[i]->getVcLoad();
		assert(temp_vect.size()==vc_load.size());
		for(int j=0;j<temp_vect.size();j++)
			vc_load[j]+=temp_vect[j];
	}
	return vc_load;
}
NetworkLink_PNET_Container_d *
NetworkLink_PNET_Container_dParams::create()
{
    return new NetworkLink_PNET_Container_d(this);
}

CreditLink_PNET_Container_d *
CreditLink_PNET_Container_dParams::create()
{
    return new CreditLink_PNET_Container_d(this);
}

