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
#include <queue>

#include "base/cast.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_d.hh"
#include "mem/ruby/slicc_interface/NetworkMessage.hh"

RoutingUnit_d::RoutingUnit_d(Router_d *router)
{
    m_router = router;
    m_routing_table.clear();
    m_weight_table.clear();
}

void
RoutingUnit_d::addRoute(const NetDest& routing_table_entry)
{
    m_routing_table.push_back(routing_table_entry);
}

void
RoutingUnit_d::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

void
RoutingUnit_d::RC_stage(flit_d *t_flit, InputUnit_d *in_unit, int invc)
{
    int outport = routeCompute(t_flit);
	bool flag_vc_reused=false;
	bool flag_non_atomic_vc_used=false;

#if USE_NON_ATOMIC_VC_ALLOC==1
	#if USE_VC_REMAP ==1
		assert(false);
	#endif
		
	// variable to store the RC
	// maybe time ??
	// bool isThereMultiplePkt 1 per VC states if the vc is storing multiple
	// packets
	if(in_unit->get_vnetIdLastFlitTail(invc)==-1)
	{//the vc has been released thus store values here as a normal RC stage
		flag_non_atomic_vc_used=false;
		assert(in_unit->getOutportTempQueue(invc)->size()==0); //no stored outports
	}	
	else
	{//another pkt is already present in this buf, vnet assert checked in IU->wakeup
		flag_non_atomic_vc_used=true;
		std::queue<int>* temp_queue=in_unit->getOutportTempQueue(invc);
		assert(outport!=-1);
		temp_queue->push(outport);	
	}
#endif	

#if USE_WH_VC_SA==1 //for the buffer reuse
#if USE_VC_REUSE ==1 && USE_VC_REMAP==1	
	#if USE_NON_ATOMIC_VC_ALLOC==1
		assert(false);
	#endif

	if( in_unit->get_reused_remap_vc_used()[invc]==true )
	{
		flag_vc_reused=true; //check auxiliary info not the one in the packet
		assert(outport!=-1);
		in_unit->get_reused_remap_vc_outport()[invc]=outport;
		in_unit->get_reused_remap_vc_outport_rc_cycle()[invc]=m_router->curCycle();
		//return; // exit here do not compute vc allocation speculatively for now, since it can be useless and it is speculative
	}
#endif
#endif

//directly update route if both flags are false
if(flag_vc_reused==false && flag_non_atomic_vc_used==false)
	{
	#if USE_LRC==1    	
		in_unit->updateRoute(invc, outport, m_router->curCycle()-1);
    	t_flit->advance_stage(VA_, m_router->curCycle()-1);
	#else
    	in_unit->updateRoute(invc, outport, m_router->curCycle());
    	t_flit->advance_stage(VA_, m_router->curCycle());
	#endif
	}
	#if USE_LRC==0
	m_router->vcarb_req();
	#else
	//m_router->vcarb_req_LRC();
	// a single VA is requested just after the loop on ip::wakeup_bw_lrc() in SA
	#endif
}

int
RoutingUnit_d::routeCompute(flit_d *t_flit)
{
    MsgPtr msg_ptr = t_flit->get_msg_ptr();
    NetworkMessage* net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
    NetDest msg_destination = net_msg_ptr->getInternalDestination();

    int output_link = -1;
    int min_weight = INFINITE_;

    for (int link = 0; link < m_routing_table.size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) {
            if (m_weight_table[link] >= min_weight)
                continue;
            output_link = link;
            min_weight = m_weight_table[link];
        }
    }

    if (output_link == -1) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    return output_link;
}
