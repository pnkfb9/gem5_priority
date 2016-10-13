#include "VCallocator_VOQ_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"


#include "mem/ruby/slicc_interface/NetworkMessage.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"

#include "config/actual_voq_uarch.hh"

void
VCallocator_VOQ_d::select_outvc(int inport_iter, int invc_iter)
{

    int outport = m_input_unit[inport_iter]->get_route(invc_iter);
	int next_outport = m_input_unit[inport_iter]->get_next_route(invc_iter);
    // int vnet = get_vnet(invc_iter);
    // int outvc_base = vnet*m_vc_per_vnet;
    // int num_vcs_per_vnet = m_vc_per_vnet;


	int next_r_num_local_ports=0;
	int next_r_outports=0;
	std::vector<OutputUnit_d*>& outports = m_router->get_outputUnit_ref();
	//this is the next_router local components
	for(int i=0;i<outports.size();i++)
	{
		Consumer* o = outports.at(i)->getOutLink_d()->getLinkConsumer();
		if(o->getObjectId().find("Router")==std::string::npos)
		{// this is a local port
			next_r_num_local_ports++;
		}
		else
		{// this is a router
			ClockedObject *obj = outports.at(i)->getOutLink_d()->getLinkConsumer()->getClockedObject();
			Router_d* next_r =  dynamic_cast<Router_d*>(obj);
			assert(next_r);
			next_r_outports = next_r->get_num_outports();
		}
	}


	//int asked_vc=-1;

	if(next_outport==-1)
	{// use the std VA if this is the final hop
		baselineVA(inport_iter, invc_iter);
	}
	else
	{// there is a next hop ask for a VC based on this next_route
		std::string actual_voq_uarch = ACTUAL_VOQ_UARCH;
		if(actual_voq_uarch=="VOQ")
			VOQ(inport_iter,invc_iter, outport,next_outport, next_r_outports,next_r_num_local_ports);
		else if(actual_voq_uarch=="VOQ_DBBM")
			VOQ_DBBM(inport_iter,invc_iter,outport,next_outport,next_r_outports,next_r_num_local_ports);
		else if(actual_voq_uarch=="DBBM")
			DBBM(inport_iter, invc_iter, outport, next_outport, next_r_outports);
		else
			assert(false && "NO RECOGNIZED VOQ UARCH");
	}
}

void 
VCallocator_VOQ_d::baselineVA(int inport_iter, int invc_iter)
{
    
	int outport = m_input_unit[inport_iter]->get_route(invc_iter);
    int vnet = get_vnet(invc_iter);
    int outvc_base = vnet*m_vc_per_vnet;
    int num_vcs_per_vnet = m_vc_per_vnet;
	int outvc_offset = m_round_robin_invc[inport_iter][invc_iter];
	int asked_vc = -1;

    m_round_robin_invc[inport_iter][invc_iter]++;

    if (m_round_robin_invc[inport_iter][invc_iter] >= num_vcs_per_vnet)
        m_round_robin_invc[inport_iter][invc_iter] = 0;

	// It tries all vcs until 1 is free using a rr scheme
    for (int outvc_offset_iter = 0; outvc_offset_iter < num_vcs_per_vnet;
            											outvc_offset_iter++) 
	{
        outvc_offset++;
        if (outvc_offset >= num_vcs_per_vnet)
            outvc_offset = 0;
        int outvc = outvc_base + outvc_offset;
        if (m_output_unit[outport]->is_vc_idle(outvc, m_router->curCycle())) 
		{
			asked_vc = outvc;
            m_local_arbiter_activity[vnet]++;
            m_outvc_req[outport][outvc][inport_iter][invc_iter] = true;
            if (!m_outvc_is_req[outport][outvc])
                m_outvc_is_req[outport][outvc] = true;
			// debug hands//////
			asked_vc=outvc;
			DPRINTF(RubyVOQ_VA, "@%lld Router_%d->VA_ sel_outvc: %d outport: %d next_outport: NOT_SET \n",
								curTick(),m_router->get_id(),asked_vc-outvc_base,outport);
        	////////////////////
			return; // out vc acquired
        }
    }
	//no print if no channel is asked for
}

/* 1 vc per outport even for local one..huge amount of vcs is required */
void 
VCallocator_VOQ_d::baselineVOQ(int inport_iter, int invc_iter, int outport, int next_outport, int next_r_outports)
{
    int vnet = get_vnet(invc_iter);
    int outvc_base = vnet*m_vc_per_vnet;
	int asked_vc=-1;

	assert(m_router->get_vc_per_vnet() >= next_r_outports);
	int vc_VOQ = outvc_base + next_outport;
	if (m_output_unit[outport]->is_vc_idle(vc_VOQ, m_router->curCycle())) 
	{
        m_local_arbiter_activity[vnet]++;
        m_outvc_req[outport][vc_VOQ][inport_iter][invc_iter] = true;
        if (!m_outvc_is_req[outport][vc_VOQ])
            m_outvc_is_req[outport][vc_VOQ] = true;
		// debug hands//////
		asked_vc=vc_VOQ;
		DPRINTF(RubyVOQ_VA, "@%lld Router_%d->VA_baseline_VOQ sel_outvc: %d outport: %d next_outport: %d \n",
								curTick(),m_router->get_id(),asked_vc-outvc_base,outport,next_outport);
        ////////////////////
        return; // out vc vc_VOQ acquired
    }	

	//no print if no channel is asked for
}

/*This policy is an VOQ policy, using 1 vc depending of the outport. The
 * possible 0<= next_outport <= num_out_ports && -1 (if no next hop).

	next_outport=-1 this tile is the destination, the standard strategy is used
	next_outport=(local) vc0
	next_outport= <other-routers>  
*/
void
VCallocator_VOQ_d::VOQ(int inport_iter, int invc_iter, int outport, int next_outport, int next_r_outports, int next_r_num_local_ports)
{
	int vnet = get_vnet(invc_iter);
    int outvc_base = vnet*m_vc_per_vnet;
	int asked_vc=-1;
	int next_link_outports = next_r_outports - next_r_num_local_ports;

	// Test if vcs are enough to be one per outports in the next router + 1
	// for all the local in the next router
	assert(m_router->get_vc_per_vnet() >= next_link_outports+1);
	int vc_VOQ=outvc_base + 0;
	if(next_outport<next_r_num_local_ports)
	{// allocates on one common vc for all locals VC0
		if (m_output_unit[outport]->is_vc_idle(vc_VOQ, m_router->curCycle())) 
		{
            m_local_arbiter_activity[vnet]++;
            m_outvc_req[outport][vc_VOQ][inport_iter][invc_iter] = true;
            if (!m_outvc_is_req[outport][vc_VOQ])
                m_outvc_is_req[outport][vc_VOQ] = true;
			// debug hands//////
			asked_vc=0;
			DPRINTF(RubyVOQ_VA, "@%lld Router_%d->VA_VOQ sel_outvc: %d outport: %d next_outport: %d \n",
								curTick(),m_router->get_id(),(asked_vc-outvc_base),outport,next_outport);
            ////////////////////

            return; // out vc 0 acquired
        }

	}
	else
	{//ask for this vc next_outport-next_r_num_local_ports+1
		int vc_VOQ = outvc_base + next_outport-next_r_num_local_ports+1;
		if (m_output_unit[outport]->is_vc_idle(vc_VOQ, m_router->curCycle())) 
		{
            m_local_arbiter_activity[vnet]++;
            m_outvc_req[outport][vc_VOQ][inport_iter][invc_iter] = true;
            if (!m_outvc_is_req[outport][vc_VOQ])
                m_outvc_is_req[outport][vc_VOQ] = true;
			// debug hands//////
			asked_vc=vc_VOQ;
			DPRINTF(RubyVOQ_VA, "@%lld Router_%d->VA_VOQ sel_outvc(no_base): %d outport: %d next_outport: %d \n",
								curTick(),m_router->get_id(),(asked_vc-outvc_base),outport,next_outport);
            ////////////////////

            return; // out vc vc_VOQ acquired
        }		
	}
	//no print if no channel is asked for
}

/*This policy allocates vcs using the VOQ policy with constraints on resources.
 * In particular it allocates vcs based on the following rule: next_outport %num_vcs . 
 * This means we can experience some congestion, while we can work with a reduce
 * number of vcs
 */

void 
VCallocator_VOQ_d::VOQ_DBBM(int inport_iter, int invc_iter, int outport, int next_outport, int next_r_outports, int next_r_num_local_ports)
{
	int vnet = get_vnet(invc_iter);
    int outvc_base = vnet*m_vc_per_vnet;
	int asked_vc=-1;
	//int next_link_outports = next_r_outports - next_r_num_local_ports;

	int vc_DBBM_offset = next_outport % m_vc_per_vnet;
	
	int vc_DBBM = outvc_base + vc_DBBM_offset;
	
	if (m_output_unit[outport]->is_vc_idle(vc_DBBM, m_router->curCycle())) 
	{
        m_local_arbiter_activity[vnet]++;
        m_outvc_req[outport][vc_DBBM][inport_iter][invc_iter] = true;
        if (!m_outvc_is_req[outport][vc_DBBM])
            m_outvc_is_req[outport][vc_DBBM] = true;
		// debug hands//////
		asked_vc=vc_DBBM;
		DPRINTF(RubyVOQ_VA, "@%lld Router_%d->VA_VOQ_DBBM sel_outvc: %d outport: %d next_outport: %d \n",
								curTick(),m_router->get_id(),(asked_vc-outvc_base),outport,next_outport);
        ////////////////////

        return; // out vc vc_DBBM acquired
    }
	//no print if no channel is asked for
}

void 
VCallocator_VOQ_d::DBBM(int inport_iter, int invc_iter, int outport, int next_outport, int next_r_outports)
{
	int vnet = get_vnet(invc_iter);
    int outvc_base = vnet*m_vc_per_vnet;
	int asked_vc=-1;
	
	// get the destination
	flit_d* t_flit = m_input_unit[inport_iter]->peekTopFlit(invc_iter);
	MsgPtr msg_ptr = t_flit->get_msg_ptr();
    NetworkMessage* net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
    NetDest msg_destination = net_msg_ptr->getInternalDestination();

	std::vector<Set>& m_bits=msg_destination.getVettBits();
	int m_mach_type=-1;
	int m_tile=-1;
	for (int i = 0; i < m_bits.size(); i++) 
	{// machine types, i.e. L1, L2, MC
        for (int j = 0; j < m_bits[i].getSize(); j++) 
		{//tile number
            if((bool) m_bits[i].isElement(j))
			{
				// we do not support for  multicast or broadcast msg
				assert(m_mach_type==-1);
				m_mach_type=i;
				m_tile=j;
			}	
        }
    }

	// implementation of the DBBM strategy
	int vc_DBBM_offset = m_tile % m_vc_per_vnet;	
	int vc_DBBM = outvc_base + vc_DBBM_offset;
	if (m_output_unit[outport]->is_vc_idle(vc_DBBM, m_router->curCycle())) 
	{
        m_local_arbiter_activity[vnet]++;
        m_outvc_req[outport][vc_DBBM][inport_iter][invc_iter] = true;
        if (!m_outvc_is_req[outport][vc_DBBM])
            m_outvc_is_req[outport][vc_DBBM] = true;
		// debug hands//////
		asked_vc=vc_DBBM;
		std::string str;
		switch(m_mach_type)
		{
			case 0: str="L1"; break;
			case 1: str="L2"; break;
			case 2: str="MC"; break;
			default:
				assert(false && "We do not allow for L3 in this simulator");
		}
		DPRINTF(RubyVOQ_VA, "@%lld Router_%d->VA_DBBM sel_outvc: %d outport: %d dest: [%s %d] \n",
						curTick(),m_router->get_id(),(asked_vc-outvc_base),outport,str,m_tile);
        ////////////////////

        return; // out vc vc_DBBM acquired
    }

}





































//void
//VCallocator_VOQ_d::select_outvc(int inport_iter, int invc_iter)
//{
//
//    int outport = m_input_unit[inport_iter]->get_route(invc_iter);
//	int next_outport = m_input_unit[inport_iter]->get_next_route(invc_iter);
//    int vnet = get_vnet(invc_iter);
//    int outvc_base = vnet*m_vc_per_vnet;
//    int num_vcs_per_vnet = m_vc_per_vnet;
//
//
//	int next_r_num_local_ports=0;
//	int next_r_outports=0;
//	std::vector<OutputUnit_d*>& outports = m_router->get_outputUnit_ref();
//	//this is the next_router local components
//	for(int i=0;i<outports.size();i++)
//	{
//		Consumer* o = outports.at(i)->getOutLink_d()->getLinkConsumer();
//		if(o->getObjectId().find("Router")==std::string::npos)
//		{// this is a local port
//			next_r_num_local_ports++;
//		}
//		else
//		{// this is a router
//			ClockedObject *obj = outports.at(i)->getOutLink_d()->getLinkConsumer()->getClockedObject();
//			Router_d* next_r =  dynamic_cast<Router_d*>(obj);
//			assert(next_r);
//			next_r_outports = next_r->get_num_outports();
//		}
//	}
//
///*This policy is an VOQ policy, using 1 vc depending of the outport. The
// * possible 0<= next_outport <= num_out_ports && -1 (if no next hop).
//
//	next_outport=-1 this tile is the destination, the standard strategy is used
//	next_outport=(local) vc0
//	next_outport= <other-routers>  
//
//*/
//	int asked_vc=-1;
////	std::cout<<"\t- START -Router"<<m_router->get_id()
////					<<" selected_outvc: "<<asked_vc
////					<<" outport: " <<outport
////					<<" next_outport: "<<next_outport
////					<<std::endl;
//	if(next_outport==-1)
//	{// use the std VA if this is the final hop
//		
//	    int outvc_offset = m_round_robin_invc[inport_iter][invc_iter];
//	    m_round_robin_invc[inport_iter][invc_iter]++;
//	
//	    if (m_round_robin_invc[inport_iter][invc_iter] >= num_vcs_per_vnet)
//	        m_round_robin_invc[inport_iter][invc_iter] = 0;
//	
//	    for (int outvc_offset_iter = 0; outvc_offset_iter < num_vcs_per_vnet;
//	            											outvc_offset_iter++) 
//		{
//	        outvc_offset++;
//	        if (outvc_offset >= num_vcs_per_vnet)
//	            outvc_offset = 0;
//	        int outvc = outvc_base + outvc_offset;
//	        if (m_output_unit[outport]->is_vc_idle(outvc, m_router->curCycle())) {
//	            m_local_arbiter_activity[vnet]++;
//	            m_outvc_req[outport][outvc][inport_iter][invc_iter] = true;
//	            if (!m_outvc_is_req[outport][outvc])
//	                m_outvc_is_req[outport][outvc] = true;
//				
//				// debug hands//////
//				asked_vc=outvc;
//				std::cout<<"\t- END - Router"<<m_router->get_id()
//					<<" selected_outvc: "<<asked_vc
//					<<" outport: " <<outport
//					<<" next_outport: "<<next_outport
//					<<std::endl;
//
//	            ////////////////////
//				
//				return; // out vc acquired
//	        }
//	    }
//	}
////	else
////	{
////		std::cout<<"m_router->get_vc_per_vnet(): "<<m_router->get_vc_per_vnet()
////				<<"next_r_outports: "<<next_r_outports
////				<<std::endl;
////		assert(m_router->get_vc_per_vnet() >= next_r_outports);
////		int vc_VOQ = outvc_base + next_outport;
////		if (m_output_unit[outport]->is_vc_idle(vc_VOQ, m_router->curCycle())) 
////		{
////            m_local_arbiter_activity[vnet]++;
////            m_outvc_req[outport][vc_VOQ][inport_iter][invc_iter] = true;
////            if (!m_outvc_is_req[outport][vc_VOQ])
////                m_outvc_is_req[outport][vc_VOQ] = true;
////			// debug hands//////
////			asked_vc=vc_VOQ;
////			std::cout<<"\t- END - Router"<<m_router->get_id()
////				<<" selected_outvc: "<<asked_vc
////				<<" outport: " <<outport
////				<<" next_outport: "<<next_outport
////				<<std::endl;
////            ////////////////////
////
////            return; // out vc vc_VOQ acquired
////        }		
////	}
//
//	else
//	{// there is a next hop ask for a VC based on this next_route
//		int next_link_outports = next_r_outports - next_r_num_local_ports;
//
//		// Test if vcs are enough to be one per outports in the next router + 1
//		// for all the local in the next router
//		assert(m_router->get_vc_per_vnet() >= next_link_outports+1);
//		int vc_VOQ=outvc_base +0;
//		if(next_outport<next_r_num_local_ports)
//		{// allocates on one common vc for all locals VC0
//			if (m_output_unit[outport]->is_vc_idle(vc_VOQ, m_router->curCycle())) 
//			{
//	            m_local_arbiter_activity[vnet]++;
//	            m_outvc_req[outport][vc_VOQ][inport_iter][invc_iter] = true;
//	            if (!m_outvc_is_req[outport][vc_VOQ])
//	                m_outvc_is_req[outport][vc_VOQ] = true;
//				// debug hands//////
//				asked_vc=0;
//				std::cout<<"\t- END - Router"<<m_router->get_id()
//					<<" selected_outvc: "<<asked_vc
//					<<" outport: " <<outport
//					<<" next_outport: "<<next_outport
//					<<std::endl;
//	            ////////////////////
//
//	            return; // out vc 0 acquired
//	        }
//
//		}
//		else
//		{//ask for this vc next_outport-next_r_num_local_ports+1
//			int vc_VOQ = outvc_base + next_outport-next_r_num_local_ports+1;
//			if (m_output_unit[outport]->is_vc_idle(vc_VOQ, m_router->curCycle())) 
//			{
//	            m_local_arbiter_activity[vnet]++;
//	            m_outvc_req[outport][vc_VOQ][inport_iter][invc_iter] = true;
//	            if (!m_outvc_is_req[outport][vc_VOQ])
//	                m_outvc_is_req[outport][vc_VOQ] = true;
//				// debug hands//////
//				asked_vc=vc_VOQ;
//				std::cout<<"\t- END - Router"<<m_router->get_id()
//					<<" selected_outvc: "<<asked_vc
//					<<" outport: " <<outport
//					<<" next_outport: "<<next_outport
//					<<std::endl;
//	            ////////////////////
//
//	            return; // out vc vc_VOQ acquired
//	        }		
//		}
//		
//	}
//	//std::cout<<"\t- END -Router"<<m_router->get_id()
//	//				<<" selected_outvc: "<<asked_vc
//	//				<<" outport: " <<outport
//	//				<<" next_outport: "<<next_outport
//	//				<<std::endl;
//}

































