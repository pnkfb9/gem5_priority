#include <iostream>
#include <cassert>
#include <string>
#include <queue>

#include "RoutingUnit_ADAPTIVE_d.hh"

#include "base/cast.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_d.hh"
#include "mem/ruby/slicc_interface/NetworkMessage.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"

#include "config/actual_adaptive_routing.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_ADAPTIVE_d.hh"

int RoutingUnit_ADAPTIVE_d::routeComputeAdaptive(flit_d *t_flit)
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

int RoutingUnit_ADAPTIVE_d::routeComputeDeterministic(flit_d *t_flit)
{ 
// the OP selection when pkt is moved to escape path is computed not only
// considering the escape path routing func but the pkt.dest and pkt actual
// position and "orientation".
	if(t_flit->getIsAdaptive()==0)
		return routeComputeOld(t_flit);

	MsgPtr msg_ptr = t_flit->get_msg_ptr();
    NetworkMessage* net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
    NetDest msg_destination = net_msg_ptr->getInternalDestination();
	std::vector<NodeID> dest = msg_destination.getAllDest();

	//std::cout<<"R"<<m_router->get_id()<<" Dest:";
	// cur router info
	int curCol=m_router->get_id()%m_router->get_net_ptr()->get_num_cols();
	int curRow=m_router->get_id()/m_router->get_net_ptr()->get_num_cols();

    int output_link = -1;

    for (int link = 0; link < m_routing_table.size(); link++) 
	{
        if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) 
		{
			// get x,y pos for the next hop
			InputUnit_d* maybeOutRouter = dynamic_cast<InputUnit_d*>
				(m_router->get_outputUnit_ref().at(link)->getOutLink_d()->getLinkConsumer());
			int next_hop_col=-1;
			int next_hop_row=-1;
			int rId=-1;	
			if(maybeOutRouter!=NULL)
			{
				//prevRid= (maybeInRouter->get_router())->get_id();
				rId = maybeOutRouter->get_router()->get_id();
				assert(rId!=-1);
				next_hop_col = rId%m_router->get_net_ptr()->get_num_cols();
				next_hop_row = rId/m_router->get_net_ptr()->get_num_cols();
			}
			else
			{
				output_link = link;	// this is a NIC
				break;
			}
			// check conditions with the used routing function for the escape
			// path VCs
		//#ifdef ESCAPE_ROUTING_FUNC==0 //XY
			if(next_hop_col!=curCol)
			{
				output_link = link;	// this is a DOR path
				break;
			}
			else
			{
				output_link = link;	// this is a path when Y dim is remained only.
			}
		//#endif		
        }
    }

    if (output_link == -1) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    return output_link;

}

// XY deterministic implementation - NOTE it depends on the weights assigned in
// $(Topology).py
int RoutingUnit_ADAPTIVE_d::routeComputeOld(flit_d *t_flit)
{
	MsgPtr msg_ptr = t_flit->get_msg_ptr();
    NetworkMessage* net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
    NetDest msg_destination = net_msg_ptr->getInternalDestination();

    int output_link = -1;

    for (int link = 0; link < m_routing_table.size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) {
            //if (m_weight_table[link] >= min_weight)
             //   continue;
            output_link = link;
			break;
            //min_weight = m_weight_table[link];
        }
    }

    if (output_link == -1) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    return output_link;

}

// checks all the outlinks looking for an available one, i.e. one outport with a
// free vc. Otherwise return -1 and the packet is routed on one deterministic path
int 
RoutingUnit_ADAPTIVE_d::checkAdaptiveOutputLinkAvailable(flit_d* t_flit, InputUnit_d *in_unit, int invc)
{
	MsgPtr msg_ptr = t_flit->get_msg_ptr();
    NetworkMessage* net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
    NetDest msg_destination = net_msg_ptr->getInternalDestination();
	
	int output_link=-1;
	int vnet = t_flit->get_vnet();
    int outvc_base = vnet*m_router->get_vc_per_vnet();

	int num_escape_path_vcs=(m_router->get_net_ptr())->getAdaptiveRoutingNumEscapePaths();
	int num_vcs_adaptive_per_vnet=m_router->get_vc_per_vnet()-num_escape_path_vcs;
	std::vector<OutputUnit_d *>& m_output_unit=m_router->get_outputUnit_ref();
	
	for (int link = 0; link < m_routing_table.size(); link++) 
	{
    	if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) 
		{
			//check if on this port a vc is available
    		for (int outvc_iter=outvc_base; outvc_iter < outvc_base+num_vcs_adaptive_per_vnet; outvc_iter++) 
			{
				if (m_output_unit[link]->is_vc_idle(outvc_iter, m_router->curCycle())) 
				{
    				output_link = link;
					break;
				}
			}
			if(output_link!=-1)
				break;
		}
    }
	
	#if USE_NON_ATOMIC_VC_ALLOC==1
	//check if a NonAtomicVCAllocation (NAVCA) VC is available
	if(output_link==-1)	
	{
		for (int link = 0; link < m_routing_table.size(); link++) 
		{
	    	if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) 
			{
				//check if on this port a vc is available
	    		for (int outvc_iter=outvc_base; outvc_iter < outvc_base+num_vcs_adaptive_per_vnet; outvc_iter++) 
				{
					
					if( testNAVCA(link,in_unit,invc,outvc_iter,t_flit) )
					{
	    				output_link = link;
						break;
					}
				}
				if(output_link!=-1)
					break;
			}
	    }
	}
	#endif
	//std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()<<" output_link"<<output_link<<std::endl;
	return output_link;
}

// checks all the outlinks, using a next hop less loaded inport, looking
// for one  one otherwise return -1 and the packet is routed on the
// deterministic path
int 
RoutingUnit_ADAPTIVE_d::checkLessLoadAdaptiveOutputLinkAvailable(flit_d* t_flit, InputUnit_d *in_unit, int invc)
{
	MsgPtr msg_ptr = t_flit->get_msg_ptr();
    NetworkMessage* net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
    NetDest msg_destination = net_msg_ptr->getInternalDestination();
	
	int output_link=-1;
	int output_link_freevc=0;
	int vnet = t_flit->get_vnet();
    int outvc_base = vnet*m_router->get_vc_per_vnet();

	int num_escape_path_vcs=(m_router->get_net_ptr())->getAdaptiveRoutingNumEscapePaths();
	int num_vcs_adaptive_per_vnet=m_router->get_vc_per_vnet()-num_escape_path_vcs;
	std::vector<OutputUnit_d *>& m_output_unit=m_router->get_outputUnit_ref();
	
	for (int link = 0; link < m_routing_table.size(); link++) 
	{
    	if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) 
		{
			int tmp_freevc=0;
			//check if on this port a vc is available
    		for (int outvc_iter=outvc_base; outvc_iter < outvc_base+num_vcs_adaptive_per_vnet; outvc_iter++) 
			{
				if (m_output_unit[link]->is_vc_idle(outvc_iter, m_router->curCycle())) 
				{
					tmp_freevc++;
				}
				#if USE_NON_ATOMIC_VC_ALLOC==1
				if( testNAVCA(link,in_unit,invc,outvc_iter,t_flit) )
				{
					tmp_freevc++;
				}
				#endif
			}
			if(tmp_freevc>output_link_freevc )
			{
				output_link = link;
				output_link_freevc=tmp_freevc;
			}
		}
    }
	//std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()<<" output_link"<<output_link<<std::endl;
	return output_link;
}

// checks all the outlinks looking for an available one, i.e. one outport with a
// free vc. Otherwise return -1 and the packet is routed on one deterministic path
int 
RoutingUnit_ADAPTIVE_d::checkFreqAdaptiveOutputLinkAvailable(flit_d* t_flit, InputUnit_d *in_unit, int invc)
{
	MsgPtr msg_ptr = t_flit->get_msg_ptr();
    NetworkMessage* net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
    NetDest msg_destination = net_msg_ptr->getInternalDestination();
	
	int output_link=-1;
	long output_link_freq=0;
	int vnet = t_flit->get_vnet();
    int outvc_base = vnet*m_router->get_vc_per_vnet();

	int num_escape_path_vcs=(m_router->get_net_ptr())->getAdaptiveRoutingNumEscapePaths();
	int num_vcs_adaptive_per_vnet=m_router->get_vc_per_vnet()-num_escape_path_vcs;
	std::vector<OutputUnit_d *>& m_output_unit=m_router->get_outputUnit_ref();
	
	for (int link = 0; link < m_routing_table.size(); link++) 
	{
    	if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) 
		{
			//check if on this port a vc is available
    		for (int outvc_iter=outvc_base; outvc_iter < outvc_base+num_vcs_adaptive_per_vnet; outvc_iter++) 
			{
				//std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()	<<" vc_iter:"<<outvc_iter<<std::endl;
				if (m_output_unit[link]->is_vc_idle(outvc_iter, m_router->curCycle())) 
				{
					InputUnit_d *tmp_inputunit=dynamic_cast<InputUnit_d*>(m_output_unit[link]->getOutLink_d()->getLinkConsumer());

					if(tmp_inputunit!=NULL)
					{
						if((tmp_inputunit->get_router())->get_frequency()>output_link_freq)
						{
							output_link = link;	
							output_link_freq=(tmp_inputunit->get_router())->get_frequency();
						}
					}
					NetworkInterface_d *tmp_niunit=dynamic_cast<NetworkInterface_d*>(m_output_unit[link]->getOutLink_d()->getLinkConsumer());
					if(tmp_niunit!=NULL)
					{// if next link is towards NI I dont ask for freq
						output_link = link;	
					}
				}
			}
		}
    }
	
#if USE_NON_ATOMIC_VC_ALLOC==1
	if(output_link==-1)
		for (int link = 0; link < m_routing_table.size(); link++) 
		{
	    	if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) 
			{
				//check if on this port a vc is available
	    		for (int outvc_iter=outvc_base; outvc_iter < outvc_base+num_vcs_adaptive_per_vnet; outvc_iter++) 
				{
					if ( testNAVCA(link,in_unit,invc,outvc_iter,t_flit) ) 
					{
						InputUnit_d *tmp_inputunit=dynamic_cast<InputUnit_d*>(m_output_unit[link]->getOutLink_d()->getLinkConsumer());
	
						if(tmp_inputunit!=NULL)
						{
							if((tmp_inputunit->get_router())->get_frequency()>output_link_freq)
							{
								output_link = link;	
								output_link_freq=(tmp_inputunit->get_router())->get_frequency();
							}
						}
						NetworkInterface_d *tmp_niunit=dynamic_cast<NetworkInterface_d*>(m_output_unit[link]->getOutLink_d()->getLinkConsumer());
						if(tmp_niunit!=NULL)
						{// if next link is towards NI I dont ask for freq
							output_link = link;	
						}
					}
				}
			}
	    }
#endif
	//std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()<<" output_link"<<output_link<<std::endl;
	return output_link;
}


/* this is the main function required by the router. we mainly modify the
 * behavior for inside this function */
void
RoutingUnit_ADAPTIVE_d::RC_stage(flit_d *t_flit, InputUnit_d *in_unit, int invc)
{
	int output_link=-1;

	bool flag_vc_reused=false;
	bool flag_non_atomic_vc_used=false;

	int num_escape_vcs = m_router->get_net_ptr()->getAdaptiveRoutingNumEscapePaths();
	int num_vc_per_vnet = in_unit->get_num_vc_per_vnet();

#if FADP_BACK_DET_TO_FADP_VC == 0	
	if( (invc % num_vc_per_vnet) <  (num_vc_per_vnet - num_escape_vcs) )
#endif
	{	// if we are in an adaptive vc a new pkt tries to route adaptively

		//select the adaptive function
		std::string actual_adaptive_routing=ACTUAL_ADAPTIVE_ROUTING;
		if(actual_adaptive_routing=="ADAPTIVE_SIMPLE")
		{
			//std::cout<<"ADAPTIVE_SIMPLE"<<std::endl;
			output_link=checkAdaptiveOutputLinkAvailable(t_flit,in_unit,invc);
		}
		else if	(actual_adaptive_routing=="ADAPTIVE_LESS_LOAD_ONE_HOP")		
		{
			//std::cout<<"ADAPTIVE_LESS_LOAD_ONE_HOP"<<std::endl;
			output_link=checkLessLoadAdaptiveOutputLinkAvailable(t_flit,in_unit,invc);
		}
		else if (actual_adaptive_routing=="ADAPTIVE_HIGH_FREQ_ONE_HOP")
		{
			//std::cout<<"ADAPTIVE_HIGH_FREQ_ONE_HOP"<<std::endl;
			output_link=checkFreqAdaptiveOutputLinkAvailable(t_flit,in_unit,invc);
		}
		else
			assert(false && "\nTHE REQUIRED ADAPTIVE ROUTING IS NOT IMPLEMENTED\n");
#if FADP_BACK_DET_TO_FADP_VC == 0
		if(output_link==-1)
		{// FADP OP not found move to DET path update info in the vc and the flit
			//	static long c=0;
			//	c++;
			//	std::cout<<c<<std::endl;
				//assert("why use a deterministic vc?\n"&&false);
				t_flit->setIsAdaptive(2/*new_value*/);
		}
#else
		if(output_link==-1)
		{// FADP OP not found move to DET path update info in the vc and the flit
			if( (invc%num_vc_per_vnet) <  (num_vc_per_vnet-num_escape_vcs) )
			{// FADP->DET
				t_flit->setIsAdaptive(2/*new_value*/);
			}
			else //it was in the escape path trying to go back to FADP vcs, but fails
			{// DET->DET
				assert(t_flit->getIsAdaptive()==0);
				t_flit->setIsAdaptive(0/*confirm old value new_value*/);	
			}
		}
		else
		{// found an FADP OP thus check if it is a packet to move back to FADP vcs
			if( (invc%num_vc_per_vnet) <  (num_vc_per_vnet-num_escape_vcs) )
			{
				assert(t_flit->getIsAdaptive()==1);
				t_flit->setIsAdaptive(1/*new_value*/);
			}
			else
			{
				assert(t_flit->getIsAdaptive()==0);
				t_flit->setIsAdaptive(3/*new_value*/);
			}
		}
#endif

	}

	if(output_link==-1)
	{// compute DET path
		//std::cout<<"look for deterministic path"<<std::endl;
		//assert(false);
		assert("second why use a deterministic vc?\n");
		output_link = routeComputeDeterministic(t_flit);
	}

	//I've got an output link now I can proceed with atomic allocation or buffer reuse

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
			assert(output_link!=-1);
			temp_queue->push(output_link);	
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
			assert(output_link!=-1);
			in_unit->get_reused_remap_vc_outport()[invc]=output_link;
			in_unit->get_reused_remap_vc_outport_rc_cycle()[invc]=m_router->curCycle();
			//return; // exit here do not compute vc allocation speculatively for now, since it can be useless and it is speculative
		}
	#endif
	#endif

	assert(output_link!=-1);
	if(flag_vc_reused==false && flag_non_atomic_vc_used==false)
	{
		in_unit->updateRoute(invc, output_link, m_router->curCycle());
		t_flit->advance_stage(VA_, m_router->curCycle());
	}
    m_router->vcarb_req();
}


bool
RoutingUnit_ADAPTIVE_d::testNAVCA(int outport, InputUnit_d* inport,int invc_iter, int outvc, flit_d* temp_flit)
{
//		NON_ATOMIC_VC_ALLOC
#if USE_NON_ATOMIC_VC_ALLOC==1
	std::vector<OutputUnit_d *>& m_output_unit = m_router->get_outputUnit_ref();
	
	Consumer *maybeNiface = m_output_unit[outport]->getOutLink_d()->getLinkConsumer();
	NetworkInterface_d *niface=dynamic_cast<NetworkInterface_d*>(maybeNiface);

	if(niface==NULL)
	{
		assert(temp_flit!=NULL);
		int vnet_flit = temp_flit->get_vnet();
	#if FULLY_ADP_TPDS_ROUTING==1
		 //Using new TDPS paper condition: use non atomic vc allocation only if the buffer have enought credit to store all the packet
     	if( ! m_output_unit[outport]->is_vc_idle(outvc, m_router->curCycle()) 
				&& vnet_flit== m_output_unit[outport]->get_vc_busy_to_vnet_id(outvc)
         && m_output_unit[outport]->get_credit_cnt(outvc)>= m_router->getVCallocatorUnit()->getPckSizeFromVnet(vnet_flit,temp_flit) )
     #else 
		 //Using our more relaxing condition: use non atomic vc allocation also if there isn't enought space in the input buffer but the Turn is allowed and there are at least one credit
		 //I can use non atomic vc allocation also for one flit packet(HEAD and HEADTAIL)
			if( ! m_output_unit[outport]->is_vc_idle(outvc, m_router->curCycle()) 
				&& vnet_flit== m_output_unit[outport]->get_vc_busy_to_vnet_id(outvc)
				  && m_output_unit[outport]->get_credit_cnt(outvc) >= 
				    (m_router->getVCallocatorUnit()->getPckSizeFromVnet(vnet_flit, temp_flit )- inport->getBufferSize(invc_iter))
				      && m_output_unit[outport]->get_credit_cnt(outvc)>=1
				        && (m_router->getVCallocatorUnit()->isTurnAllowed(inport->getID(), outport) == true || temp_flit->get_type() == HEAD_TAIL_))			
	#endif
			{
				return true;
			}//if(niface) non_atomic_vc_alloc does not support NIC for now
	}
#endif
	return false;
}

