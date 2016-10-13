#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_CREDIT_LINK_PNET_CONTAINER_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_CREDIT_LINK_PNET_CONTAINER_D_HH__

#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_PNET_Container_d.hh"
#include "params/CreditLink_PNET_Container_d.hh"

class CreditLink_PNET_Container_d : public NetworkLink_PNET_Container_d
{
  public:
    typedef CreditLink_PNET_Container_dParams Params;
    CreditLink_PNET_Container_d(const Params *p) : NetworkLink_PNET_Container_d(p)
    {
        std::ostringstream ss;
        ss << typeid(this).name() << objid;
        m_objectId = ss.str();
        
        this->setEventPrio(Event::NOC_CreditLink_Pri);
    };

};

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_CREDIT_LINK_PNET_CONTAINER_D_HH__
