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
#include "mem/ruby/network/garnet/fixed-pipeline/SWallocator_d.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_d.hh"

#define PRIORITY_SCHED 1

SWallocator_d::SWallocator_d(Router_d *router)
    : Consumer(router)
{
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_vc_per_vnet = m_router->get_vc_per_vnet();

    m_local_arbiter_activity = 0;
    m_global_arbiter_activity = 0;
    m_local_arbiter_activity_resettable = 0;
    m_global_arbiter_activity_resettable = 0;
    
    this->setEventPrio(Event::NOC_SWAllocator_Pri);
}

void
SWallocator_d::init()
{
    m_input_unit = m_router->get_inputUnit_ref();
    m_output_unit = m_router->get_outputUnit_ref();

    m_num_inports = m_router->get_num_inports();
    m_num_outports = m_router->get_num_outports();
    m_round_robin_outport.resize(m_num_outports);
    m_round_robin_inport.resize(m_num_inports);
    m_port_req.resize(m_num_outports);
    m_vc_winners.resize(m_num_outports);
	// to implement the fast switch
	m_outport_busy.resize(m_num_outports);
	m_inport_credit_busy.resize(m_num_inports);

	// to implement the wormhole flow control in a vc architecture
	m_wh_path_busy.resize(m_num_inports);
	for(int i=0;i< m_num_inports;i++)
	{
		m_wh_path_busy[i].resize(m_router->get_num_vcs());
		for(int j=0;j<m_router->get_num_vcs();j++)
		{
			m_wh_path_busy[i][j].resize(m_num_outports);
			for(int r=0;r<m_num_outports;r++)
			{		
				m_wh_path_busy[i][j][r]=false;
			}
		}
	}
	///////////////////////////////////////////////////////////////
    for (int i = 0; i < m_num_inports; i++) 
	{
        m_round_robin_inport[i] = 0;
    }

    for (int i = 0; i < m_num_outports; i++) 
	{
        m_port_req[i].resize(m_num_inports);
        m_vc_winners[i].resize(m_num_inports);

        m_round_robin_outport[i] = 0;

        for (int j = 0; j < m_num_inports; j++) {
            m_port_req[i][j] = false; // [outport][inport]
        }
    }
	
	#if PRIORITY_SCHED == 1

	m_priority_invc.resize(m_num_inports);

	for(int inport = 0;inport<m_num_inports;inport++)
	{
		m_priority_invc[inport].resize(m_num_vcs);
		for(int invc = 0;invc < m_num_vcs;invc++)
		{
			m_priority_invc[inport][invc]=0;
		}
	}

	#endif

}



void
SWallocator_d::wakeup()
{

	#if PRIORITY_SCHED == 1
	
	for(int inp=0;inp<m_num_inports;inp++)
	{
		for(int inv=0;inv<m_num_vcs;inv++)
		{
			get_invc_priority(inp,inv);
		}
	}

	#endif

	#if USE_LRC==1
		//NOTE not at the beginning of the function otherwise some B and T flits
		//can participate SA in the same cycle they are written but this is not
		//correct.
		std::vector<InputUnit_d *>& ip_vect = m_router->get_inputUnit_ref();
		for(int i=0;i<ip_vect.size();i++)
			ip_vect[i]->wakeup_LRC_BW();

	//	// directly ask for a VA stage in this cycle when everybody has been
	//	// written
	//	if(m_router->calledVAthisCycle<curTick())
	//	{
	//		(m_router->getVCallocatorUnit())->wakeup();
	//		m_router->calledVAthisCycle=curTick(); //call va once per cycle
	//	}
	#endif
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
		DPRINTF(RubyPowerGatingSA, "@%lld Router_%d->SA WAIT FOR SWITCH SW_POWER_STATE %d NEXT_SW_STATE %d cycle %lld\n ",
			m_router->curCycle(),m_router->get_id(),m_router->getPowerStateSW(0), m_router->getNextPowerStateSW(0));
		scheduleEvent(1);
	}
	else
	{
		/*
		check if a single outlink is busy and evanutally stall the pipeline:
		easier to implement router looses performance. A performance oriented
		method avoids sa on busy out links only.
		*/
	  #if USE_SLOW_SWITCH==1
		bool is_one_out_busy=false;
  	#endif
		for(int i=0;i<m_output_unit.size();i++)
		{
            Resynchronizer *r=m_output_unit.at(i)->getOutLink_d()->getReshnchronizer();
            if(r->isAcknowledgeAvailable()) 
				r->clearAcknowledge();
			m_outport_busy[i]=false;
			if(r->isAcknowkedgePending())
			{
        r->advanceTransmitSide();
        #if USE_SLOW_SWITCH==1
				is_one_out_busy=true;
        #endif
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
        #if USE_SLOW_SWITCH==1
				is_one_out_busy=true;
		    #endif
        m_inport_credit_busy[i]=true;
			}
		}
	#if USE_SLOW_SWITCH==1
		if(is_one_out_busy==true)
		{/*stall router pipeline if one outlink busy*/
			scheduleEvent(1);
		}
		else
		{
	#endif
			// check to release reserved paths
			#if USE_WH_VC_SA == 1
			#if USE_RELEASE_PATH_OPT == 1
			checkAndReleaseReservedPath();
			#endif
			#endif
			// standard sa arbitration stage
			arbitrate_inports(); // First stage of allocation
			arbitrate_outports(); // Second stage of allocation

			clear_request_vector();
			check_for_wakeup();
	#if USE_SLOW_SWITCH==1
		}
	#endif
	}

	#if USE_LRC==1
		// ask for a VA stage in this cycle only after sa to allow
		// non-speculative request to be allocated first
		if(m_router->calledVAthisCycle<curTick())
		{
			(m_router->getVCallocatorUnit())->wakeup();
			m_router->calledVAthisCycle=curTick(); //call va once per cycle
		}
	#endif


}

void
SWallocator_d::get_invc_priority(int inport_iter, int invc_iter)                //method that get priority for [input port][input vc] 
{

        int invc_prio = m_input_unit[inport_iter]->get_vc_priority(invc_iter);
        m_priority_invc[inport_iter][invc_iter]=invc_prio;
}

std::deque<std::pair<int,int>>
SWallocator_d::get_priority_vector()
{
std::deque<std::pair<int,int>> priorities;
int max_prio=0;
        for(int inport=0;inport<m_num_inports;inport++)
        {
                for(int invc=0;invc<m_num_vcs;invc++)
                {
                        if(m_priority_invc[inport][invc]>=max_prio) max_prio=m_priority_invc[inport][invc];
                }
        }

for(int i=max_prio;i>=0;i--)
{
        for(int inport=0;inport<m_num_inports;inport++)
        {
                for(int invc=0;invc<m_num_vcs;invc++)
                {
                        if(m_priority_invc[inport][invc]==i&&(std::find(priorities.begin(),priorities.end(),std::pair<int,int>(inport,invc))==priorities.end()))
                        {
                        priorities.push_back(std::pair<int,int>(inport,invc));
                        }
                }
        }

}

return priorities;
}


void
SWallocator_d::checkAndReleaseReservedPath()
{
    for(int i = 0; i < m_num_inports; i++) 
        for(int j=0; j<m_router->get_num_vcs();j++)
            for(int q = 0; q < m_num_outports; q++) 
                if(m_wh_path_busy[i][j][q] == true)
                {
                    int outvc_test = m_input_unit[i]->get_outvc(j);
                    if(!m_input_unit[i]->need_stage(j, ACTIVE_, SA_,m_router->curCycle()) || m_output_unit[q]->get_credit_cnt(outvc_test)==0)
                    {// release path if no flit for sa in ibuf OR no credit in the corresponding (OP,OutVC)
                        m_wh_path_busy[i][j][q] = false;
                    }	
                }
}

void
SWallocator_d::arbitrate_inports()
{
	#if DEBUG_WH_VC_SA==1
		#if	USE_WH_VC_SA==0
			panic("\n\nDEBUG_WH_VC_SA==1 while the strategy is not active USE_WH_VC_SA == 0\n\n");
		#endif
		for(int i = 0; i < m_num_inports; i++) 
		{
			int reserved_on_each_input=0;
			for(int j=0; j<m_router->get_num_vcs();j++)
				for(int q = 0; q < m_num_outports; q++) 
					if(m_wh_path_busy[i][j][q] == true)
						reserved_on_each_input++;
			if(reserved_on_each_input>1)
				panic("\n\nDEBUG_WH_VC_SA - multiple path reserved for the same input port\n\n");
		}
		for(int i = 0; i < m_num_outports; i++) 
		{
			int reserved_on_each_output=0;
			for(int j=0; j<m_router->get_num_vcs();j++)
				for(int q = 0; q < m_num_inports; q++) 
					if(m_wh_path_busy[i][j][q] == true)
						reserved_on_each_output++;
			if(reserved_on_each_output>1)
				panic("\n\nDEBUG_WH_VC_SA - multiple path reserved for the same output port\n\n");
		}
	#endif
#if PRIORITY_SCHED == 1
std::deque<std::pair<int,int>> priorities=get_priority_vector();

for(int prio=0;prio<priorities.size();prio++)
{
	int inport = priorities[prio].first;
	
	 if(m_inport_credit_busy[inport])
                {// fast switch implementation
                        continue;
                }

	int invc = priorities[prio].second;

if (!((m_router->get_net_ptr())->validVirtualNetwork(get_vnet(invc))))
                continue;
                        int outvc_test = m_input_unit[inport]->get_outvc(invc);
                        int outport_test = m_input_unit[inport]->get_route(invc);
                //Speculative statistics: SA request not satisfied because no credit in outport
                if (m_input_unit[inport]->need_stage(invc, ACTIVE_, SA_, m_router->curCycle()) && m_output_unit[outport_test]->get_credit_cnt(outvc_test)==0)
                {
                         Router_d::incSaNoCredGlobalCountStatic();
                         m_router->incSaNoCredLocalCount();
                }
        
                if (m_input_unit[inport]->need_stage(invc, ACTIVE_, SA_, m_router->curCycle()) && m_output_unit[outport_test]->get_credit_cnt(outvc_test)>0)
                { 
                        if (is_candidate_inport(inport, invc))
                        {
                                //Speculative statistics
                                 Router_d::incSaGlobalCountStatic();
                                 m_router->incSaLocalCount();

                                int outport = m_input_unit[inport]->get_route(invc);

                                
                        m_local_arbiter_activity++;
                        m_local_arbiter_activity_resettable++;
                        m_port_req[outport][inport] = true;

                                //        m_round_robin_inport[inport]=invc;

                        m_vc_winners[outport][inport]= invc;
                        break; // got one vc winner for this port
                        }//end if
                }

}

#else
    // First do round robin arbitration on a set of input vc requests
    for (int inport = 0; inport < m_num_inports; inport++) 
	{
		if(m_inport_credit_busy[inport])
		{// fast switch implementation
			continue;
		}
        
		int invc = m_round_robin_inport[inport];

        for (int invc_iter = 0; invc_iter < m_num_vcs; invc_iter++) 
		{
            invc++;
            if (invc >= m_num_vcs)
                invc = 0;

            if (!((m_router->get_net_ptr())->validVirtualNetwork(get_vnet(invc))))
                continue;
			int outvc_test = m_input_unit[inport]->get_outvc(invc);
			int outport_test = m_input_unit[inport]->get_route(invc);
		//Speculative statistics: SA request not satisfied because no credit in outport
		if (m_input_unit[inport]->need_stage(invc, ACTIVE_, SA_, m_router->curCycle()) && m_output_unit[outport_test]->get_credit_cnt(outvc_test)==0) 
      		{
			 Router_d::incSaNoCredGlobalCountStatic();
			 m_router->incSaNoCredLocalCount();
		}
	#if USE_VICHAR==1
		if (m_input_unit[inport]->need_stage(invc, ACTIVE_, SA_, m_router->curCycle()) && m_output_unit[outport_test]->getAvailSlotPerVnet(outvc_test/m_vc_per_vnet)>0)
	#else
		if (m_input_unit[inport]->need_stage(invc, ACTIVE_, SA_, m_router->curCycle()) && m_output_unit[outport_test]->get_credit_cnt(outvc_test)>0)
	#endif
      		//if (m_input_unit[inport]->need_stage(invc, ACTIVE_, SA_, m_router->curCycle()) && m_input_unit[inport]->has_credits(invc)) 
      		{	
			if (is_candidate_inport(inport, invc)) 
			{
				//Speculative statistics
				 Router_d::incSaGlobalCountStatic();
				 m_router->incSaLocalCount();
				
				int outport = m_input_unit[inport]->get_route(invc);

				#if USE_WH_VC_SA==1
				// check to use inport for a reserved path at most
      		 		bool reserved = false;
      		 		for(int j=0; j<m_router->get_num_vcs();j++)
      		 			for(int i = 0; i < m_num_outports; i++) 
      		 				if ( (j!=invc || i!=outport) && m_wh_path_busy[inport][j][i] == true) 
      		 				{
      		 					reserved = true;
      		 					break;
      		 				}
      		 		if (reserved) 
      		 		{
      		 			continue;
      		 		}
      		 	#endif

      		        m_local_arbiter_activity++;
                        m_local_arbiter_activity_resettable++;
      		        m_port_req[outport][inport] = true;
	
					m_round_robin_inport[inport]=invc;

      		    	m_vc_winners[outport][inport]= invc;
      		    	break; // got one vc winner for this port
      			}//end if
      		}
        }
    }
#endif
}

bool
SWallocator_d::is_candidate_inport(int inport, int invc)
{
    int outport = m_input_unit[inport]->get_route(invc);
    int t_enqueue_time = m_input_unit[inport]->get_enqueue_time(invc);
    int t_vnet = get_vnet(invc);
    int vc_base = t_vnet*m_vc_per_vnet;
    if ((m_router->get_net_ptr())->isVNetOrdered(t_vnet)) {
        for (int vc_offset = 0; vc_offset < m_vc_per_vnet; vc_offset++) {
            int temp_vc = vc_base + vc_offset;
            if (m_input_unit[inport]->need_stage(temp_vc, ACTIVE_, SA_,
                                                 m_router->curCycle()) &&
               (m_input_unit[inport]->get_route(temp_vc) == outport) &&
               (m_input_unit[inport]->get_enqueue_time(temp_vc) <
                    t_enqueue_time)) {
                return false;
                break;
            }
        }
    }
    return true;
}


void
SWallocator_d::arbitrate_outports()
{
	#if DEBUG_WH_VC_SA
		std::vector<bool> released_path_input;
		std::vector<bool> released_path_output;

		released_path_input.resize(m_num_inports);		
		for(int i=0;i<m_num_inports;i++)
			released_path_input[i]=false;
		released_path_output.resize(m_num_outports);
		for(int j=0;j<m_num_outports;j++)
			released_path_output[j]=false;
	#endif
    // Now there are a set of input vc requests for output vcs.
    // Again do round robin arbitration on these requests
		
#if PRIORITY_SCHED ==1


std::deque<std::pair<int,int>> priorities=get_priority_vector();
/**
for(int p=0;p<priorities.size();p++)
{
	std::cout<<"<inport,invc>:"<<priorities[p].first<<","<<priorities[p].second<<"with prio:"<<m_priority_invc[priorities[p].first][priorities[p].second]<<std::endl;
}
**/
for (int prio = 0; prio < priorities.size(); prio++) 
	{
		
        int inport = priorities[prio].first;

        for (int outport = 0; outport < m_num_outports; outport++) {
   
		if(m_outport_busy[outport])
                {// fast switch constraint
                        continue;
                }

            // inport has a request this cycle for outport:
            if (m_port_req[outport][inport]) 
			{
                m_port_req[outport][inport] = false;
                int invc = m_vc_winners[outport][inport];
                int outvc = m_input_unit[inport]->get_outvc(invc);
				// update round_robin_information for arbiters//
				

                // remove flit from Input Unit
                flit_d *t_flit = m_input_unit[inport]->getTopFlit(invc);

				// DEBUG PRINT PIPELINE STAGE			
				m_router->print_pipeline_state(t_flit, "SW WON");		

                t_flit->advance_stage(ST_, m_router->curCycle());
                t_flit->set_vc(outvc);
                t_flit->set_outport(outport);
                t_flit->set_time(m_router->curCycle() + 1);
	//	std::cout<<"granted to traverse switch inport:"<<inport<<" vc:"<<invc<<std::endl;
                m_output_unit[outport]->decrement_credit(outvc);
                m_router->update_sw_winner(inport, t_flit);
                m_global_arbiter_activity++;
                m_global_arbiter_activity_resettable++;
				
		bool free_signal_fast = true;
							
				if ((t_flit->get_type() == TAIL_) || t_flit->get_type() == HEAD_TAIL_) 
				{
				/*NON_ATOMIC_VC_ALLOC
					
					since the VA is the only stage that can trigger the non
					atomic vc allocation it is same to update the variables
					here.
				*/
					if(m_output_unit[outport]->get_vc_counter_pkt(outvc, m_router->curCycle()) > 0 )
					{
						//TODO move to VA : set VNETID-novnetid && m_output_unit[outport]->dec_vc_counter_pkt(outvc);
						m_output_unit[outport]->set_vc_busy_to_vnet_id(outvc, t_flit->get_vnet());
					}
	
				assert(m_input_unit[inport]->isReady(invc, m_router->curCycle()) == false);
				m_input_unit[inport]->set_vc_state(IDLE_, invc, m_router->curCycle());
	
				m_input_unit[inport]->increment_credit(invc, true, free_signal_fast,m_router->curCycle());
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
				    m_input_unit[inport]->increment_credit(invc, false, free_signal_fast,m_router->curCycle());
                }
                break; // got a in request for this outport
            }
        }
    }		
#else
    for (int outport = 0; outport < m_num_outports; outport++) 
	{
		if(m_outport_busy[outport])
		{// fast switch constraint
			continue;
		}
        int inport = m_round_robin_outport[outport];

        for (int inport_iter = 0; inport_iter < m_num_inports; inport_iter++) {
            inport++;
            if (inport >= m_num_inports)
                inport = 0;

            // inport has a request this cycle for outport:
            if (m_port_req[outport][inport]) 
			{
			#if USE_WH_VC_SA
			// check if multiple input wants the same eventually reserved output
				bool check_reserved_out=false;
				for(int i=0;i<m_num_inports;i++)
					for(int j=0;j<m_router->get_num_vcs();j++)
						if(m_wh_path_busy[i][j][outport]==true && (inport!=i || m_vc_winners[outport][inport]!=j))
						{
							check_reserved_out=true;
							break;
						}
				if(check_reserved_out==true)
					continue;
			#endif


                m_port_req[outport][inport] = false;
                int invc = m_vc_winners[outport][inport];
                int outvc = m_input_unit[inport]->get_outvc(invc);
				// update round_robin_information for arbiters//
				m_round_robin_outport[outport]=inport;

				//assert(m_input_unit[inport]->has_credits(invc));
			#if USE_VICHAR==1
				assert(m_output_unit[outport]->getAvailSlotPerVnet(outvc/m_vc_per_vnet)>0);
			#endif

                // remove flit from Input Unit
                flit_d *t_flit = m_input_unit[inport]->getTopFlit(invc);

				// DEBUG PRINT PIPELINE STAGE			
				m_router->print_pipeline_state(t_flit, "SW WON");		

                t_flit->advance_stage(ST_, m_router->curCycle());
                t_flit->set_vc(outvc);
                t_flit->set_outport(outport);
                t_flit->set_time(m_router->curCycle() + 1);

				#if DEBUG_CREDIT_RTT == 1	
				if( m_router->get_id()==0 && outvc == 0 && outport==3 )
						std::cout<<"@"<<curTick()<<" R"<<m_router->get_id() <<"->SA"
							<<" outvc"<<outvc<<" CREDIT USED"<<std::endl;
				#endif
                m_output_unit[outport]->decrement_credit(outvc);
                m_router->update_sw_winner(inport, t_flit);
                m_global_arbiter_activity++;
                m_global_arbiter_activity_resettable++;
				
		bool free_signal_fast = true;
			#if USE_WH_VC_SA==1
				// set the reserved path for the winner, it could be useless,
				// since it is done flit by flit on a possible already active
				// reserved path
				m_wh_path_busy[inport][invc][outport] = true;

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
					&& ( m_wh_path_busy[inport][invc][outport] == true/*path reserved*/)
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
				/*NON_ATOMIC_VC_ALLOC
					
					since the VA is the only stage that can trigger the non
					atomic vc allocation it is same to update the variables
					here.
				*/
					if(m_output_unit[outport]->get_vc_counter_pkt(outvc, m_router->curCycle()) > 0 )
					{
						//TODO move to VA : set VNETID-novnetid && m_output_unit[outport]->dec_vc_counter_pkt(outvc);
						m_output_unit[outport]->set_vc_busy_to_vnet_id(outvc, t_flit->get_vnet());
					}
				
				#if USE_WH_VC_SA==1
					// deassert the reserved path if this is the tail flit
						m_wh_path_busy[inport][invc][outport] = false;

					#if DEBUG_WH_VC_SA
						released_path_input[inport]=true;
						released_path_output[outport]=true;
					#endif
				#endif

				#if DEBUG_CREDIT_RTT == 1	
					if( m_router->get_id()==1 && invc == 0 && inport==2 )
						std::cout<<"@"<<curTick()<<" R"<<m_router->get_id() <<"->SA"
							<<" invc"<<invc<<" CREDIT SENT BACK"<<std::endl;
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
						//Setting for vc reuse statistics cutTick in vc
						m_input_unit[inport]->setReuseCycleVc(invc, m_router->curCycle()); 
						//m_input_unit[inport]->set_enqueue_time(invc, INFINITE_);
					}
					m_input_unit[inport]->get_reused_remap_vc_usable()[invc]=false;
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
							
							  //BUBBLE Checking	
								Router_d::incBubbleTotalCountStatic();
								checkBubbleNextCycle(inport,invc);
						}
					#else	
						assert(m_input_unit[inport]->isReady(invc, m_router->curCycle()) == false);
						m_input_unit[inport]->set_vc_state(IDLE_, invc, m_router->curCycle());
					#endif
				#endif
					m_input_unit[inport]->increment_credit(invc, true, free_signal_fast,m_router->curCycle());
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
				    m_input_unit[inport]->increment_credit(invc, false, free_signal_fast,m_router->curCycle());
                }
                break; // got a in request for this outport
            }
        }
    }
#endif
	#if DEBUG_WH_VC_SA
	// check if some inport/outport pairs could be used but aren't 
		std::vector<int> used_input_path;
		used_input_path.resize(m_num_inports);
		for(int i=0;i<m_num_inports;i++)
		{
			used_input_path[i]=0;
			for(int j=0;j<m_num_outports;j++)	
				for(int q=0; q<m_router->get_num_vcs();q++)
					if(m_wh_path_busy[i][q][j] == true)
						used_input_path[i]++;
		}
	
		std::vector<int> used_output_path;
		used_output_path.resize(m_num_outports);
		for(int i=0;i<m_num_outports;i++)
		{	
			used_output_path[i]=0;
			for(int j=0;j<m_num_inports;j++)	
				for(int q=0; q<m_router->get_num_vcs();q++)
					if(m_wh_path_busy[j][q][i] == true)
						used_output_path[i]++;
		}
		
		for(int i=0;i<m_num_inports;i++)
			for(int j=0;j<m_num_outports;j++)
			{
				// 0, 1 reserved pat
				assert(used_input_path[i]<=1 && used_input_path[i]>=0 );
				assert(used_output_path[j]<=1 && used_output_path[j]>=0 );

				// no required paths not assigned, eventually deallocated this cycle
				std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()<<" "<<i<<"->"<<j<<" "
					<<released_path_input[i]<<" "<<released_path_output[j]<<" in_busy?"<<used_input_path[i]<<" out_busy?"<<used_output_path[j] 
					<<"\t"<<released_path_input[i]+released_path_output[j]+used_input_path[i]+used_output_path[j] <<">="<<m_port_req[j][i]<<std::endl;

				assert(released_path_input[i]+released_path_output[j]+used_input_path[i]+used_output_path[j] >= m_port_req[j][i]);
			}
	#endif
}

void
SWallocator_d::check_for_wakeup()
{
    for (int i = 0; i < m_num_inports; i++) {
        for (int j = 0; j < m_num_vcs; j++) {
            if (m_input_unit[i]->need_stage_nextcycle(j, ACTIVE_, SA_,
                                                      m_router->curCycle())) {
                scheduleEvent(1);
                return;
            }
        }
    }
}

int
SWallocator_d::get_vnet(int invc)
{
    int vnet = invc/m_vc_per_vnet;
    assert(vnet < m_router->get_num_vnets());
    return vnet;
}

void
SWallocator_d::clear_request_vector()
{
    for (int i = 0; i < m_num_outports; i++) {
        for (int j = 0; j < m_num_inports; j++) {
            m_port_req[i][j] = false;
        }
    }
}

void
SWallocator_d::checkBubbleNextCycle(int inport, int invc)
{
				bool find=false;
        for (int j = 0; j < m_num_vcs; j++) {
            if (m_input_unit[inport]->need_stage_nextcycle(j, ACTIVE_, SA_,
                                                m_router->curCycle()) && invc!=j) {
								Router_d::incBubbleNoDegradationStatic();
                find=true;
								break;
            }
        }
				if(find == false){
								Router_d::incBubbleDegradationStatic();
				}
}
