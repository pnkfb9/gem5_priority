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
#include "mem/ruby/network/garnet/fixed-pipeline/Router_PNET_Container_d.hh"

#if USE_SYNTHETIC_TRAFFIC==0 && USE_MOESI_HAMMER==0
#include "mem/protocol/L2Cache_Controller.hh"
#endif

#if USE_VOQ == 0
	#if USE_ADAPTIVE_ROUTING == 0
		#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_d.hh"
		#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_d.hh"
	#else
		#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_ADAPTIVE_d.hh"
		#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_ADAPTIVE_d.hh"		
	#endif
#else
	#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_VOQ_d.hh"
	#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_VOQ_d.hh"
#endif

#include "mem/ruby/network/garnet/fixed-pipeline/SWallocator_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Switch_d.hh"

#include "sim/periodchangeable.hh"
#include "DVFSPolicy.h"
#include "PowerGatingPolicy.hh"
#if USE_BOOST_CPP2011 == 1
	#include <boost/random.hpp>
#endif

using namespace std;
using m5::stl_helpers::deletePointers;


//Speculative statistics	
float Router_d::global_arbit_count=0;
float Router_d::va_global_arbit_count=0;
float Router_d::sa_global_arbit_count=0;
float Router_d::saspec_global_arbit_count=0;
float Router_d::sa_nocred_global_arbit_count=0;
float Router_d::va_win_global_arbit_count=0;
float Router_d::va_nobuff_global_arbit_count=0;

float Router_d::bubble_total_count=0;
float Router_d::bubble_degradation=0;
float Router_d::bubble_no_degradation=0;

Router_d::Router_d(const Params *p)
    : BasicRouter(p)
{
	//assert(false);
	#if USE_VICHAR==1
		//assert(m_network_ptr!=NULL);
		totVicharSlotPerVnet=0;
		totVicharSlotPerVnet = p->totVicharSlotPerVnet;
		std::cout<< totVicharSlotPerVnet<<std::endl;	
		assert(totVicharSlotPerVnet>0);
	#endif

    vreg=0;
	m_num_fifo_resynch_slots = p->numFifoResynchSlots;
	assert(m_num_fifo_resynch_slots>0);

	m_virtual_networks_protocol = p->virt_nets;
	#if USE_SPURIOUS_VC_VNET_REUSE==1
    m_virtual_networks = p->virt_nets+p->virt_nets_spurious;
    m_virtual_networks_spurious = p->virt_nets_spurious;
	#else
    m_virtual_networks = p->virt_nets;
	m_virtual_networks_spurious = 0;
	#endif

	#if USE_SPURIOUS_VC_VNET_REUSE==1
	std::cout<<"m_virtual_networks: "<<m_virtual_networks
			<<" m_virtual_networks_protocol: "<<m_virtual_networks_protocol
			<<" m_virtual_networks_spurious: "<<m_virtual_networks_spurious
			<<std::endl;
	#endif
    
	m_vc_per_vnet = p->vcs_per_vnet;
    m_num_vcs = m_virtual_networks * m_vc_per_vnet;

/** Manage the different definition for the routing unit and VA_unit***/
#if USE_VOQ == 0
	#if USE_ADAPTIVE_ROUTING == 0
    	m_routing_unit = new RoutingUnit_d(this);
		m_vc_alloc = new VCallocator_d(this);
	#else
		m_routing_unit = new RoutingUnit_ADAPTIVE_d(this);
		m_vc_alloc = new VCallocator_ADAPTIVE_d(this);
	#endif
#else
    m_routing_unit = new RoutingUnit_VOQ_d(this);
	m_vc_alloc = new VCallocator_VOQ_d(this);
#endif
/*************************************************************/    
    m_sw_alloc = new SWallocator_d(this);
    m_switch = new Switch_d(this);

    m_input_unit.clear();
    m_output_unit.clear();

    crossbar_count = 0;
    sw_local_arbit_count = 0;
    sw_global_arbit_count = 0;
    buf_read_count.resize(m_virtual_networks);
    buf_write_count.resize(m_virtual_networks);
    vc_local_arbit_count.resize(m_virtual_networks);
    vc_global_arbit_count.resize(m_virtual_networks);
    for (int i = 0; i < m_virtual_networks; i++) {
        buf_read_count[i] = 0;
        buf_write_count[i] = 0;
        vc_local_arbit_count[i] = 0;
        vc_global_arbit_count[i] = 0;
    }

	router_frequency=p->router_frequency;
	router_frequency_adaptive=p->router_frequency;
	//std::cout<<"@"<<curTick()<<" R"<<m_id
	//			<<" router_frequency:"<<router_frequency
	//			<<" SimClock::Frequency:"<<SimClock::Frequency
	//			<<" AskForChangePeriod i.e. new period:"<<SimClock::Frequency/router_frequency
	//							<<std::endl;


#if USE_PNET==0 
//all this initialization has to be moved when the objid is set for the PNET
// impl since the ptr to the container is required!!!
    std::ostringstream ss;
    ss << typeid(this).name() << get_id();
    m_objectId = ss.str();

// Power Gating policy introduced here	
	PowerGatingPolicy::addRouter(this,this->_params->eventq);

// DFS policy introduced here
#if USE_PNET==0
    DVFSPolicy::addRouter(this,this->_params->eventq);
#endif
	// try to immediately change frequency to the correct period at the
	// beginning of the simulation
	assert(SimClock::Frequency/router_frequency>0);
	this->changePeriod(SimClock::Frequency/router_frequency); 
#endif // if USE_PNET==0, namely use virtual network implementation

    orion_rtr_ptr=NULL;
    orion_cfg_ptr=NULL;
}

void
Router_d::setObjectId()
{
#if USE_PNET==1
    std::ostringstream ss;
    ss <<
	typeid(m_container_ptr).name()<<m_container_ptr->get_id()<<"."<<typeid(this).name() << getMinorId();
    m_objectId = ss.str();

// Power Gating policy introduced here	
	PowerGatingPolicy::addRouter(this,this->_params->eventq);

// DFS policy introduced here
//    DVFSPolicy::addRouter(this,this->_params->eventq);
  
	// try to immediately change frequency to the correct period at the
	// beginning of the simulation
	assert(SimClock::Frequency/router_frequency>0);
	this->changePeriod(SimClock::Frequency/router_frequency); 
#else
	assert(false&&"\n\nImpossible to call this function when PNET impl is not used\n\n");
#endif
}

Router_d::~Router_d()
{
    deletePointers(m_input_unit);
    deletePointers(m_output_unit);
    delete m_routing_unit;
    delete m_vc_alloc;
    delete m_sw_alloc;
    delete m_switch;

	//delete power model pointers
	delete orion_cfg_ptr;
	delete orion_rtr_ptr;
}

void
Router_d::init()
{
    BasicRouter::init();

	m_routing_unit->init(); //used in the (outputvirtualqueue) VOQ

    m_vc_alloc->init();
    m_sw_alloc->init();
    m_switch->init();
	
	init_power_model();//init orion power model in GEM5 1 for each router

#if USE_OCTAVE
	assert(false && "Enable at least init_sleep_model();");
	if(m_id==5)
	{
		/*init the sleep model if defined*/
		//init_sleep_model();
		/*add a logger for each logic component*/
		//init_rt_logger_model();
	
		//initial_policy_delay=100;
		//powerGatingPolicy=new SimplePowerGatingPolicy("test_policy",this);
		//powerGatingPolicy->scheduleEvent(initial_policy_delay);
	}
#endif
	c1_matrix.resize(get_num_inports() * get_num_vcs());
	for(int i=0;i<c1_matrix.size();i++)
		c1_matrix[i].resize(get_num_outports());
	
	c2_matrix.resize(get_num_vcs());
	for(int i=0;i<c2_matrix.size();i++)
		c2_matrix[i].resize(get_num_outports());

	c1_in_pkt.resize(get_num_outports());
	for(int i=0;i<c1_in_pkt.size();i++)
		c1_in_pkt[i]=0;
	
	m_outFlits=0;
  
  buffer_reused_count=0;
  head_and_tail_count=0;
  only_headtail_count=0;
  
	last_power_calc_all_file=1;
	last_power_calc_sw_file=1;


/*set up additional variables for run-time power evaluation*/
	buf_read_count_rt.resize(m_virtual_networks);
	buf_write_count_rt.resize(m_virtual_networks);
	vc_local_arbit_count_rt.resize(m_virtual_networks);
	vc_global_arbit_count_rt.resize(m_virtual_networks);
	
	buf_read_count_rt_prev.resize(m_virtual_networks);
	buf_write_count_rt_prev.resize(m_virtual_networks);
	vc_local_arbit_count_rt_prev.resize(m_virtual_networks);
	vc_global_arbit_count_rt_prev.resize(m_virtual_networks);
	for (int i = 0; i < m_virtual_networks; i++) 
	{
       		 buf_read_count_rt[i] = 0;
       		 buf_write_count_rt[i] = 0;
       		 vc_local_arbit_count_rt[i] = 0;
       		 vc_global_arbit_count_rt[i] = 0;

       		 buf_read_count_rt_prev[i] = 0;
       		 buf_write_count_rt_prev[i] = 0;
       		 vc_local_arbit_count_rt_prev[i] = 0;
       		 vc_global_arbit_count_rt_prev[i] = 0;
	}
	crossbar_count_rt = 0;
	sw_local_arbit_count_rt = 0;
	sw_global_arbit_count_rt = 0;

	crossbar_count_rt_prev = 0;
	sw_local_arbit_count_rt_prev = 0;
	sw_global_arbit_count_rt_prev = 0;

	avg_pwr_total=0;
	avg_pwr_clock=0;
	avg_pwr_static=0;
	avg_pwr_dynamic=0;
	avg_pwr_tick_elapsed=0;

//
//	std::cout<<"Router"<<m_id<<std::endl;
//	for(int i=0;i<m_output_unit.size();i++)
//	{
////		std::cout<<"\tOutport"<<m_output_unit.at(i)->get_id() <<" to: "
////			<<((m_output_unit.at(i)->getOutLink_d())->getLinkConsumer())->getObjectId()<<std::endl;	
//		Consumer *maybeNiface=m_output_unit.at(i)->getOutLink_d()->getLinkConsumer();
//		NetworkInterface_d *niface=dynamic_cast<NetworkInterface_d*>(maybeNiface);
//		if(niface)
//		{
//			
//			Consumer *maybeL2OrMemCtrl=niface->getConnectedComponent();
//			if(maybeL2OrMemCtrl)
//            {
//				std::cout<<"\tOutport"<<m_output_unit.at(i)->get_id() <<" to: "
//					<<typeid(*maybeL2OrMemCtrl).name()<<" "<<maybeL2OrMemCtrl->getObjectId()<<std::endl;
//			}
//			else
//			{
//				std::cout<<"\tOutport"<<m_output_unit.at(i)->get_id() <<" to: "
//					<<typeid(*niface).name()<<std::endl;
//
//			}
//		}
//
//	}
//    
#if USE_SYNTHETIC_TRAFFIC==0
    //Add output links and credit links as slave objects
    for(int i=0;i<m_output_unit.size();i++)
		addSlaveObject(m_output_unit.at(i)->getOutLink_d());
	
	for(int i=0;i<m_input_unit.size();i++)
		addSlaveObject(m_input_unit.at(i)->getCreditLink_d());
   

	#if USE_MOESI_HAMMER==0 // using MOESI_hammer no l2 coherence is present, i.e. no l2_cache_controller
	#if USE_PNET==0// PNET run each l1 l2 or mc to its own freq resynchronizing at the nic border
	for(int i=0;i<m_output_unit.size();i++)
    {
        Consumer *maybeNiface=m_output_unit.at(i)->getOutLink_d()->getLinkConsumer();
        cout<<"***"<<getObjectId()<<" is connected to "<<typeid(*maybeNiface).name();
        NetworkInterface_d *niface=dynamic_cast<NetworkInterface_d*>(maybeNiface);
        if(niface)
        {
            Consumer *maybeL2OrMemCtrl=niface->getConnectedComponent();
            if(maybeL2OrMemCtrl)
            {
                cout<<" which is connected to "<<typeid(*maybeL2OrMemCtrl).name()<<endl;
                L2Cache_Controller *L2=dynamic_cast<L2Cache_Controller*>(maybeL2OrMemCtrl);
                if(L2)
                {
                    addSlaveObject(niface->getClockedObject());
                    addSlaveObject(L2->getClockedObject());
                    assert(L2->getSequencer()==0);
                }
            }
        }
        cout<<endl;
    }
	#endif
	#endif
#endif
	#if USE_APNEA_BASE==1
	//init the used vcs to the whole vc set. NOTE: it can be modified in the
	//APNEA_configuration_policy event!!!
	initVcOp.resize(m_output_unit.size());
	finalVcOp.resize(m_output_unit.size());
	initVcIp.resize(m_input_unit.size());
	finalVcIp.resize(m_input_unit.size());
	assert(m_input_unit.size()==m_output_unit.size());
	for(int i=0;i<m_input_unit.size();i++)
	{
	//init as the baseline, eventually modified with apnea policy
		initVcOp.at(i)=0;
		finalVcOp.at(i)=m_num_vcs;
		initVcIp.at(i)=0;
		finalVcIp.at(i)=m_num_vcs;
	}
	#endif

	// ratio count the used buffer
	used_buf_inport.resize(m_input_unit.size());
	last_used_buf_inport.resize(m_input_unit.size());
	last_computed_used_buf_inport_tick=0;
	for(int i=0;i<m_input_unit.size();i++)
	{
		used_buf_inport[i].resize(m_num_vcs);
		last_used_buf_inport[i].resize(m_num_vcs);
		for(int j=0;j<m_num_vcs;j++)
		{
			used_buf_inport[i][j]=0;
			last_used_buf_inport[i][j]=0;
		}
	}
	#if USE_LRC==1
	calledVAthisCycle=0;
	#endif

	if(m_network_ptr->clockPeriod()>this->clockPeriod())
		std::cout<<"@"<<curTick()
			<<" R"<<m_id
			<<" m_network_ptr->clockPeriod():"<<m_network_ptr->clockPeriod()
			<<" router->clockPeriod():"<<this->clockPeriod()
			<<std::endl;

	assert(m_network_ptr->clockPeriod()<=this->clockPeriod()&& "This \
implementation allows for router frequencies lower than or equal to Network\
frequency. NOTE: Network Freq is actually the system frequency and CPU freq\
if nobody changes it.");

}

void
Router_d::addInPort(NetworkLink_d *in_link, CreditLink_d *credit_link)
{
    int port_num = m_input_unit.size();
    InputUnit_d *input_unit = new InputUnit_d(port_num, this);

    input_unit->set_in_link(in_link);
    input_unit->set_credit_link(credit_link);
    in_link->setLinkConsumer(input_unit);
	
	credit_link->setLinkSource(input_unit);

    credit_link->setSourceQueue(input_unit->getCreditQueue());

    m_input_unit.push_back(input_unit);
}

void
Router_d::addOutPort(NetworkLink_d *out_link,
    const NetDest& routing_table_entry, int link_weight,
    CreditLink_d *credit_link)
{
    int port_num = m_output_unit.size();
    OutputUnit_d *output_unit = new OutputUnit_d(port_num, this);

    output_unit->set_out_link(out_link);
    output_unit->set_credit_link(credit_link);
    credit_link->setLinkConsumer(output_unit);
    out_link->setSourceQueue(output_unit->getOutQueue());
	
	out_link->setLinkSource(output_unit);
   
	m_output_unit.push_back(output_unit);

    m_routing_unit->addRoute(routing_table_entry);
    m_routing_unit->addWeight(link_weight);
}

void
Router_d::route_req(flit_d *t_flit, InputUnit_d *in_unit, int invc)
{
    m_routing_unit->RC_stage(t_flit, in_unit, invc);
}

void
Router_d::vcarb_req()
{
    m_vc_alloc->scheduleEvent(1);
}

void
Router_d::swarb_req()
{
    m_sw_alloc->scheduleEvent(1);
}

void
Router_d::update_incredit(int in_port, int in_vc, int credit)
{
    m_input_unit[in_port]->update_credit(in_vc, credit);
}

void
Router_d::update_sw_winner(int inport, flit_d *t_flit)
{
    m_switch->update_sw_winner(inport, t_flit);
    m_switch->scheduleEvent(1);
}

void
Router_d::calculate_performance_numbers_resettable()
{
    for (int j = 0; j < m_virtual_networks; j++) {
        for (int i = 0; i < m_input_unit.size(); i++) {
            buf_read_count[j] += m_input_unit[i]->get_buf_read_count_resettable(j);
            buf_write_count[j] += m_input_unit[i]->get_buf_write_count_resettable(j);
        }

        vc_local_arbit_count[j]  = m_vc_alloc->get_local_arbit_count_resettable(j);
        vc_global_arbit_count[j] = m_vc_alloc->get_global_arbit_count_resettable(j);
    }

    sw_local_arbit_count = m_sw_alloc->get_local_arbit_count_resettable();
    sw_global_arbit_count = m_sw_alloc->get_global_arbit_count_resettable();
    crossbar_count = m_switch->get_crossbar_count_resettable();
}

void
Router_d::calculate_performance_numbers()
{
    for (int j = 0; j < m_virtual_networks; j++) {
        for (int i = 0; i < m_input_unit.size(); i++) {
            buf_read_count[j] += m_input_unit[i]->get_buf_read_count(j);
            buf_write_count[j] += m_input_unit[i]->get_buf_write_count(j);
        }

        vc_local_arbit_count[j]  = m_vc_alloc->get_local_arbit_count(j);
        vc_global_arbit_count[j] = m_vc_alloc->get_global_arbit_count(j);
    }

    sw_local_arbit_count = m_sw_alloc->get_local_arbit_count();
    sw_global_arbit_count = m_sw_alloc->get_global_arbit_count();
    crossbar_count = m_switch->get_crossbar_count();
}



void
Router_d::reset_performance_numbers()
{
    for (int i = 0; i < m_input_unit.size(); i++)
    {
        m_input_unit[i]->reset_buf_count();
    }
    m_vc_alloc->reset_arbit_count();
    m_sw_alloc->reset_arbit_count();
    m_switch->reset_crossbar_count();
}

/*
	This function returns the local congestion on the router buffers.
	The local congestion is intended as the filling level of the buffers on the input ports
*/
int
Router_d::getCongestion() const
{
    int result=0;
    for(int i=0;i<m_input_unit.size();i++)
        result+=m_input_unit[i]->getCongestion();
    return result;
}

int
Router_d::getFullCongestion()
{
#if USE_PNET==0
    std::vector<Router_d *>& r_vect=this->get_net_ptr()->getRouters_d_ref();
    std::vector<NetworkInterface_d*>& ni_vect=this->get_net_ptr()->getNetworkInterface_d_ref();

    /**************************************************************************************/                                
    // increment the in_flits that are for me from nighbor routers      
    // this count the number of flits stored for me in the upstream invc routers
    int tot_count_busy_inport=0;                                                                                    
    for(int j=0;j<r_vect.size();j++)
    {           
        std::vector<OutputUnit_d *>& ou=r_vect.at(j)->get_outputUnit_ref();
        for(int i=0;i<ou.size();i++)
        {
            if(ou.at(i)->getOutLink_d()->getLinkConsumer()->getObjectId()==this->getObjectId())
            {       
                std::vector<InputUnit_d *>& iu=r_vect.at(j)->get_inputUnit_ref();
                for(int k=0;k < iu.size();k++)
                {       // loop on input port of neighbor routers
                    for(int q=0;q<iu.at(k)->get_num_vcs();q++)
                        if( iu.at(k)->isReady(q, r_vect.at(j)->curCycle())==true && iu.at(k)->get_route(q)==i)
                            tot_count_busy_inport+=iu.at(k)->getCongestion(q);

                }
            }
        }
    }

   for(int j=0;j<ni_vect.size();j++)
    {
        if(ni_vect.at(j)->getOutNetLink()->getLinkConsumer()->getObjectId()==this->getObjectId())
        {
            std::vector<OutVcState_d *>& outVcState =ni_vect.at(j)->getOutVcState_d_ref();
            for(int k=0;k<outVcState.size();k++)
                // loop on vcs
                if(outVcState.at(k)->isInState(IDLE_,ni_vect.at(j)->curCycle())==false)
                    tot_count_busy_inport+=ni_vect.at(j)->getCongestion(k);
        }
    }
    return tot_count_busy_inport;
#else //USE_PNET==1
	assert(false&&"Not implemented for the PNET impl");

#endif
}

void
Router_d::printFullCongestionData(ostream& dump)
{
#if USE_PNET==0
    dump<<this->clockPeriod();
    std::vector<Router_d *>& r_vect=this->get_net_ptr()->getRouters_d_ref();
    std::vector<NetworkInterface_d*>& ni_vect=this->get_net_ptr()->getNetworkInterface_d_ref();

    int tot_count_busy_inport=getFullCongestion();
/**************************************************************************************/
    // increment the in_flits that are for me from nighbor routers	
    int count_busy_inport=0;
    for(int j=0;j<r_vect.size();j++)
    {
        std::vector<OutputUnit_d *>& ou=r_vect.at(j)->get_outputUnit_ref();
        for(int i=0;i<ou.size();i++)
        {
            if(ou.at(i)->getOutLink_d()->getLinkConsumer()->getObjectId()==this->getObjectId())
            {
                std::vector<InputUnit_d *>& iu=r_vect.at(j)->get_inputUnit_ref();
                for(int k=0;k < iu.size();k++)
                {	// loop on input port of neighbor routers
                    for(int q=0;q<iu.at(k)->get_num_vcs();q++)
                        if( iu.at(k)->isReady(q,r_vect.at(j)->curCycle())==true && iu.at(k)->get_route(q)==i)
                            count_busy_inport++;		

                }
            }	
        }
    }

    for(int j=0;j<ni_vect.size();j++)
    {
        if(ni_vect.at(j)->getOutNetLink()->getLinkConsumer()->getObjectId()==this->getObjectId())
        {
            std::vector<OutVcState_d *>& outVcState =ni_vect.at(j)->getOutVcState_d_ref();
            for(int k=0;k<outVcState.size();k++)
                if(outVcState.at(k)->isInState(IDLE_,ni_vect.at(j)->curCycle())==false)
                    count_busy_inport++;	
        }
    }
/**************************************************************************************/

    // write c1 CONGESTION data to dump file for ATTACHED NETWORK INTERFACES
    for(int j=0;j<ni_vect.size();j++)
    {
        if(ni_vect[j]->getOutNetLink()->getLinkConsumer()->getObjectId()==this->getObjectId())
        {
            //dump NI stats ni_CPU, ni_L2 and ni_MC if any 
            // NOTE: stats RESET INCLUDED!!. 
            dump<<" "<<ni_vect[j]->getC1Cycles();
        }
    }

    // write c1 CONGESTION data to dump file from NEIGHBOR ROUTERS
    for(int j=0;j<r_vect.size();j++)
    {
        for(int i=0;i<r_vect[j]->get_num_outports();i++)
        {
            if(r_vect.at(j)->get_outputUnit_ref()[i]->getOutLink_d()->getLinkConsumer()->getObjectId()==this->getObjectId())
                dump<<" "<<r_vect[j]->getC1(i);
                //dump<<" Router"<<j<<" "<<r_vect[j]->getC1(i);
        }
    }

    // write c1_ni_pkt_info to dump file for ATTACHED NETWORK INTERFACES
    for(int j=0;j<ni_vect.size();j++)
    {
        if(ni_vect[j]->getOutNetLink()->getLinkConsumer()->getObjectId()==this->getObjectId())
        {
            //std::cout<<ni_vect[j]->getObjectId() <<" "<<ni_vect[j]->getOutPkt() <<std::endl;

            //dump NI stats ni_CPU, ni_L2 and ni_MC if any.
            dump<<" "<<ni_vect[j]->getOutPkt();
            //reset pkt
            ni_vect[j]->resetOutPkt();
        }
    }

    // write c1_in_pkt_info to dump file from NEIGHBOR ROUTERS
    for(int j=0;j<r_vect.size();j++)
    {
        for(int i=0;i<r_vect[j]->get_num_outports();i++)
        {
            if(r_vect.at(j)->get_outputUnit_ref()[i]->getOutLink_d()->getLinkConsumer()->getObjectId()==this->getObjectId())
            {
                dump<<" "<<r_vect[j]->getInPktOutport(i);
                r_vect[j]->resetInPktOutport(i);
            }
            //dump<<" Router"<<j<<" "<<r_vect[j]->getC1(i);
        }
        //reset in_pkt statistics
    }
    dump<<" "<<count_busy_inport; // it should count if a vc is free or not 
    dump<<" "<<tot_count_busy_inport; // it should count the effective number of flit stored per vc per inport

    // write the output flits from this router as a measure to mitigate the in packets
    dump<<" "<<this->getOutFlits();
    this->resetOutFlits();

    dump<<endl;
#endif
}
void
Router_d::computeRouterPower(double VddActual)
{
    PowerContainerRT res_pwr;
    //std::cout<<"compute power for router"<<m_id<<std::endl;
    calculate_performance_numbers_run_time();
    res_pwr = calculate_power_run_time(last_power_calc_all_file,VddActual);
	updateFinalAvgPwr(res_pwr);
    last_power_calc_all_file=curTick();
}
void
Router_d::printRouterPower(ostream& outfreq,double VddActual)
{

    PowerContainerRT res_pwr;
    //std::cout<<"compute power for router"<<m_id<<std::endl;
    calculate_performance_numbers_run_time();

    res_pwr = calculate_power_run_time(last_power_calc_all_file,VddActual);

    outfreq<<last_power_calc_all_file<<" "
        <<curTick()<<" "
        <<router_frequency<<" "
        <<res_pwr.PtotBlock<<" "
        <<res_pwr.PdynBlock<<" "
        <<res_pwr.PstaticBlock<<" "
        <<res_pwr.PdynClock<<" "
        <<getCongestion()<<" "
		<<getFullCongestion()<<" "
		<<VddActual
        <<std::endl;

	updateFinalAvgPwr(res_pwr);

    last_power_calc_all_file=curTick();
}

void
Router_d::printSWPower(ostream &dump)
{
	// check if the switch is ON or OFF 
	// assert if switch off and dyn power > 0
	PowerContainerRT sw_pwr;
	calculate_performance_numbers_run_time();
	sw_pwr=calculate_power_sw(last_power_calc_sw_file);
    dump<<last_power_calc_sw_file<<" "
        <<curTick()<<" "
        <<router_frequency<<" "
        <<sw_pwr.PtotBlock<<" "
        <<sw_pwr.PdynBlock<<" "
        <<sw_pwr.PstaticBlock<<" "
        <<getActualPowerStateSW(0)<<" "
        <<getCongestion()
        <<std::endl;


    last_power_calc_sw_file=curTick();
}

Tick
Router_d::getC1(int outport)
{
	Tick results=0;
	for(int i=0;i<c1_matrix.size();i++)
	{
		results+=c1_matrix.at(i).at(outport).count;
		if(c1_matrix.at(i).at(outport).t_start!=-1)
		{
			results+= curCycle()- c1_matrix.at(i).at(outport).t_start;
			c1_matrix.at(i).at(outport).t_start=curCycle();
		}	
		c1_matrix.at(i).at(outport).count=0;
	}
	return results;
}


Tick
Router_d::getC2(int outport)
{
	Tick results=0;
	for(int i=0;i<c2_matrix.size();i++)
	{
		results+=c2_matrix.at(i).at(outport).count;
		if(c2_matrix.at(i).at(outport).t_start!=-1)
		{
			results+= curCycle()- c2_matrix.at(i).at(outport).t_start;
			c2_matrix.at(i).at(outport).t_start=curCycle();
		}	
		c2_matrix.at(i).at(outport).count=0;
	}
	return results;
}

void Router_d::changePeriod(Tick period)
{
	ClockedObject::changePeriod(period);
    
    /*
     * We can't really change our period if not on a clock edge.
     * We will be called again by a PeriodChangeEvent generated
     * by the clocked object.
     */
    if(curTick()!=clockEdge()) return;
    
    router_frequency=SimClock::Frequency/period;
	
    changeSlaveObjectPeriod(period);
}

void
Router_d::printFaultVector(ostream& out)
{
    int temperature_celcius = BASELINE_TEMPERATURE_CELCIUS;
    int num_fault_types = m_network_ptr->fault_model->number_of_fault_types;
    float fault_vector[num_fault_types];
    get_fault_vector(temperature_celcius, fault_vector);
    out << "Router-" << m_id << " fault vector: " << endl;
    for (int fault_type_index = 0; fault_type_index < num_fault_types;
         fault_type_index++){
        out << " - probability of (";
        out << 
        m_network_ptr->fault_model->fault_type_to_string(fault_type_index);
        out << ") = ";
        out << fault_vector[fault_type_index] << endl;
    }
}

void
Router_d::printAggregateFaultProbability(std::ostream& out)
{
    int temperature_celcius = BASELINE_TEMPERATURE_CELCIUS;
    float aggregate_fault_prob;
    get_aggregate_fault_probability(temperature_celcius, 
                                    &aggregate_fault_prob);
    out << "Router-" << m_id << " fault probability: ";
    out << aggregate_fault_prob << endl;
}

Router_d *
GarnetRouter_dParams::create()
{
    return new Router_d(this);
}

std::vector<NetDest>&
Router_d::getRoutingTable()
{
	return m_routing_unit->getRoutingTable();
}

std::vector<int>&
Router_d::getRoutingTableWeights()
{
	return m_routing_unit->getRoutingTableWeights();
}

#if USE_SPECULATIVE_VA_SA==1
flit_d*
Router_d::spec_get_switch_buffer_flit(int inport)
{
	return m_switch->get_switch_buffer_flit(inport);
}
bool
Router_d::spec_get_switch_buffer_ready(int inport, Time curTime)
{
	return m_switch->get_switch_buffer_ready(inport, curTime);
}	
#endif
void
Router_d::print_pipeline_state(flit_d* t_flit, std::string stage)
{	
int router_num;
int flit_num;
bool enable;

// SETTING
//-1 print all
router_num=-1;
flit_num=1;
enable=true;

	if(enable){
		bool print;
		print=false;	
		int r_id;	
		r_id=this->get_id();
		if(t_flit!=NULL){
			if(router_num<=0 && flit_num>=0){
					//Print all routers onfly flit filter
					if(t_flit->getUniqueId()==flit_num){
						print=true;	
					}
			}else if(flit_num<=0 && router_num>=0){
					if(r_id==router_num){
						print=true;	
					}
			}else if(flit_num>=0 && router_num>=0){
					if(r_id==router_num && t_flit->getUniqueId()==flit_num){
						print=true;	
					}
			}else{
				print=true;	
			}

			if(print){
				std::cout<<"@"<<curTick()<<" R"<<r_id <<"\t"<<stage 
										<<"\tflit"<<t_flit->getUniqueId() 
										<<" flitType "<<t_flit->get_type()
										<<"\tHop "<<t_flit->getHop()
															<<std::endl;	
			}
		}
	}
}
void
Router_d::print_route_time(flit_d* t_flit, std::string stage)
{	
int router_num;
int flit_num;
bool enable;

// SETTING
//-1 print all
router_num=-1;
flit_num=-1;
enable=false;

	if(enable){
		bool print;
		print=false;	
		int r_id;	
		r_id=this->get_id();
		if(t_flit!=NULL){
			if(router_num<=0 && flit_num>=0){
					//Print all routers onfly flit filter
					if(t_flit->getUniqueId()==flit_num){
						print=true;	
					}
			}else if(flit_num<=0 && router_num>=0){
					if(r_id==router_num){
						print=true;	
					}
			}else if(flit_num>=0 && router_num>=0){
					if(r_id==router_num && t_flit->getUniqueId()==flit_num){
						print=true;	
					}
			}else{
				print=true;	
			}

			if(print){
				std::cout<<(curTick()/1000)<<";"<<r_id
								<<";"<<stage 
									<<";"<<t_flit->getUniqueId()
												<<";"<<t_flit->getHop()
														<<std::endl;	
			}
		}
	}
}
bool
Router_d::get_m_wh_path_busy(int inport, int invc, int outport)
{
    return m_sw_alloc->get_wh_path_busy(inport, invc, outport); 
}
void
Router_d::set_m_wh_path_busy(bool set, int inport, int invc, int outport)
{
    m_sw_alloc->set_wh_path_busy(set, inport, invc, outport);
}

#if USE_LRC == 1
void 
Router_d::swarb_req_LRC() 
{ // schedule in the same cycle
	m_sw_alloc->scheduleEvent(0); 
} 
void 
Router_d::vcarb_req_LRC()
{// schedule in the same cycle
	m_vc_alloc->scheduleEvent(0); 
}
#endif


