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

#include "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_d.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"

#define NON_ATOMIC_VC_ALLOC_FULLY_ADP_ROUTING 0


VCallocator_d::VCallocator_d(Router_d *router)
    : Consumer(router)
{
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_vc_per_vnet = m_router->get_vc_per_vnet();

    m_local_arbiter_activity.resize(m_num_vcs/m_vc_per_vnet);
    m_global_arbiter_activity.resize(m_num_vcs/m_vc_per_vnet);
    m_local_arbiter_activity_resettable.resize(m_num_vcs/m_vc_per_vnet);
    m_global_arbiter_activity_resettable.resize(m_num_vcs/m_vc_per_vnet);
    for (int i = 0; i < m_local_arbiter_activity.size(); i++) {
        m_local_arbiter_activity[i] = 0;
        m_global_arbiter_activity[i] = 0;
        m_local_arbiter_activity_resettable[i] = 0;
        m_global_arbiter_activity_resettable[i] = 0;
    }
    
    this->setEventPrio(Event::NOC_VCAllocator_Pri);
}
//FIXME REMOVE
int 
VCallocator_d::getPckSizeFromVnet(int vnet, flit_d *t_flit){
	int pktSize = -1;
	if(vnet==0) pktSize = 1;
	if(vnet==1 && t_flit->get_type() == HEAD_ ) pktSize = 9;
	if(vnet==1 && t_flit->get_type() == HEAD_TAIL_) pktSize = 1;
	if(vnet==2) pktSize = 1;
	return pktSize;
}
void
VCallocator_d::init()
{
    m_input_unit = m_router->get_inputUnit_ref();
    m_output_unit = m_router->get_outputUnit_ref();

    m_num_inports = m_router->get_num_inports();
    m_num_outports = m_router->get_num_outports();
    m_round_robin_invc.resize(m_num_inports);
    m_round_robin_outvc.resize(m_num_outports);
    m_outvc_req.resize(m_num_outports);
    m_outvc_is_req.resize(m_num_outports);

    for (int i = 0; i < m_num_inports; i++) {
        m_round_robin_invc[i].resize(m_num_vcs);

        for (int j = 0; j < m_num_vcs; j++) {
            m_round_robin_invc[i][j] = 0;
        }
    }

    for (int i = 0; i < m_num_outports; i++) {
        m_round_robin_outvc[i].resize(m_num_vcs);
        m_outvc_req[i].resize(m_num_vcs);
        m_outvc_is_req[i].resize(m_num_vcs);

        for (int j = 0; j < m_num_vcs; j++) {
            m_round_robin_outvc[i][j].first = 0;
            m_round_robin_outvc[i][j].second = 0;
            m_outvc_is_req[i][j] = false;

            m_outvc_req[i][j].resize(m_num_inports);

            for (int k = 0; k < m_num_inports; k++) {
                m_outvc_req[i][j][k].resize(m_num_vcs);
                for (int l = 0; l < m_num_vcs; l++) {
                    m_outvc_req[i][j][k][l] = false;
                }
            }
        }
    }
	// for vnet reuse and spurious vnet optimizations
	assert(m_router!=NULL);
	assert(m_router->get_net_ptr()!=NULL);
	m_virtual_networks_protocol=m_router->get_net_ptr()->getNumVnetProtocol();
	m_virtual_networks_spurious=m_router->get_net_ptr()->getNumVnetSpurious();
	
	vnet_in_this_vc_outport.resize(m_num_outports);
	for(int j=0;j<m_num_outports;j++)
	{
		vnet_in_this_vc_outport[j].resize(m_num_vcs);
		for(int i=0;i<vnet_in_this_vc_outport[j].size();i++)
			vnet_in_this_vc_outport[j][i]=-5;
	}

	#if USE_SPECULATIVE_VA_SA==1
             spec_SWallocator_startup();	
	#endif
}

void
VCallocator_d::clear_request_vector()
{
    for (int i = 0; i < m_num_outports; i++) {
        for (int j = 0; j < m_num_vcs; j++) {
            if (!m_outvc_is_req[i][j])
                continue;
            m_outvc_is_req[i][j] = false;
            for (int k = 0; k < m_num_inports; k++) {
                for (int l = 0; l < m_num_vcs; l++) {
                    m_outvc_req[i][j][k][l] = false;
                }
            }
        }
    }
}

void
VCallocator_d::wakeup()
{
#if USE_LRC==1
	if(m_router->calledVAthisCycle==curTick())
		return; //already called this cycle do not call it again
	else if(m_router->calledVAthisCycle>curTick())
		assert(false);
#endif

#if USE_SPECULATIVE_VA_SA==1
	//spec_SWallocator_setup();	
#endif

    arbitrate_invcs(); // First stage of allocation
    arbitrate_outvcs(); // Second stage of allocation

    clear_request_vector();
	
#if USE_SPECULATIVE_VA_SA==1
	spec_SWallocator_clear_request_vector();	
#endif

    check_for_wakeup();
}

bool
VCallocator_d::is_invc_candidate(int inport_iter, int invc_iter)
{
    int outport = m_input_unit[inport_iter]->get_route(invc_iter);
    int vnet = get_vnet(invc_iter);
    int t_enqueue_time = m_input_unit[inport_iter]->get_enqueue_time(invc_iter);

    int invc_base = vnet*m_vc_per_vnet;

    if ((m_router->get_net_ptr())->isVNetOrdered(vnet)) 
	{
        for (int vc_offset = 0; vc_offset < m_vc_per_vnet; vc_offset++) 
		{
            int temp_vc = invc_base + vc_offset;
			#if USE_SPECULATIVE_VA_SA==1
			if ( m_input_unit[inport_iter]->need_stage(temp_vc, VC_SP_, VA_, m_router->curCycle()) //Changed only VC_AB_ state in speculative state VC_SP_ 
				&& (m_input_unit[inport_iter]->get_route(temp_vc) == outport) 
				&& (m_input_unit[inport_iter]->get_enqueue_time(temp_vc) < t_enqueue_time)	) 
			#else
			if (m_input_unit[inport_iter]->need_stage(temp_vc, VC_AB_, VA_, m_router->curCycle()) 
				&& (m_input_unit[inport_iter]->get_route(temp_vc) == outport) 
				&& (m_input_unit[inport_iter]->get_enqueue_time(temp_vc) < t_enqueue_time)	) 
			#endif
			{
			return false;
			}
		}
        
    }
    return true;
}

void
VCallocator_d::select_outvc(int inport_iter, int invc_iter)
{

    int outport = m_input_unit[inport_iter]->get_route(invc_iter);
	assert(outport>=0&& outport<m_num_outports);

	int vnet=get_vnet(invc_iter);
    int outvc_base = vnet*m_vc_per_vnet;
    int num_vcs_to_analyze = m_vc_per_vnet;

	//#if USE_WH_VC_SA==1
	#if USE_VNET_REUSE==1

	//check if the output is NOT towards a NIC. If so all vcs can be allocated
	//on the requested outport	limited to the deadlock property. al most
	//num_vc_per_vnet can be allocated
	
	//NetworkLink_d* link = m_output_unit[outport]->getOutLink_d();
	//NetworkInterface_d* ni = dynamic_cast<NetworkInterface_d*>(link->getLinkConsumer());
	//if(ni)
	//{
	//	vnet=m_input_unit[inport_iter]->get_real_vnet_used(invc_iter);    
	//	assert(vnet>=0&&vnet<m_num_vcs/m_vc_per_vnet && "VA impossible VNET out of bound");
	//	outvc_base = vnet*m_vc_per_vnet;
    //	num_vcs_to_analyze = m_vc_per_vnet;
	//}
	//else
	//{
		#if USE_APNEA_BASE==1		
		// apnea modifies the way the vcs are allocated and the visible subset
		// available to the upstream router VA stage
		outvc_base= m_router->getInitVcOp(outport);
		num_vcs_to_analyze= m_router->getFinalVcOp(outport);
		#else		
		outvc_base=0; // the offset_vc will be added later
		num_vcs_to_analyze=m_num_vcs;
		#endif
	//}
	#endif	
	//#endif
	if(vnet==-1|| vnet==-2)
		std::cout<<"#########################"<<vnet<<std::endl;	
	assert(vnet!=-1 && vnet!=-2);
	#if USE_APNEA_BASE == 1	
	int outvc_offset = 0;
	for (int outvc_offset_iter = 0; outvc_offset_iter < num_vcs_to_analyze; outvc_offset_iter++) 
	{
        int outvc = outvc_base + outvc_offset;
		assert(num_vcs_to_analyze==m_router->getFinalVcOp(outport));
		assert(outvc_base==m_router->getInitVcOp(outport));

        outvc_offset++;
        if (outvc_offset >= num_vcs_to_analyze)
            outvc_offset = 0;
	#else
    	int outvc_offset = m_round_robin_invc[inport_iter][invc_iter];
	for (int outvc_offset_iter = 0; outvc_offset_iter < num_vcs_to_analyze; outvc_offset_iter++) 
	{
        outvc_offset++;
        if (outvc_offset >= num_vcs_to_analyze)
            outvc_offset = 0;
        int outvc = outvc_base + outvc_offset;
	#endif

	#if USE_VICHAR==1
		//allocate a vc if there is at least a slot
		if(!(m_output_unit[outport]->getAvailSlotPerVnet(outvc/m_vc_per_vnet)>0))
			continue;
	#endif


        if (m_output_unit[outport]->is_vc_idle(outvc, m_router->curCycle())) 
		{

		#if USE_APNEA_BASE==1	
			assert(outvc>=m_router->getInitVcOp(outport) &&outvc<m_router->getFinalVcOp(outport));
		#endif
            m_local_arbiter_activity[vnet]++;
            m_local_arbiter_activity_resettable[vnet]++;
            m_outvc_req[outport][outvc][inport_iter][invc_iter] = true;
            if (!m_outvc_is_req[outport][outvc])
                m_outvc_is_req[outport][outvc] = true;
		
			//Speculative statistics
			if(m_outvc_is_req[outport][outvc] == true){
				if( m_input_unit[inport_iter]->peekTopFlit(invc_iter)->get_type() == HEAD_TAIL_ ||
						m_input_unit[inport_iter]->peekTopFlit(invc_iter)->get_type() == HEAD_ ){
						    Router_d::incGlobalCountStatic();
						    m_router->incLocalCount();
				//	std::cout<<"[INC]Globale ["<<Router_d::va_global_arbit_count<<"] Router = "<<m_router->get_id()<<" Locale ["<<m_router->getLocalCount()<<"]"<<std::endl;
				}
			}
			
			m_round_robin_invc[inport_iter][invc_iter]=outvc_offset;
			
            return; // out vc acquired
        }
		////////////////////////////////////////////////////////////////////////////////
//		NON_ATOMIC_VC_ALLOC
	#if USE_NON_ATOMIC_VC_ALLOC==1
		Consumer *maybeNiface = m_output_unit[outport]->getOutLink_d()->getLinkConsumer();
		NetworkInterface_d *niface=dynamic_cast<NetworkInterface_d*>(maybeNiface);
		if(niface==NULL)
		{
			int vnet_flit = m_input_unit[inport_iter]->peekTopFlit(invc_iter)->get_vnet();
			if( ! m_output_unit[outport]->is_vc_idle(outvc, m_router->curCycle()) 
				&& vnet_flit== m_output_unit[outport]->get_vc_busy_to_vnet_id(outvc) )
			{// VA_IP non atomic allocation is possible try to reserve
		
				m_local_arbiter_activity[vnet]++;
			    m_local_arbiter_activity_resettable[vnet]++;
			    m_outvc_req[outport][outvc][inport_iter][invc_iter] = true;
			    if (!m_outvc_is_req[outport][outvc])
					m_outvc_is_req[outport][outvc] = true;
  		
  			//Speculative statistics
				if(m_outvc_is_req[outport][outvc] == true){
					if( m_input_unit[inport_iter]->peekTopFlit(invc_iter)->get_type() == HEAD_TAIL_ ||
							m_input_unit[inport_iter]->peekTopFlit(invc_iter)->get_type() == HEAD_ )
					{
							    Router_d::incGlobalCountStatic();
							    m_router->incLocalCount();
  				//	std::cout<<"[INC]Globale ["<<Router_d::va_global_arbit_count<<"] Router = "<<m_router->get_id()<<" Locale ["<<m_router->getLocalCount()<<"]"<<std::endl;
					}
				}

				m_round_robin_invc[inport_iter][invc_iter]=outvc_offset; //
    	    	return; // non atomic out vc acquired
			}
		}//if(niface) non_atomic_vc_alloc does not support NIC for now
	#endif
////////////////////////////////////////////////////////////////////////////////
    }
	//assert(outvc_offset_iter==num_vcs_to_analyze);
}
void
VCallocator_d::arbitrate_invcs()
{
	
    for (int inport_iter = 0; inport_iter < m_num_inports; inport_iter++) 
	{
        for (int invc_iter = 0; invc_iter < m_num_vcs; invc_iter++) 
		{
            if (!((m_router->get_net_ptr())->validVirtualNetwork(get_vnet(invc_iter))))
                continue;
			//#if USE_APNEA_BASE==1				
			//	if( (invc_iter<m_router->getInitVcIp(inport_iter) || invc_iter>m_router->getFinalVcIp(inport_iter)) )
			//		continue;
			//#endif

	
			//check if buf reused
			bool flag_vc_reused=false;
		
			#if USE_SPECULATIVE_VA_SA==1
			if (m_input_unit[inport_iter]->need_stage(invc_iter, VC_SP_, VA_, m_router->curCycle()))//Changed only VC_AB_ state in speculative state VC_SP_ 
			#else
			if (m_input_unit[inport_iter]->need_stage(invc_iter, VC_AB_, VA_, m_router->curCycle()))
			#endif 
			{
				//Speculative statistics
				 Router_d::incVaGlobalCountStatic();
				 m_router->incVaLocalCount();
			#if USE_VNET_REUSE==1 
				/*	
					check if the allocated packets on this outport are 
					already = to the number of vc per vnet, to prevent possible deadlocks.
				*/
				//int count_used_vnet_vcs=0;
				int req_outport = m_input_unit[inport_iter]->get_route(invc_iter);
				int req_vnet_original = (m_input_unit[inport_iter]->peekTopFlit(invc_iter))->get_vnet();
				assert(req_vnet_original!=-1);

				bool flag_continue=false;	
				//new code
				//int outvc_base=0; // the offset_vc will be added later
				int num_vcs_to_analyze=m_num_vcs;

				// allocate a counter per each vnet of the coherence protocol
				// per each specific outport required by the analyzed
				// (inport_iter,invc_iter)
				std::vector<int> count_vc_used_vnet;
				count_vc_used_vnet.resize(m_virtual_networks_protocol);
				for(int zs=0;zs<m_virtual_networks_protocol;zs++)
					count_vc_used_vnet[zs]=0;
				
				int idle_vcs=0;
				#if USE_APNEA_BASE==1
				//using the apnea policy the vnet-reuse check for deadlock is
				//computed on the active vcs only!!!
				for (int outvc_offset_iter = m_router->getInitVcOp(req_outport); outvc_offset_iter < m_router->getFinalVcOp(req_outport); outvc_offset_iter++)
				{
					if(m_output_unit[req_outport]->is_vc_idle(outvc_offset_iter, m_router->curCycle()))
					{// check if the vc is idle
						idle_vcs++;
					}
					else
					{// if not idle check the vc that is using it
						assert(vnet_in_this_vc_outport[req_outport][outvc_offset_iter] != -5);
						assert(vnet_in_this_vc_outport[req_outport][outvc_offset_iter] != -2);
						count_vc_used_vnet[vnet_in_this_vc_outport[req_outport][outvc_offset_iter]]++;
					}	
				}

				#else
				for (int outvc_offset_iter = 0; outvc_offset_iter < num_vcs_to_analyze; outvc_offset_iter++)
				{
					if(!(outvc_offset_iter >=0 && outvc_offset_iter <m_num_vcs))
						std::cout<<outvc_offset_iter <<std::endl;
					if(m_output_unit[req_outport]->is_vc_idle(outvc_offset_iter, m_router->curCycle()))
					{// check if the vc is idle
						idle_vcs++;
					}
					else
					{// if not idle check the vc that is using it
						assert(vnet_in_this_vc_outport[req_outport][outvc_offset_iter] != -5);
						assert(vnet_in_this_vc_outport[req_outport][outvc_offset_iter] != -2);
						count_vc_used_vnet[vnet_in_this_vc_outport[req_outport][outvc_offset_iter]]++;
					}	
				}
				#endif
				//policy is aggressive use of vcs. At least 1vc used/usable per protocol vnet
				for(int qiter=0; qiter<m_virtual_networks_protocol;qiter++)
				{// check the requiring vnet only, since it is the only that can acquire a vc
					if (req_vnet_original==qiter)
					{
						if(count_vc_used_vnet[qiter]<(m_vc_per_vnet+m_virtual_networks_spurious)) 
						{
							assert(count_vc_used_vnet[qiter]>=0);
							continue;
						}
						else
						{
							flag_continue=true;
							break;// this vnet reached the maximum number of allowable vcs
						}
					}
					else
					{// other vnet at least 1vc free or used
						if(count_vc_used_vnet[qiter]>0)
						{
							//if(!(count_vc_used_vnet[qiter]<=(m_vc_per_vnet+m_virtual_networks_spurious)))
							//	std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()
							//		<<" qiter"<<qiter<<" "
							//		<<count_vc_used_vnet[qiter]<<" "
							//		<<"m_virtual_networks_spurious"<<m_virtual_networks_spurious<<" "
							//		<<"m_vc_per_vnet"<<m_vc_per_vnet
							//		 <<std::endl;
							//assert(count_vc_used_vnet[qiter]<=m_vc_per_vnet+m_virtual_networks_spurious);
							continue;
						}
						else  if (idle_vcs>1)
							idle_vcs--; // 1 idle vc for this vnet
						else
						{
							//Speculative statistics
							Router_d::incVaNoBuffGlobalCountStatic();
							m_router->incVaNoBuffLocalCount();
	
							flag_continue=true;
							break;// this vnet reached the maximum number of allowable vcs
						}
					}
				}
				if(flag_continue==true)
					continue;
			#endif	
		
		#if USE_WH_VC_SA==1	
			#if USE_VC_REUSE == 1  //for the buffer reuse
			  // this is speculative, since the VA stage for a reused buf can
			  // degrade other packets, that are from a non-reused buffer
				#if USE_NON_ATOMIC_VC_ALLOC==0
				if( m_input_unit[inport_iter]->get_reused_remap_vc_used()[invc_iter]==true )
				{
					flag_vc_reused=true; //check auxiliary info not the one in the packet
		
					if(m_input_unit[inport_iter]->get_reused_remap_vc_outport()[invc_iter]!=-1 /*RC done*/
							&& m_input_unit[inport_iter]->get_reused_remap_vc_outvc()[invc_iter]==-1 /*need VA*/)
					{
						if (!is_invc_candidate(inport_iter, invc_iter))
            	        				continue; 
						select_outvc(inport_iter, invc_iter);
					}
				}
				#endif
			#endif
		#endif
			}// invc need stage
			if(flag_vc_reused==false)
			{
				#if USE_SPECULATIVE_VA_SA==1		
					if (m_input_unit[inport_iter]->need_stage(invc_iter, VC_SP_, VA_, m_router->curCycle())) //Changed only VC_AB_ state in speculative state VC_SP_ 
				#else
					if (m_input_unit[inport_iter]->need_stage(invc_iter, VC_AB_, VA_, m_router->curCycle()))
				#endif 
				{
					if (!is_invc_candidate(inport_iter, invc_iter))
						continue; 
					select_outvc(inport_iter, invc_iter);
				}
			}
        }
    }
}

void
VCallocator_d::arbitrate_outvcs()
{
	std::vector<int> outvc_allocated_per_vnet;
	outvc_allocated_per_vnet.resize(m_num_vcs/m_vc_per_vnet);
	for(int i=0;i<outvc_allocated_per_vnet.size();i++)
		outvc_allocated_per_vnet.at(i)=0;

    for (int outport_iter = 0; outport_iter < m_num_outports; outport_iter++) 
	{
        for (int outvc_iter = 0; outvc_iter < m_num_vcs; outvc_iter++) 
		{
            if (!m_outvc_is_req[outport_iter][outvc_iter]) 
			{
                // No requests for this outvc in this cycle
                continue;
            }
		#if USE_APNEA_BASE==1				
			if( !(outvc_iter>=m_router->getInitVcOp(outport_iter) &&outvc_iter<m_router->getFinalVcOp(outport_iter)) )
				continue;
		#endif

		#if USE_VICHAR==1
			//allocate a vc if there is at least a slot
			if(!m_output_unit[outport_iter]->getAvailSlotPerVnet(outvc_iter/m_vc_per_vnet)>0)
				continue;
		#endif
            int inport = m_round_robin_outvc[outport_iter][outvc_iter].first;
            int invc_offset = m_round_robin_outvc[outport_iter][outvc_iter].second;

            int vnet = get_vnet(outvc_iter);
            int invc_base = vnet*m_vc_per_vnet;
            int num_vcs_per_vnet = m_vc_per_vnet;

	//#if USE_WH_VC_SA==1
	#if USE_VNET_REUSE==1
			// if the outlink is towards a NiC all the invc can be directed to
			// all
			//NetworkLink_d* link = m_output_unit[outport_iter]->getOutLink_d();
			//NetworkInterface_d* ni = dynamic_cast<NetworkInterface_d*>(link->getLinkConsumer());
			//if(ni)
			{
				#if USE_APNEA_BASE==1				
				invc_base=m_router->getInitVcOp(outport_iter);
				num_vcs_per_vnet=m_router->getFinalVcOp(outport_iter);
				#else
				invc_base=0;
				num_vcs_per_vnet=m_num_vcs;
				#endif
			}

	#endif
	//#endif
		//tiro fuori 
            for (int in_iter = 0; in_iter < m_num_inports*num_vcs_per_vnet; in_iter++) 
			{
                invc_offset++;
                if (invc_offset >= num_vcs_per_vnet) 
				{
                    invc_offset = 0;
                    inport++;
                    if (inport >= m_num_inports)
                        inport = 0;
                }
                int invc = invc_base + invc_offset;
                if (m_outvc_req[outport_iter][outvc_iter][inport][invc]) 
				{// reserving outvc outport for a invc inport
		#if USE_APNEA_BASE==1	
					assert(outvc_iter>=m_router->getInitVcOp(outport_iter) && outvc_iter<m_router->getFinalVcOp(outport_iter));
		#endif
                    m_global_arbiter_activity[vnet]++;
                    m_global_arbiter_activity_resettable[vnet]++;
					//update the vc round_robin information for the arbiters//

            		m_round_robin_outvc[outport_iter][outvc_iter].first=inport;
            		m_round_robin_outvc[outport_iter][outvc_iter].second= invc_offset;
			
					//check if buf reused
					bool flag_vc_reused=false;
			#if USE_WH_VC_SA==1 //for the buffer reuse
			#if USE_VC_REUSE == 1
					if( m_input_unit[inport]->get_reused_remap_vc_used()[invc]==true )
					{
						flag_vc_reused=true; //check auxiliary info not the one in the packet
						m_input_unit[inport]->get_reused_remap_vc_outvc()[invc]=outvc_iter;
						m_input_unit[inport]->get_reused_remap_vc_outvc_va_cycle()[invc]=m_router->curCycle();
						assert(m_input_unit[inport]->get_reused_remap_vc_outport()[invc]==outport_iter);
						break;
					}
			#endif
			#endif
					if(flag_vc_reused==false)
					{
						vnet_in_this_vc_outport[outport_iter][outvc_iter]=
									m_input_unit[inport]->peekTopFlit(invc)->get_vnet();
						assert(vnet_in_this_vc_outport[outport_iter][outvc_iter]!=-2);

						/*NON_ATOMIC_VC_ALLOC*/
					#if  USE_NON_ATOMIC_VC_ALLOC == 1
						assert( m_output_unit[outport_iter]->get_vc_counter_pkt(outvc_iter, m_router->curCycle())>0);
						//assert(TODO);		
						m_output_unit[outport_iter]->dec_vc_counter_pkt(outvc_iter, m_router->curCycle());
						m_output_unit[outport_iter]->set_vc_busy_to_vnet_id(outvc_iter,m_router->getVirtualNetworkProtocol()/*NOVNETID*/);
					#endif
						//NON_ATOMIC_PRINT
						//std::cout<<"ROUTER["<<m_router->get_id()<<"] DECR => "<<m_output_unit[outport_iter]->get_vc_counter_pkt(outvc_iter, m_router->curCycle()+1)<<std::endl;			

                    	m_input_unit[inport]->grant_vc(invc, outvc_iter, m_router->curCycle());
                    	m_output_unit[outport_iter]->update_vc( outvc_iter, inport, invc);
					#if USE_APNEA_BASE==1		
					// check the selected VC is in the right range
						//std::cout<<"@"<<curTick()<<" used_outvc:"<<outvc_iter<<std::endl;
						assert(outvc_iter>=m_router->getInitVcOp(outport_iter) &&
							outvc_iter<m_router->getFinalVcOp(outport_iter));
					#endif
						//Speculative statistics
						if( m_input_unit[inport]->peekTopFlit(invc)->get_type() == HEAD_ ||
								m_input_unit[inport]->peekTopFlit(invc)->get_type() == HEAD_TAIL_ )
						{
							Router_d::decGlobalCountStatic();
							m_router->decLocalCount();
							//  std::cout<<"[DEC] Globale ["<<Router_d::va_global_arbit_count<<"] 
							//		Router = "<<m_router->get_id()<<" Locale ["<<m_router->getLocalCount()<<"]"<<std::endl;
						}
						m_router->print_pipeline_state(m_input_unit[inport]->peekTopFlit(invc), "VA");
						Router_d::incVaWinGlobalCountStatic();
						m_router->incVaWinLocalCount();
						
					#if USE_SPECULATIVE_VA_SA==1
						bool spec_succesful=false;					
						spec_succesful=spec_SWallocator(inport, invc, outport_iter, outvc_iter);
						if(spec_succesful)
						{					
							m_router->print_pipeline_state(m_router->spec_get_switch_buffer_flit(inport), "VA_SPEC_WIN");
						}
						else
						{
							m_router->print_pipeline_state(m_input_unit[inport]->peekTopFlit(invc), "VA_SPEC_LOSE");
						}
					#endif
						m_router->swarb_req();
					break;
					}//if(flag_vc_reused==false)
                }// reserving outvc outport for a invc inport
            }
        }
    }
}

int
VCallocator_d::get_vnet(int invc)
{
    int vnet = invc/m_vc_per_vnet;
    assert(vnet < m_router->get_num_vnets());

    return vnet;
}

void
VCallocator_d::check_for_wakeup()
{
    for (int i = 0; i < m_num_inports; i++) 
	{
        for (int j = 0; j < m_num_vcs; j++) 
		{
            #if USE_SPECULATIVE_VA_SA==1
	    	if (m_input_unit[i]->need_stage_nextcycle(j, VC_SP_, VA_,m_router->curCycle())) //Changed only VC_AB_ state in speculative state VC_SP_ 
            #else
	    	if (m_input_unit[i]->need_stage_nextcycle(j, VC_AB_, VA_,m_router->curCycle())) 
			#endif
	    	{
                scheduleEvent(1);
                return;
            }
        }
    }
}

#if USE_SPECULATIVE_VA_SA==1
//SPECULATIVE: add new function into VCallocation to emulate SWallocator in speculation return true if win speculation
bool
VCallocator_d::spec_SWallocator(int inport, int invc, int outport, int outvc){

	//std::cout<<"@"<<curTick()<<" spec_SWallocator_arbitrate_inports"<<std::endl;
	static long spec_sa_win=0;
	Router_d::incSaSpecGlobalCountStatic();
	m_router->incSaSpecLocalCount();

	bool specResult=spec_SWallocator_arbitrate_inports(inport, invc, outport, outvc);
	if(specResult)
	{		
		//std::cout<<"@"<<curTick()<<" spec_SWallocator_arbitrate_outport"<<std::endl;
		specResult=spec_SWallocator_arbitrate_outport(inport, invc, outport, outvc);
		//if(specResult)
			spec_sa_win++;
			//std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()<<" IP"<<inport<<" invc"<<invc<<" SPECULATION SUCCEDED"<<std::endl;
	}
//	std::cout<<"Req tot VA "<<va_spec_req_tot<<" spec_tot_req:"<<spec_va_win<<" spec_ok:"<<spec_sa_win<<std::endl;
	return specResult;
}

bool
VCallocator_d::spec_SWallocator_arbitrate_inports(int inport, int invc, int outport, int outvc){

	//Inport win VAllocation (don't cycle to manage every inport but only winner inport in VA stage)
  		/*
		check if a single outlink is busy and evanutally stall the pipeline:
		easier to implement router looses performance. A performance oriented
		method avoids sa on busy out links only.
		*/

		for(int i=0;i<m_output_unit.size();i++)
		{   Resynchronizer *r=m_output_unit.at(i)->getOutLink_d()->getReshnchronizer();
			if(r->isAcknowledgeAvailable()) 
				r->clearAcknowledge();
			m_outport_busy[i]=false;
			if(r->isAcknowkedgePending())
			{
				r->advanceTransmitSide();
				m_outport_busy[i]=true;
			}
		}
		
		for(int i=0;i<m_input_unit.size();i++)
		{
			    Resynchronizer *r=m_input_unit.at(i)->getCreditLink_d()->getReshnchronizer();
			if(r->isAcknowledgeAvailable())
				r->clearAcknowledge();

			m_inport_credit_busy[i]=false;
			if(r->isAcknowkedgePending())
			{
				r->advanceTransmitSide();
				m_inport_credit_busy[i]=true;
			}
		}
	//Check credit busy inport 
	if(m_inport_credit_busy[inport])
	{// resynch implementation
		return false;
	}

	#if USE_WH_VC_SA==1
	// check to use inport for a reserved path at most
		bool reserved = false;
		for(int j=0; j<m_router->get_num_vcs();j++)
			for(int i = 0; i < m_num_outports; i++) 
				if ( (j!=invc || i!=outport) && m_router->get_m_wh_path_busy(inport, j, i) == true) 
				{
					reserved = true;
					break;
				}
		if (reserved) 
			return false;
	#endif

	// switch buffer has a flit for the next cycle to be sent for this inport
	if(m_router->spec_get_switch_buffer_ready(inport,m_router->curCycle()))
	{
		flit_d* temp_flit=m_router->spec_get_switch_buffer_flit(inport);
		if(temp_flit!=NULL)
		//if(	(temp_flit->get_stage()).first==ST_ && 	(temp_flit->get_stage()).second== m_router->curCycle()+1)
			return false;
	}
	// check if someone in this inport already triggered an SA request
	for (int h = 0; h < m_num_vcs; h++)
	{
		if( m_input_unit[inport]->need_stage(h, ACTIVE_,SA_,m_router->curCycle()) )
			return false;
	}  

    if (!((m_router->get_net_ptr())->validVirtualNetwork(get_vnet(invc))))
		return false;

#if USE_VICHAR==1
	if(m_output_unit[outport]->getAvailSlotPerVnet(outvc/m_vc_per_vnet)>0)
#else
	if(m_output_unit[outport]->get_credit_cnt(outvc)>0) 
#endif
	{
		if (spec_SWallocator_is_candidate_inport(inport, invc)  &&  m_port_req[outport][inport] == false) 
		{
		//Outport came from parameter

			   m_spec_local_arbiter_activity++;
			   m_spec_local_arbiter_activity_resettable++;
			   m_port_req[outport][inport] = true;
			   return true; // got one vc winner for this port
		}
	}
        return false;
}

bool
VCallocator_d::spec_SWallocator_arbitrate_outport(int inport,int invc, int outport, int outvc){

	if(m_outport_busy[outport])
	{// fast switch constraint
		return false;
	}

	#if USE_WH_VC_SA
	// check if multiple input wants the same eventually reserved output
		bool check_reserved_out=false;
		for(int i=0;i<m_num_inports;i++)
			for(int j=0;j<m_router->get_num_vcs();j++)
				if(m_router->get_m_wh_path_busy(i, j ,outport)==true && (inport!=i || invc!=j))
				{
					check_reserved_out=true;
					break;
				}
		if(check_reserved_out==true)
			return false;
	#endif

	//Check if route inport to outport is yet free
	assert(m_port_req[outport][inport]); 
		
	 //Check for every input port in switch buffer if there is already a flit ready to forward crossbar in my input port or in the same outport
	for (int h = 0; h < m_num_inports; h++)
	{ 
		if (!m_router->spec_get_switch_buffer_ready(h, m_router->curCycle()))
		    continue;	//if the switch buffer is free
		assert(h!=inport);
		flit_d *k_flit = m_router->spec_get_switch_buffer_flit(h);
		if(k_flit==NULL)
			continue;

	//	if (k_flit->is_stage(ST_, m_router->curCycle()+1) /*|| k_flit->is_stage(SA_, m_router->curCycle())*/) 
	//	{
		    if((k_flit->get_outport())==outport)
			return false;    //This outport has already an ST flit with the same outport as mine
	//	}
	}
					
	m_port_req[outport][inport] = false;

	// update round_robin_information for arbiters//
	m_round_robin_outport[outport]=inport;
	assert(m_output_unit[outport]->get_credit_cnt(outvc)>0);
	//assert(m_input_unit[inport]->has_credits(invc));//FIXME it seems wrong!!! //TODO SA Checked in arbitrate inport
	#if USE_VICHAR==1
		assert(m_output_unit[outport]->getAvailSlotPerVnet(outvc/m_vc_per_vnet)>0);
	#endif
	//SPECULATIVE: IMPORTANT update Flit with new stage!!!
	// remove flit from Input Unit and update stage with switch traversal ST

	flit_d *t_flit = m_input_unit[inport]->getTopFlit(invc);
	t_flit->advance_stage(ST_, m_router->curCycle());
	t_flit->set_vc(outvc); //TODO in VA
	t_flit->set_outport(outport);//TODO in VA
	t_flit->set_time(m_router->curCycle() + 1);	

	m_output_unit[outport]->decrement_credit(outvc);

	//Add the new flit into switch buffer
	m_router->update_sw_winner(inport, t_flit);
	m_spec_global_arbiter_activity++;//TODO Check next
	m_spec_global_arbiter_activity_resettable++;//TODO Check next 
	
	bool free_signal_fast = true;
	#if USE_WH_VC_SA==1
		// set the reserved path for the winner, it could be useless, if done on
		// an already reserved path
        m_router->set_m_wh_path_busy(true, inport, invc, outport);	
	
	///// eventually set reuse vc and signal fake vc free//////////
				#if USE_VC_REUSE==1
				int flit_stored = m_input_unit[inport]->getCongestion(invc); 
				int flit_buf_size = m_input_unit[inport]->getBufferSize(invc);
				int out_flit_cnt = m_output_unit[outport]->get_credit_cnt(outvc);
				bool firstTailLastFlit = m_input_unit[inport]->isFirstTailLastFlit(invc);
				bool reusable = m_input_unit[inport]->get_reused_remap_vc_usable()[invc];
				bool reused = m_input_unit[inport]->get_reused_remap_vc_used()[invc];

				assert(flit_buf_size >= flit_stored);

				if (   ( firstTailLastFlit || (t_flit->get_type() == TAIL_) || t_flit->get_type() == HEAD_TAIL_/*all remaining packet flits stored in the buffer*/ )
					&& ( flit_stored <= out_flit_cnt /*enough space in the downstream in buffer*/ )
					&& ( m_router->get_m_wh_path_busy(inport,invc,outport) == true/*path reserved*/)
					&& ( !reused ) && ( !reusable ) /*not reused or reusable flagged*/
					&& ( flit_buf_size > flit_stored ) /*one flit slot free to store new packet flits */ 
				)
				{// declare it reusable and send a sig_free control sig without turn it into ready state!!!	

						assert(m_input_unit[inport]->isFirstTailLastFlit(invc)|| (t_flit->get_type() == TAIL_) || t_flit->get_type() == HEAD_TAIL_);

						m_input_unit[inport]->get_reused_remap_vc_usable()[invc]=true;
					// TODO DELETE only the info, i.e. outvc and outport, of the new reusing packet are
					// stored in temporary elements
						//m_input_unit[inport]->get_reused_remap_vc_outport()[invc]=outport;
						//m_input_unit[inport]->get_reused_remap_vc_outvc()[invc]=outvc;
						
						//FIXME if optimized for performance sent back the fake free sig to increase the throughput
						m_input_unit[inport]->get_reused_remap_vc_fake_free_sig()[invc]=m_router->curCycle();
				}

				//Check if i've already send free_signal for buffer reused
				if(m_router->curCycle() == m_input_unit[inport]->get_reused_remap_vc_fake_free_sig()[invc])
				{
					free_signal_fast=true;
					m_input_unit[inport]->get_reused_remap_vc_fake_free_sig()[invc]=0;
				}else
				{	
					free_signal_fast=false;
				}
				#endif //USE_VC_REUSE==1
			#endif //USE_WH_VC_SA==1

	if ((t_flit->get_type() == TAIL_) || t_flit->get_type() == HEAD_TAIL_) 
	{
		#if USE_WH_VC_SA==1
	  	// deassert the reserved path if this is the tail flit
		 m_router->seutport_iter]->update_vc( outvc_iter, inport, invc);utport_iter]->update_vc( outvc_iter, inport, invc);t_m_wh_path_busy(false, inport, invc, outport);
		#endif

	#if USE_VC_REUSE == 1
		#if USE_NON_ATOMIC_VC_ALLOC==1
			if(m_input_unit[inport]->getOutportTempQueue(invc)->size()==0)
			{
				m_input_unit[inport]->set_vc_state(IDLE_, invc, m_router->curCycle()-1);
			}
			else
			{//some other packet is stored in the same input buffer test if head or head_tail
				assert(m_input_unit[inport]->peekTopFlit(invc)->get_type() == HEAD_ 
					|| m_input_unit[inport]->peekTopFlit(invc)->get_type() == HEAD_TAIL_);
			}
		#else	
         if(m_input_unit[inport]->get_reused_remap_vc_used()[invc]==false)
         {
            //m_input_unit[inport]->get_reused_remap_vc_usable()[invc]=false;
            //m_input_unit[inport]->increment_credit(invc, true, m_router->curCycle());
               // This Input VC should now be empty
            assert(m_input_unit[inport]->isReady(invc, m_router->curCycle()) == false);
            
            // FIXME FORWARD IDLE info to allow reuse of consecutive single flit packets
            // we are gaining 1 cycle only with active vc_reuse
               m_input_unit[inport]->set_vc_state(IDLE_, invc, m_router->curCycle()-1); 

               //m_input_unit[inport]->set_enqueue_time(invc, INFINITE_);
         }
         m_input_unit[inport]->get_reused_remap_vc_usable()[invc]=false;
         m_input_unit[inport]->set_enqueue_time(invc, INFINITE_);
		#endif
	 #else
		// This Input VC should now be empty
		#if USE_NON_ATOMIC_VC_ALLOC==1
		if(m_input_unit[inport]->getOutportTempQueue(invc)->size()==0)
		{
			m_input_unit[inport]->set_vc_state(IDLE_, invc, m_router->curCycle()-1);
		}
		else
		{//some other packet is stored in the same input buffer test if head or head_tail
			assert(m_input_unit[inport]->peekTopFlit(invc)->get_type() == HEAD_ 
			|| m_input_unit[inport]->peekTopFlit(invc)->get_type() == HEAD_TAIL_);
		}
		#else	
			assert(m_input_unit[inport]->isReady(invc, m_router->curCycle()) == false);
			m_input_unit[inport]->set_vc_state(IDLE_, invc, m_router->curCycle());
		#endif
	#endif//USE_VC_REUSE==1
//         // This Input VC should now be empty	
//         assert(m_input_unit[inport]->isReady(invc, m_router->curCycle()) == false);
//         m_input_unit[inport]->set_vc_state(IDLE_, invc, m_router->curCycle());
//     #endif //USE_VC_REUSE==1
         m_input_unit[inport]->increment_credit(invc, true, free_signal_fast, m_router->curCycle());
         m_input_unit[inport]->set_enqueue_time(invc, INFINITE_);
		/*congestion metrics starting cycle evaluation*/
		Router_d::CongestionMetrics& cm1=m_router->getC1Matrix().at(inport*m_num_vcs+invc).at(m_input_unit[inport]->get_route(invc));	
		//assert(cm1.t_start!=-1);
		cm1.count+=m_router->curCycle()-cm1.t_start;
		cm1.t_start=Cycles(-1);
		
		Router_d::CongestionMetrics& cm2= m_router->getC2Matrix().at(invc).at(m_input_unit[inport]->get_route(invc));
		//assert(cm2.t_start!=-1);
		cm2.count+=m_router->curCycle()-cm1.t_start;
		cm1.t_start=Cycles(-1);
	/*end congestion*/

	}
	else /*winner flit not an HEAD_TAIL_ or TAIL_*/
	{			
	    // Send a credit back
	    // but do not indicate that the VC is idle
		m_input_unit[inport]->increment_credit(invc, false, free_signal_fast, m_router->curCycle());
	}

	return true; // got a in request for this outport
}

bool
VCallocator_d::spec_SWallocator_is_candidate_inport(int inport, int invc)
{
    int outport = m_input_unit[inport]->get_route(invc);
    int t_enqueue_time = m_input_unit[inport]->get_enqueue_time(invc);
    int t_vnet = get_vnet(invc);
    int vc_base = t_vnet*m_vc_per_vnet;
    if ((m_router->get_net_ptr())->isVNetOrdered(t_vnet)) 
	{
        for (int vc_offset = 0; vc_offset < m_vc_per_vnet; vc_offset++) 
		{
	    int temp_vc = vc_base + vc_offset;
        //TODO REMOVE #if USE_SPECULATIVE_VA_SA==1
            if (m_input_unit[inport]->need_stage(temp_vc, VC_SP_, VA_, m_router->curCycle()) &&
               (m_input_unit[inport]->get_route(temp_vc) == outport) &&
               (m_input_unit[inport]->get_enqueue_time(temp_vc) <
                    t_enqueue_time)) 
        //#endif 
	    {
                return false;
                break;
            }
        }
    }
    return true;
}

void
VCallocator_d::spec_SWallocator_setup()
{
	bool skip=false;
	if(!m_router->getPowerStateSW(0))
	{
		skip=true;
	}
	if( m_router->getTickChangePowerStateSW()==curTick() && m_router->getPowerStateSW(0) && !m_router->getNextPowerStateSW(0) )
	{// switch is switching off but not yet happened due to eventqueue order
		skip=true;
	}
	if( m_router->getTickChangePowerStateSW()==curTick() && !m_router->getPowerStateSW(0) && m_router->getNextPowerStateSW(0) )
	{	// switch is switching on but not yet happened due to eventqueue order
		//use this to avoid losing one cycle
		skip=false;
	}


	if (skip)
	{
	//	DPRINTF(RubyPowerGatingSA, "@%lld Router_%d->SA WAIT FOR SWITCH SW_POWER_STATE %d NEXT_SW_STATE %d cycle %lld\n ",
	//		m_router->curCycle(),m_router->get_id(),m_router->getPowerStateSW(0), m_router->getNextPowerStateSW(0));
		scheduleEvent(1);
	}
	else
	{
		/*
		check if a single outlink is busy and evanutally stall the pipeline:
		easier to implement router looses performance. A performance oriented
		method avoids sa on busy out links only.
		*/

		for(int i=0;i<m_output_unit.size();i++)
		{   Resynchronizer *r=m_output_unit.at(i)->getOutLink_d()->getReshnchronizer();
			if(r->isAcknowledgeAvailable()) 
				r->clearAcknowledge();
			m_outport_busy[i]=false;
			if(r->isAcknowkedgePending())
			{
				r->advanceTransmitSide();
				m_outport_busy[i]=true;
			}
		}
		
		for(int i=0;i<m_input_unit.size();i++)
		{
			    Resynchronizer *r=m_input_unit.at(i)->getCreditLink_d()->getReshnchronizer();
			if(r->isAcknowledgeAvailable())
				r->clearAcknowledge();

			m_inport_credit_busy[i]=false;
			if(r->isAcknowkedgePending())
			{
				r->advanceTransmitSide();
				m_inport_credit_busy[i]=true;
			}
		}
	
	}
}

void
VCallocator_d::spec_SWallocator_startup()
{

	m_spec_local_arbiter_activity = 0;
	m_spec_global_arbiter_activity = 0;
        m_spec_local_arbiter_activity_resettable = 0;
        m_spec_global_arbiter_activity_resettable = 0;
	
	//init vector
	m_outport_busy.resize(m_num_outports);
	m_port_req.resize(m_num_outports);
	m_round_robin_outport.resize(m_num_outports);
    m_inport_credit_busy.resize(m_num_inports);
	
	// to implement the wormhole flow control in a vc architecture
	
	for (int i = 0; i < m_num_outports; i++) 
	{
			m_port_req[i].resize(m_num_inports);
			m_round_robin_outport[i] = 0;

		for (int j = 0; j < m_num_inports; j++) 
		{
			m_port_req[i][j] = false; // [outport][inport]
		}
	}
}

void
VCallocator_d::spec_SWallocator_clear_request_vector()
{
	 for (int i = 0; i < m_num_outports; i++) {
		for (int j = 0; j < m_num_inports; j++) {
		    m_port_req[i][j] = false;
		}
	  }
}
#endif //end speculative pipeline

bool
VCallocator_d::isTurnAllowed(int temp_inport,int temp_outport)
{
	int curCol=m_router->get_id()%m_router->get_net_ptr()->get_num_cols();
	int curRow=m_router->get_id()/m_router->get_net_ptr()->get_num_cols();
	// get previous hop num_col and num_row
	int src_col=-1;
	int src_row=-1;
	//std::cout<<"VCallocator_d::isTurnAllowed"<<std::endl;
	
	OutputUnit_d* maybeInRouter = dynamic_cast<OutputUnit_d*>(m_router->get_inputUnit_ref().at(temp_inport)->getInLink_d()->getLinkSource());
	//int prevRid=-1;

	int rInId=-1;
	if(maybeInRouter!=NULL)
	{
		//prevRid= (maybeInRouter->get_router())->get_id();
		rInId = maybeInRouter->get_router()->get_id();
		src_col = rInId%m_router->get_net_ptr()->get_num_cols();
		src_row = rInId/m_router->get_net_ptr()->get_num_cols();
	}
	else//previous hop is a NIC thus all turns are permitted
	{
		return true;
	}

	// get next hop num_col and num_row

	int dest_col=-1;
	int dest_row=-1;	

	InputUnit_d* maybeOutRouter = dynamic_cast<InputUnit_d*>
				(m_router->get_outputUnit_ref().at(temp_outport)->getOutLink_d()->getLinkConsumer());
	//int nextRid=-1;
	int rOutId=-1;
	if(maybeOutRouter!=NULL)
	{
		//nextRid=(maybeOutRouter->get_router())->get_id();
		rOutId = maybeOutRouter->get_router()->get_id();
		dest_col = rOutId%m_router->get_net_ptr()->get_num_cols();
		dest_row = rOutId/m_router->get_net_ptr()->get_num_cols();
	}
	else//next hop is a NIC thus all turns are allowed
	{
		return true;
	}

	bool isAllowed=false;
	#if NON_ATOMIC_VC_ALLOC_FULLY_ADP_ROUTING == 0
		// XY allowed
		isAllowed=checkTurnXY(curRow,curCol,src_row,src_col,dest_row,dest_col);
	#elif NON_ATOMIC_VC_ALLOC_FULLY_ADP_ROUTING == 1
		// YX allowed
		isAllowed=checkTurnYX(curRow,curCol,src_row,src_col,dest_row,dest_col);
	#elif NON_ATOMIC_VC_ALLOC_FULLY_ADP_ROUTING == 2
		// North-first
		isAllowed=checkNorthFirst(curRow,curCol,src_row,src_col,dest_row,dest_col);
		#elif NON_ATOMIC_VC_ALLOC_FULLY_ADP_ROUTING == 3
		// Negative-first
		isAllowed=checkNegativeFirst(curRow,curCol,src_row,src_col,dest_row,dest_col);	
	#endif
//	std::cout
//			<<"ALLOWED ? "<<isAllowed
//			<<" num_cols"<<m_router->get_net_ptr()->get_num_cols()
//			<<" num_rows"<<m_router->get_net_ptr()->get_num_rows()
//			<<"\tcurR"<<m_router->get_id() <<" (row:"<<curRow<<" col:"<<curCol<<")"
//			<<"\tRin:" <<prevRid <<" (row:"<<src_row	 <<" col:"<<src_col <<")"	
//			<<"\tRout:"<<nextRid <<" (row:"<<dest_row <<" col:"<<dest_col<<")"
//			<<std::endl;
	return isAllowed;
}

bool
VCallocator_d::checkTurnXY(int rowCur,int colCur,int rowSrc,int colSrc,int rowDst,int colDst)
{
	if( rowSrc==rowDst || colSrc==colDst )
	{//not a turn at all
		return true;
	}
	if(rowSrc==rowCur)
	{// an XY generic turn
		return true;
	}
	return false;	
}


bool
VCallocator_d::checkTurnYX(int rowCur,int colCur,int rowSrc,int colSrc,int rowDst,int colDst)
{
	if( rowSrc==rowDst || colSrc==colDst )
	{//not a turn at all
		return true;
	}
	if(colSrc==colCur)
	{// an YX generic turn
		return true;
	}
	return false;	
}

bool
VCallocator_d::checkNorthFirst(int rowCur,int colCur,int rowSrc,int colSrc,int rowDst,int colDst)
{ 
  if( rowSrc==rowDst || colSrc==colDst )
  {//not turn at all
				return true;
	}
	if( rowCur < rowDst )
	{//the second move of the turn is to the North: not allowed only the first move can be to th north 
     return false;
  }
	return true;	
}

bool
VCallocator_d::checkNegativeFirst(int rowCur,int colCur,int rowSrc,int colSrc,int rowDst,int colDst)
{ 
  if( rowSrc==rowDst || colSrc==colDst )
  {//not turn at all
				return true;
	}
	if( rowDst > rowCur || colDst < colCur )
	{//the second move of the turn is to the North: not allowed only the first move can be to th north 
     return false;
  }
	return true;	
}
