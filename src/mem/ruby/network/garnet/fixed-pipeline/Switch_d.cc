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
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Switch_d.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"

using m5::stl_helpers::deletePointers;

Switch_d::Switch_d(Router_d *router)
    : Consumer(router)
{
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_crossbar_activity = 0;
    m_crossbar_activity_resettable = 0;
    
    this->setEventPrio(Event::NOC_Switch_Pri);
}

Switch_d::~Switch_d()
{
    deletePointers(m_switch_buffer);
}

void
Switch_d::init()
{
    m_output_unit = m_router->get_outputUnit_ref();

    m_num_inports = m_router->get_num_inports();
    m_switch_buffer.resize(m_num_inports);
    for (int i = 0; i < m_num_inports; i++) {
        m_switch_buffer[i] = new flitBuffer_d();
    }
}

void
Switch_d::wakeup()
{
	//if(m_router->get_id()==3)
	//	std::cout<<"\t\t\t@"<<curTick()<<" wakeup_SW state: "<<getPowerState();

	if(getTickChangePowerState()==curTick())
    {
		bool oldPowerState= getPowerState();
		setPowerState( getNextPowerState() );
		setTickChangePowerState(0);

	    DPRINTF(RubyPowerGatingSW, "@%lld Router_%d->Switch WAKEUP_POWER_CHANGE_STATE  %d -> %d \n",
			m_router->curCycle(),m_router->get_id(),oldPowerState,getPowerState() );
	}
	if(!getPowerState())
	{//if not on reschedule else execute switch stage

		//if(m_router->get_id()==3)
		//	std::cout<<" RESCHEDULE"<<std::endl;
		DPRINTF(RubyPowerGatingSW, "@%lld Router_%d->Switch POWER_STATE: %d RESCHEDULE\n",
			m_router->curCycle(),m_router->get_id(),getPowerState());
	
		scheduleEvent(1);
	}
	else
	{//do normal stuff

		//if(m_router->get_id()==3)
		//	std::cout<<" DO NORMAL"<<std::endl;
	    DPRINTF(RubyPowerGatingSW, "@%lld Router_%d->Switch woke up: [STATE:%d]\n", 
	    				m_router->curCycle(),m_router->get_id(),getPowerState());
	    DPRINTF(RubyNetwork, "@%lld Router_%d->Switch woke up: [STATE:%d]\n", 
	    				m_router->curCycle(),m_router->get_id(),getPowerState());
	    for (int inport = 0; inport < m_num_inports; inport++) 
	    {
	        if (!m_switch_buffer[inport]->isReady(m_router->curCycle()))
	            continue;
			
	        flit_d *t_flit = m_switch_buffer[inport]->peekTopFlit();
			
		m_router->print_pipeline_state(t_flit, "ST");
		

	        if (t_flit->is_stage(ST_, m_router->curCycle())) 
            {
	            int outport = t_flit->get_outport();
				////if(m_router->get_id()==0)// && ( outport==4))
				////std::cout<<"@"<<curTick()<<" -- SWITCH -- R"<<m_router->get_id()	
				////		<<" ou"<<outport<<" vc"<<t_flit->get_vc()
				////		<<" HEAD?"<<((t_flit->get_type() == HEAD_)?1:0)
				////		<<" HEAD_TAIL_?"<<((t_flit->get_type() == HEAD_TAIL_)?1:0)
				////		<<" BODY_?"<<((t_flit->get_type() == BODY_)?1:0)
				////		<<" TAIL_?"<<((t_flit->get_type() == TAIL_)?1:0)
				////		<<" Consumer: "<<m_output_unit[outport]->getOutLink_d()->getLinkConsumer()->getObjectId()
				////		<<std::endl;
		
		    
			//SPECULATIVE Follow packet check Out	
			std::vector<OutputUnit_d *>& ou = m_router->get_outputUnit_ref();
			NetworkLink_d* link = ou[outport]->getOutLink_d();
			NetworkInterface_d* ni = dynamic_cast<NetworkInterface_d*>(link->getLinkConsumer());
			if(ni)
			{// compute packet router time 
				m_router->print_route_time(t_flit, "OUT_NETWORK");
				//Added Stop flit tick tracker latency
				t_flit->stopTracker(curTick());
				ni->getNetPtr()->increment_network_latency_tick(t_flit->getTickLatency(), t_flit->get_vnet());
			}

	            t_flit->advance_stage(LT_, m_router->curCycle());
	            t_flit->set_time(m_router->curCycle() + 1);
	
	            // This will take care of waking up the Network Link
	            m_output_unit[outport]->insert_flit(t_flit);
	            m_switch_buffer[inport]->getTopFlit();
	            m_crossbar_activity++;
                m_crossbar_activity_resettable++;
				// this is to track how many flits exit in a fixed time period
				m_router->incrementOutFlits();
            }
			else
				panic("Switch impossible to have a ready flit not in ST_ stage\n");
        }
        check_for_wakeup();
    }
}

void
Switch_d::check_for_wakeup()
{
    for (int inport = 0; inport < m_num_inports; inport++) {
        if (m_switch_buffer[inport]->isReadyForNext(m_router->curCycle())) {
            scheduleEvent(1);
            break;
        }
    }
}
