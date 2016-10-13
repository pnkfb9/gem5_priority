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

#include "base/stl_helpers.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"

#define PRIORITY_SCHED 1

using namespace std;
using m5::stl_helpers::deletePointers;

OutputUnit_d::OutputUnit_d(int id, Router_d *router)
    : Consumer(router)
{
    m_id = id;
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_out_buffer = new flitBuffer_d();
    #if PRIORITY_SCHED == 1
	m_last_max_priority_vc = -1;
	#endif
    for (int i = 0; i < m_num_vcs; i++) {
        OutVcState_d *vc=new OutVcState_d(i, m_router->get_net_ptr());
        vc->setState(IDLE_,router->curCycle());
        m_outvc_state.push_back(vc);
    }
    #if USE_VICHAR==1
		//init total credit
		availSlotPerVnet.resize(m_router->get_num_vnets());
		for(int i=0;i<m_router->get_num_vnets();i++)
			availSlotPerVnet.at(i)=m_router->getTotVicharSlotPerVnet();
	#endif
    this->setEventPrio(Event::NOC_OutputUnit_Pri);
}

OutputUnit_d::~OutputUnit_d()
{
    delete m_out_buffer;
    deletePointers(m_outvc_state);
}
#define OU 4
#define R0 1
void
OutputUnit_d::decrement_credit(int out_vc)
{


    m_outvc_state[out_vc]->decrement_credit();
    m_router->update_incredit(m_outvc_state[out_vc]->get_inport(),
                              m_outvc_state[out_vc]->get_invc(),
                              m_outvc_state[out_vc]->get_credit_count());

	#if USE_VICHAR==1
	decrAvailSlotPerVnet(out_vc/m_router->get_vc_per_vnet());
	assert(getAvailSlotPerVnet(out_vc/m_router->get_vc_per_vnet())>=0);
	#endif

}

void
OutputUnit_d::wakeup()
{
    Resynchronizer *r=m_credit_link->getReshnchronizer();
    if (m_credit_link->isReady(m_router->curCycle()) && r->isRequestAvailable()) {
        r->clearRequest();
        r->sendAcknowledge();
        flit_d *t_flit = m_credit_link->consumeLink();
        int out_vc = t_flit->get_vc();
        m_outvc_state[out_vc]->increment_credit();
	#if USE_VICHAR==1
		incrAvailSlotPerVnet(out_vc/m_router->get_vc_per_vnet());
	#endif
        m_router->update_incredit(m_outvc_state[out_vc]->get_inport(),
                                  m_outvc_state[out_vc]->get_invc(),
                                  m_outvc_state[out_vc]->get_credit_count());

#if USE_NON_ATOMIC_VC_ALLOC == 1
        if (t_flit->is_free_signal())
		{// TODO- requires VA to decrease the allocated pkts
			assert(	m_outvc_state[out_vc]->get_counter_pkt(m_router->curCycle())<m_router->get_tot_atomic_pkt() 
					&& m_outvc_state[out_vc]->get_counter_pkt(m_router->curCycle())>=0 );

			if( m_outvc_state[out_vc]->get_counter_pkt(m_router->curCycle())==(m_router->get_tot_atomic_pkt()-1) )
			{// a single packet was allocated on this buffer set it as idle from next cycle
				set_vc_state(IDLE_, out_vc, m_router->curCycle());
				m_outvc_state[out_vc]->set_busy_to_vnet_id(m_router->getVirtualNetworkProtocol()/*NOVNETID*/);
			}
			//else {// multiple pkts are allocated in this outvc do not idle it but update the cnt only TODO one cycle later}	
			m_outvc_state[out_vc]->inc_counter_pkt(m_router->curCycle()); //TODO add concept of time when inc and decr counter as in the is_vc_idle func
		}
/////////////////////////////////////////////////////
#else
	if (t_flit->is_free_signal())
		{
			set_vc_state(IDLE_, out_vc, m_router->curCycle());
			m_last_max_priority_vc = -1;
		}
#endif
	#if DEBUG_CREDIT_RTT == 1	
		// for rtt credit only no functional use
		if(m_router->get_id()==0 && m_id==3 && out_vc==0 && t_flit->is_free_signal() )
			std::cout<<"@"<<curTick()<<" R"<<m_router->get_id() <<" OP"<<m_id
							<<" outvc"<<out_vc<<" CREDIT GOT BACK"<<std::endl;	
	#endif	
        delete t_flit;
    }
    //Retry, you may have better luck next time
    if(r->isRequestPending())
    {
        r->advanceReceiveSide();
        scheduleEvent(1);
    }
}

flitBuffer_d*
OutputUnit_d::getOutQueue()
{
    return m_out_buffer;
}

void
OutputUnit_d::set_out_link(NetworkLink_d *link)
{
    m_out_link = link;
}

void
OutputUnit_d::set_credit_link(CreditLink_d *credit_link)
{
    m_credit_link = credit_link;
}

void
OutputUnit_d::update_vc(int vc, int in_port, int in_vc)
{
    //Called by VCallocator_d of the same router. As it does not cross clock
    //domain boundaries this is correct.

    m_outvc_state[vc]->setState(ACTIVE_, m_router->curCycle() + 1);
    m_outvc_state[vc]->set_inport(in_port);
    m_outvc_state[vc]->set_invc(in_vc);
    m_router->update_incredit(in_port, in_vc,
    m_outvc_state[vc]->get_credit_count());
}
