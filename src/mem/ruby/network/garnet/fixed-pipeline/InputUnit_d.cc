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
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_d.hh"

#if USE_ADAPTIVE_ROUTING == 0
	#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_d.hh"
#else
	#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_ADAPTIVE_d.hh"
#endif
#define PRIORITY_SCHED 1


using namespace std;
using m5::stl_helpers::deletePointers;

InputUnit_d::InputUnit_d(int id, Router_d *router) : Consumer(router)
{
    m_id = id;
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_vc_per_vnet = m_router->get_vc_per_vnet();

    m_num_buffer_reads.resize(m_num_vcs/m_vc_per_vnet);
    m_num_buffer_writes.resize(m_num_vcs/m_vc_per_vnet);
    m_num_buffer_reads_resettable.resize(m_num_vcs/m_vc_per_vnet);
    m_num_buffer_writes_resettable.resize(m_num_vcs/m_vc_per_vnet);
    for (int i = 0; i < m_num_buffer_reads.size(); i++) {
        m_num_buffer_reads[i] = 0;
        m_num_buffer_writes[i] = 0;
        m_num_buffer_reads_resettable[i] = 0;
        m_num_buffer_writes_resettable[i] = 0;
    }

    creditQueue = new flitBuffer_d();
    // Instantiating the virtual channels
    m_vcs.resize(m_num_vcs);
	m_reused_remap_vc_usable.resize(m_num_vcs);
	m_reused_remap_vc_used.resize(m_num_vcs);
	m_reused_remap_vc_outport.resize(m_num_vcs);
	m_reused_remap_vc_outvc.resize(m_num_vcs);
    m_reused_remap_vc_outport_rc_cycle.resize(m_num_vcs);
	m_reused_remap_vc_outvc_va_cycle.resize(m_num_vcs);

	m_reused_remap_vc_fake_free_sig.resize(m_num_vcs);

	m_buf_is_on.resize(m_num_vcs);

	//used to track buf time utilization
	timestamp_last_used_buf.resize(m_num_vcs);

	for (int i=0; i < m_num_vcs; i++)
	{
        m_vcs[i] = new VirtualChannel_d(i, m_router->curCycle());

	//	if(i<m_num_vcs-(m_router->get_net_ptr())->getAdaptiveRoutingNumEscapePaths())
		if((i%m_vc_per_vnet)<(m_vc_per_vnet-(m_router->get_net_ptr())->getAdaptiveRoutingNumEscapePaths()))
			m_vcs[i]->set_is_adaptive(1);
		else
			m_vcs[i]->set_is_adaptive(0);
	#if USE_ADAPTIVE_ROUTING==1
		std::cout<<"Inport"<<m_id<<" vc["<<i<<"] adaptive?"<<m_vcs[i]->get_is_adaptive()<<std::endl;
	#endif

		// initialize the remapping logic (NOTE -1 means remap position not used)
		m_vc_remap[i]=-1;
		m_reused_remap_vc_usable[i]=false;
		m_reused_remap_vc_used[i]=false;
		m_reused_remap_vc_outport[i]=-1;
		m_reused_remap_vc_outvc[i]=-1;
		m_reused_remap_vc_outport_rc_cycle[i]=Cycles(0);
		m_reused_remap_vc_outvc_va_cycle[i]=Cycles(0);
		m_reused_remap_vc_fake_free_sig[i]=false;
		timestamp_last_used_buf[i]=1;

		// information on buffer usage
		m_buf_is_on[i]=true;

    }
	// NON_ATOMIC_VC_ALLOC ////////////////////////////
	m_isLastFlitTail.resize(m_num_vcs);
	m_vnetIdLastFlitTail.resize(m_num_vcs);
	m_outportTempQueue.resize(m_num_vcs);
	for(int i=0;i<m_num_vcs;i++)
	{
		m_isLastFlitTail.at(i)=true; //buffer is empty
		m_vnetIdLastFlitTail.at(i)=-1; //no vnet selected all are ok to be stored here
		m_outportTempQueue.at(i)=new std::queue<int>();
	}
	///////////////////////////////////////////////////

    this->setEventPrio(Event::NOC_InputUnit_Pri);


	#if USE_VICHAR==1
		//init total used credit
		usedSlotPerVnet.resize(m_router->get_num_vnets());
		for(int i=0;i<m_router->get_num_vnets();i++)
			usedSlotPerVnet.at(i)=0;
	#endif

	#if PRIORITY_SCHED == 1
		m_last_max_priority_vc = -1;
	#endif
}

InputUnit_d::~InputUnit_d()
{
    delete creditQueue;
    deletePointers(m_vcs);
}

#if USE_LRC == 1
/*
 * This function resembles the wakeup, while it is directly called by SA if the
 * LRC is implemented. Basically, it does the same work of the original wakeup
 * function, while working on time, i.e. set state for both buffer and flit at
 * curCycle-1, allows to invoke both VA and SA at the same clock cycle.
 * Such implementation completely differs from the HW one, since events are
 * executed in backward order in one cycle the BW is the last visited stage!!!
*/
void
InputUnit_d::wakeup_LRC_BW()
{
    flit_d *t_flit;
    Resynchronizer *r=m_in_link->getReshnchronizer();
    if (m_in_link->isReady(m_router->curCycle()) && r->isRequestAvailable())
	{
        r->clearRequest();
        r->sendAcknowledge();
        t_flit = m_in_link->consumeLink();
		// get the vc, eventually to be remapped
        int vc = t_flit->get_vc();

		//SPECULATIVE Follow packet check Out
	std::vector<InputUnit_d *>& in = m_router->get_inputUnit_ref();
	NetworkLink_d* link = in[m_id]->getInLink_d();
	NetworkInterface_d* ni = dynamic_cast<NetworkInterface_d*>(link->getLinkSource());
	if(ni)
	{// compute packet router time
		t_flit->initHop();
		t_flit->setUniqueId();
		m_router->print_route_time(t_flit, "IN_NETWORK");
		//Add flit packet tick latency tracker
		t_flit->startTracker(curTick());

	}else{
	  t_flit->addHop();
  }

	m_router->print_pipeline_state(t_flit, "LRC");

	if ((t_flit->get_type() == HEAD_) || (t_flit->get_type() == HEAD_TAIL_))
	{
        m_router->add_head_and_tail_count();
        if(t_flit->get_type() == HEAD_TAIL_)
		{
            m_router->add_only_headtail_count();
        }

		#if USE_VC_REMAP==1
		  #if USE_NON_ATOMIC_VC_ALLOC==1
			assert(false && "Impossible to use USE_VC_REMAP and USE_NON_ATOMIC_VC_ALLOC");
		  #endif
				//actual remap policy
				//std::cout<<"@"<<curTick()<<" R"<< m_router->get_id()
				//		<<" iu"<<m_id<<" vc"<<vc
				//		<<" HEAD?"<<((t_flit->get_type() == HEAD_)?1:0)
				//		<<" HEAD_TAIL_?"<<((t_flit->get_type() == HEAD_TAIL_)?1:0)
				//		<<"  ";
			int new_remap_vc=-1;
		  #if USE_VNET_REUSE==0
			new_remap_vc = vcRemapPolicy(vc); // get the remapped vc
		  #else // using both VNET_REUSE and VC_REMAP
	  		new_remap_vc = vcRemapPolicyReuseVnet(vc,t_flit); // get the remapped vc reuse vnet
		  #endif
			assert(new_remap_vc!=-1);
			vc=new_remap_vc; // use this as vc

		#else
		  #if USE_NON_ATOMIC_VC_ALLOC==0
			if(m_vcs[vc]->get_state() != IDLE_)
				std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()
					<<" inport"<<m_id<<" vc"<<vc
					<<" is_adaptive "<<getIsAdaptive(vc)
					<<std::endl;
            	assert(m_vcs[vc]->get_state() == IDLE_);
		  #endif
		#endif

			if(m_vcs[vc]->get_state() == IDLE_)
			{//this checks for reuse if not idle means others packets are in the buffer thus using the old counter
				timestamp_last_used_buf[vc]=curTick();
			}

			// update the vnet on the input buffer every time an head/head-tail arrives
			assert(t_flit->get_vnet()!=-1);
			m_vcs[vc]->set_real_vnet_used(t_flit->get_vnet());

            // Do the route computation for this vc in LRC way thus VA is
            // required in this cycle

			// LRC CHANGES HERE in time curCycle()-1
            m_router->route_req(t_flit, this, vc);
            m_vcs[vc]->set_enqueue_time(m_router->curCycle()-1); // -1 allows to ask for VA in this cycle

		/*congestion metrics starting cycle evaluation*/
			Router_d::CongestionMetrics&
			cm1=m_router->getC1Matrix().at(m_id*m_num_vcs+vc).at(m_vcs[vc]->get_route());

			//assert(cm1.t_start==Cycles(-1));
			cm1.t_start=m_router->curCycle();

			Router_d::CongestionMetrics&
			cm2=m_router->getC2Matrix().at(vc).at(m_vcs[vc]->get_route());
			//assert(cm2.t_start==Cycles(-1));
			cm2.t_start=m_router->curCycle();

			// increment incoming flits by 1 to the specific outport
			m_router->incrementInPkt(m_vcs[vc]->get_route());
		/*end congestion*/
        }
		else
		{
			#if USE_VC_REMAP==1
				#if USE_NON_ATOMIC_VC_ALLOC==1
				assert(false&& "\n\nImpossible to use USE_VC_REMAP and USE_NON_ATOMIC_VC_ALLOC\n\n");
		  		#endif
				vc=m_vc_remap[vc];
			#endif
			// LRC CHANGES HERE
            t_flit->advance_stage(SA_, m_router->curCycle()-1);
            m_router->swarb_req_LRC();

			// increment incoming flits by 1 to the specific outport
			m_router->incrementInPkt(m_vcs[vc]->get_route());

        }

        // write flit into input buffer and update the real vnet (for vnet reuse optim)
        m_vcs[vc]->insertFlit(t_flit);
	#if USE_VICHAR==1
		incrUsedSlotPerVnet(vc/m_vc_per_vnet);

	//	std::vector<int> testVect;
	//	testVect.resize(m_router->get_num_vnets());
	//	for(int i=0;i<testVect.size();i++)
	//		testVect[i]=0;
	//	for(int i=0;i<m_num_vcs;i++)
	//	{
	//		testVect[i/]
	//	}

		assert(usedSlotPerVnet.at(vc/m_vc_per_vnet)<=m_router->getTotVicharSlotPerVnet());
	#endif
		// NON_ATOMIC_VC_ALLOC

		//checks
		if( (t_flit->get_type() == HEAD_) || (t_flit->get_type() == HEAD_TAIL_))
			assert(m_isLastFlitTail.at(vc)==true);
		if(  (t_flit->get_type() == BODY_) )
			assert(m_isLastFlitTail.at(vc)==false);

		if ((t_flit->get_type() == TAIL_) || (t_flit->get_type() == HEAD_TAIL_))
		{//is the last flit of a packet?
			m_isLastFlitTail.at(vc)=true;
			m_vnetIdLastFlitTail.at(vc)=t_flit->get_vnet();
			assert(m_vnetIdLastFlitTail.at(vc)>=0&& m_vnetIdLastFlitTail.at(vc)<m_router->getVirtualNetworkProtocol());
		}
		else
		{// the current pkt is not ended, yet. WAIT before non_atomic_alloc
			m_isLastFlitTail.at(vc)=false;
			m_vnetIdLastFlitTail.at(vc)=-1;
		}

		//debug///////////////////////////////////
		int flit_buf_size = getBufferSize(vc);
		int flit_stored = getCongestion(vc);

		assert(flit_stored<=flit_buf_size);
		////////////////////////////////////////////
        int vnet = vc/m_vc_per_vnet;
        // number of writes same as reads
        // any flit that is written will be read only once
        m_num_buffer_writes[vnet]++;
        m_num_buffer_reads[vnet]++;
        m_num_buffer_writes_resettable[vnet]++;
        m_num_buffer_reads_resettable[vnet]++;
    }
    //Retry, you may have better luck next time
    if(r->isRequestPending())
    {
        r->advanceReceiveSide();
        scheduleEvent(1);
    }
}
#endif //USE_LRC==1
void
InputUnit_d::wakeup()
{
	#if USE_LRC==1
	assert(false);
//&&"\n\nIMPOSSIBLE TO CALL InputUnit_d::wakeup() USING LRC OPT.
//THE EQUIVALENT FUNCTION IS CALLED IN SA\n\n");
	#endif
    flit_d *t_flit;
    Resynchronizer *r=m_in_link->getReshnchronizer();
    if (m_in_link->isReady(m_router->curCycle()) && r->isRequestAvailable())  // if link è idle in questo momento e c'è una richiesta per questo link
	{
        r->clearRequest();
        r->sendAcknowledge();
        t_flit = m_in_link->consumeLink();
		// get the vc, eventually to be remapped
        int vc = t_flit->get_vc();

		//SPECULATIVE Follow packet check Out
	std::vector<InputUnit_d *>& in = m_router->get_inputUnit_ref();
	NetworkLink_d* link = in[m_id]->getInLink_d();
	NetworkInterface_d* ni = dynamic_cast<NetworkInterface_d*>(link->getLinkSource());
	if(ni)
	{// compute packet router time
		t_flit->initHop();
		t_flit->setUniqueId();
		m_router->print_route_time(t_flit, "IN_NETWORK");
		//Add flit packet tick latency tracker
		t_flit->startTracker(curTick());

	}else{
	  t_flit->addHop();
  }

	m_router->print_pipeline_state(t_flit, "BW");

	if ((t_flit->get_type() == HEAD_) || (t_flit->get_type() == HEAD_TAIL_))
	{
        m_router->add_head_and_tail_count();
        if(t_flit->get_type() == HEAD_TAIL_)
		{
            m_router->add_only_headtail_count();
        }

		#if USE_VC_REMAP==1
		  #if USE_NON_ATOMIC_VC_ALLOC==1
			assert(false&& "\n\nImpossible to use USE_VC_REMAP and USE_NON_ATOMIC_VC_ALLOC\n\n");
		  #endif
				//actual remap policy
				//std::cout<<"@"<<curTick()<<" R"<< m_router->get_id()
				//		<<" iu"<<m_id<<" vc"<<vc
				//		<<" HEAD?"<<((t_flit->get_type() == HEAD_)?1:0)
				//		<<" HEAD_TAIL_?"<<((t_flit->get_type() == HEAD_TAIL_)?1:0)
				//		<<"  ";
			int new_remap_vc=-1;
		  #if USE_VNET_REUSE==0
			new_remap_vc = vcRemapPolicy(vc); // get the remapped vc
		  #else // using both VNET_REUSE and VC_REMAP
	  		new_remap_vc = vcRemapPolicyReuseVnet(vc,t_flit); // get the remapped vc reuse vnet
		  #endif
			assert(new_remap_vc!=-1);
			vc=new_remap_vc; // use this as vc

		#else		//end USE_vC_REMAP = TRUE
		  #if USE_NON_ATOMIC_VC_ALLOC==0
			if(m_vcs[vc]->get_state() != IDLE_)
				std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()
					<<" inport"<<m_id<<" vc"<<vc
					<<" is_adaptive "<<getIsAdaptive(vc)
					<<std::endl;
            	assert(m_vcs[vc]->get_state() == IDLE_);
		  #endif
		#endif		//END USE_VC_REMAP = FALSE

			if(m_vcs[vc]->get_state() == IDLE_)
			{//this checks for reuse if not idle means others packets are in the buffer thus using the old counter
				timestamp_last_used_buf[vc]=curTick();
				assert(m_vcs[vc]->get_prio_latency()==0);
			}

			// update the vnet on the input buffer every time an head/head-tail arrives
			assert(t_flit->get_vnet()!=-1);
			m_vcs[vc]->set_real_vnet_used(t_flit->get_vnet());

            // Do the route computation for this vc
            m_router->route_req(t_flit, this, vc);
            m_vcs[vc]->set_enqueue_time(m_router->curCycle());
	    #if PRIORITY_SCHED == 1
		//if(m_router->get_id()==5)std::cout<<"InputUnit "<<m_id<<": set priority for vc:"<<vc<<std::endl;
	    	m_vcs[vc]->set_prio_latency(t_flit->get_priority());

	    #endif
		//here I have to set the priority of the packet based on its hop count

		/*congestion metrics starting cycle evaluation*/
			Router_d::CongestionMetrics&
			cm1=m_router->getC1Matrix().at(m_id*m_num_vcs+vc).at(m_vcs[vc]->get_route());

			//assert(cm1.t_start==Cycles(-1));
			cm1.t_start=m_router->curCycle();

			Router_d::CongestionMetrics&
			cm2=m_router->getC2Matrix().at(vc).at(m_vcs[vc]->get_route());
			//assert(cm2.t_start==Cycles(-1));
			cm2.t_start=m_router->curCycle();

			// increment incoming flits by 1 to the specific outport
			m_router->incrementInPkt(m_vcs[vc]->get_route());
		/*end congestion*/
        }
		else  //se invece ho già un link occupato vuol dire che sto facendo passare i vari body flits
		{
			#if USE_VC_REMAP==1
				#if USE_NON_ATOMIC_VC_ALLOC==1
				assert(false&& "\n\nImpossible to use USE_VC_REMAP and USE_NON_ATOMIC_VC_ALLOC\n\n");
		  		#endif
				vc=m_vc_remap[vc];
			#endif

            t_flit->advance_stage(SA_, m_router->curCycle());	//infatti qui faccio avanzare il flit direttamente allo switch allocator senza computare la route
            m_router->swarb_req();

			// increment incoming flits by 1 to the specific outport
			m_router->incrementInPkt(m_vcs[vc]->get_route());

        }

        // write flit into input buffer and update the real vnet (for vnet reuse optim)
        m_vcs[vc]->insertFlit(t_flit);
	#if USE_VICHAR==1
		incrUsedSlotPerVnet(vc/m_vc_per_vnet);

	//	std::vector<int> testVect;
	//	testVect.resize(m_router->get_num_vnets());
	//	for(int i=0;i<testVect.size();i++)
	//		testVect[i]=0;
	//	for(int i=0;i<m_num_vcs;i++)
	//	{
	//		testVect[i/]
	//	}

		assert(usedSlotPerVnet.at(vc/m_vc_per_vnet)<=m_router->getTotVicharSlotPerVnet());
	#endif
		// NON_ATOMIC_VC_ALLOC

		//checks
		if( (t_flit->get_type() == HEAD_) || (t_flit->get_type() == HEAD_TAIL_))
			assert(m_isLastFlitTail.at(vc)==true);
		if(  (t_flit->get_type() == BODY_) )
			assert(m_isLastFlitTail.at(vc)==false);

		if ((t_flit->get_type() == TAIL_) || (t_flit->get_type() == HEAD_TAIL_))		//if it is the last flit of the packet being injected, I have to reset the priority of that vc
		{//is the last flit of a packet?
			m_isLastFlitTail.at(vc)=true;

			m_vnetIdLastFlitTail.at(vc)=t_flit->get_vnet();
			assert(m_vnetIdLastFlitTail.at(vc)>=0&& m_vnetIdLastFlitTail.at(vc)<m_router->getVirtualNetworkProtocol());
		}
		else
		{// the current pkt is not ended, yet. WAIT before non_atomic_alloc
			m_isLastFlitTail.at(vc)=false;
			m_vnetIdLastFlitTail.at(vc)=-1;
		}

		//debug///////////////////////////////////
		int flit_buf_size = getBufferSize(vc);
		int flit_stored = getCongestion(vc);

		assert(flit_stored<=flit_buf_size);
		////////////////////////////////////////////
        int vnet = vc/m_vc_per_vnet;
        // number of writes same as reads
        // any flit that is written will be read only once
        m_num_buffer_writes[vnet]++;
        m_num_buffer_reads[vnet]++;
        m_num_buffer_writes_resettable[vnet]++;
        m_num_buffer_reads_resettable[vnet]++;
    }
    //Retry, you may have better luck next time
    if(r->isRequestPending())
    {
        r->advanceReceiveSide();
        scheduleEvent(1);
    }
}

#if PRIORITY_SCHED == 1
int
InputUnit_d::get_vc_priority(int vc)
{
 return m_vcs[vc]->get_prio_latency();
}
#endif

int
InputUnit_d::getCongestion() const
{
    int result=0;
    for(int i=0;i<m_vcs.size();i++) result+=m_vcs[i]->getCongestion();
    return result;
}

void
InputUnit_d::increment_credit(int in_vc, bool free_signal, bool free_signal_fast, Time curTime)			//in this method I have to check if, is free signal, that my priority has been resetted.
{
       #if PRIORITY_SCHED == 1
	if(free_signal == true)
		{
			//if(m_router->get_id()==5) std::cout<<"InputUnit: reset priority latency for vc:"<<in_vc<<std::endl;
			 m_vcs[in_vc]->set_prio_latency(0);
		}
       #endif
	int original_vc=in_vc;
	#if USE_VICHAR==1
		decrUsedSlotPerVnet(original_vc/m_vc_per_vnet);
	#endif
	///// update the used buffer count in time ////
	//if(free_signal==1)
	//{
	//	if(m_reused_remap_vc_used[in_vc]==false)
	//	{
	//		assert(curTick()>timestamp_last_used_buf[in_vc]);
	//		assert(timestamp_last_used_buf[in_vc]>0);
	//		(m_router->getUsedBufInport())[m_id][in_vc]+= (curTick()-timestamp_last_used_buf[in_vc]);
	//		timestamp_last_used_buf[in_vc]=0;//curTick()+1;
	//		assert((m_router->getUsedBufInport())[m_id][in_vc]>0);
	//	}
	//}
	///////////////////////////////////////////////
	#if USE_NON_ATOMIC_VC_ALLOC==1
		#if USE_VC_REMAP ==1
			assert(false);
		#endif

	if( free_signal==true)
	{
		if(m_outportTempQueue.at(in_vc)->size()>0) //do post RC stuff suppose rc always succeed
		{
			//int temp_outvc=in_vc;
			int temp_outport=m_outportTempQueue.at(in_vc)->front();

			m_outportTempQueue.at(in_vc)->pop();
			updateRoute(in_vc,temp_outport,m_router->curCycle());

			if( !((m_vcs[in_vc]->peekTopFlit()->get_type() == HEAD_) ||
						(m_vcs[in_vc]->peekTopFlit()->get_type() == HEAD_TAIL_)) )
				std::cout<<m_vcs[in_vc]->peekTopFlit()->get_type()
						<<" flitStored:"<<getCongestion(in_vc)
						<<" outport:"<<temp_outport
						<<std::endl;

			assert( (m_vcs[in_vc]->peekTopFlit()->get_type() == HEAD_) ||
						(m_vcs[in_vc]->peekTopFlit()->get_type() == HEAD_TAIL_));

			m_vcs[in_vc]->peekTopFlit()->advance_stage(VA_,m_router->curCycle());
//			m_router->vcarb_req();
		}
		else
		{//this is the last one stored pkt
			m_vnetIdLastFlitTail.at(in_vc)=-1; //used in RC to see if multiple pkt are stored in this buffer
		}
		m_router->vcarb_req();
	}
	#endif
	///////////////////////////////////////////////

	#if USE_VC_REMAP == 1
		#if USE_NON_ATOMIC_VC_ALLOC==1
			assert(false);
		#endif

		assert(m_vc_remap_inverse.find(in_vc)!=m_vc_remap_inverse.end());
		original_vc=m_vc_remap_inverse[in_vc].front();
		assert(m_vc_remap[original_vc]!=-1);

		if(free_signal==true)
		{

			m_vc_remap_inverse[in_vc].pop_front();

			for(int ite=0;ite<m_vc_remap.size();ite++)
				if(m_vc_remap[ite]==in_vc)
				{
					bool flag_is_present=false;
					for(auto ite1=m_vc_remap_inverse[in_vc].begin();ite1!=m_vc_remap_inverse[in_vc].end();ite1++)
						if( (*ite1) == ite )
							flag_is_present=true;
					if(flag_is_present==false)
						m_vc_remap[ite]=-1;
				}

		///// update reuse logic on free_signal ////////////////
			if(m_reused_remap_vc_used[in_vc]==false)
			{//if not reused the vc reset the vnet
				m_vcs[in_vc]->set_real_vnet_used(-2);
			}
			else				// if(m_reused_remap_vc_used[in_vc]==true)
			{//vc reused
				int flit_stored = getCongestion(in_vc);

				assert(flit_stored>0); // the head of the new packet!!
				assert(m_vcs[in_vc]->peekTopFlit()->get_type()== HEAD_
						|| m_vcs[in_vc]->peekTopFlit()->get_type()== HEAD_TAIL_); //top flit is head or head_tail_ of the next packet

				int temp_outvc=m_reused_remap_vc_outvc[in_vc];
				int temp_outport=m_reused_remap_vc_outport[in_vc];
				if(m_reused_remap_vc_outport[in_vc]!=-1) //do post RC stuff
				{
					updateRoute(in_vc,temp_outport,m_router->curCycle());
					m_vcs[in_vc]->peekTopFlit()->advance_stage(VA_,m_reused_remap_vc_outport_rc_cycle[in_vc]);
					int temp_vnet_original=m_vcs[in_vc]->peekTopFlit()->get_vnet();


					std::vector<OutputUnit_d *>& ou_temp = m_router->get_outputUnit_ref();
					if( (m_reused_remap_vc_outvc[in_vc]!=-1) &&
							(ou_temp[temp_outport]->is_vc_idle(temp_outvc,m_router->curCycle())))
						// do post VA stuff, since precomputed vc is still available
					{
						grant_vc(in_vc,temp_outvc,m_router->curCycle()/*m_reused_remap_vc_outvc_va_cycle[in_vc]*/);
						ou_temp[temp_outport]->update_vc(  temp_outvc,m_id, in_vc);
						assert(temp_vnet_original!=-1);
						((m_router->getVCallocatorUnit())->getVnetInThisVcOutport())[temp_outport][temp_outvc]=temp_vnet_original;
						//update the vnet on the packet that is using the vc to
						//solve issue in the VA stage otherwise no vnet usage
						//is seen
						m_router->swarb_req(); // not sure if do this to save 1 cycle
					}
					else
					{
						//assert(false&&"supererror\n\n");
						m_router->vcarb_req();
					}
				}
				else
				{
					m_router->route_req(m_vcs[in_vc]->peekTopFlit(), this, in_vc);
				}
				// free all the temp resources since outvc and outport
				// eventually precomputed have been already used
				m_reused_remap_vc_used[in_vc]=false;
				m_reused_remap_vc_outport[in_vc]=-1;
				m_reused_remap_vc_outvc[in_vc]=-1;

			}
			//else
			//	m_vc_remap[original_vc]=-1;
			//std::cout<<"@"<<curTick()<<" FREE_VC after R"<<m_router->get_id()<<" "
			//		<<"iu"<<m_id<<" invc"<<in_vc<<" m_vc_remap["<<original_vc<<"] = "<<m_vc_remap[original_vc]
			//		<<" REUSED?"<<m_reused_remap_vc_used[in_vc] <<std::endl;
			//	activeRemap(std::cout);
		//#endif
		}
	#endif //#use_vc_remap==1


	flit_d *t_flit;

#if USE_VC_REUSE == 1
	#if USE_FAST_FREE_SIGNAL==0
		t_flit = new flit_d(original_vc/*in_vc*/, free_signal, curTime);
	#else
		t_flit = new flit_d(original_vc/*in_vc*/, free_signal_fast, curTime);
	#endif
#else
	t_flit = new flit_d(original_vc/*in_vc*/, free_signal, curTime);
#endif

    creditQueue->insert(t_flit);

	m_credit_link->getReshnchronizer()->sendRequest();
    m_credit_link->scheduleEvent(1);
}

int
InputUnit_d::vcRemapPolicy(int vc)
{
	// simple policy which remap the first vnet vc available
	int remap_vc = -1;
	int vnet = vc/m_vc_per_vnet;
	int init_vc = vnet*m_vc_per_vnet;
	int end_vc = (1+vnet)*m_vc_per_vnet;

	#if USE_VC_REUSE==1
		#if USE_NON_ATOMIC_VC_ALLOC==1
		assert(false && "\n\nIMPOSSIBLE TO USE NON_ATOMIC_VC_ALLOC WITH VC_REUSE\n\n\n");
		#endif
	for(int i=init_vc;i<end_vc;i++)
	{
		if(m_reused_remap_vc_usable[i]==true)
		{
			m_reused_remap_vc_used[i] = true;
			m_reused_remap_vc_usable[i] = false;
			remap_vc = i; //use it

		break;
		}
	}
	#endif
	if(remap_vc==-1)
	{
		for(int i=init_vc;i<end_vc;i++) //
		{
			if(m_vcs[i]->get_state() == IDLE_)
			{
				assert(getCongestion(i)==0);
				remap_vc=i;
				break;
			}
		}
	}
	if(remap_vc==-1)
	{
		std::cout<<"\n----------------------------------\n@"
				<<curTick()<<" R"<<m_router->get_id()
				<<" I"<<m_id<<" A REMAP " <<vc<<"->"<<remap_vc<<std::endl;
		activeRemap(std::cout);
		for(int i=init_vc;i<end_vc+1;i++)
		{
			std::cout<<"\t\tm_vcs["<<i<<"]->getCongestion() "<< m_vcs[i]->getCongestion()<<" ";
			if(m_vcs[i]->getCongestion()>0)
				std::cout<<m_vcs[i]->peekTopFlit()->get_type()<<" "
						<<m_vcs[i]->get_state()<<std::endl;
			else
				std::cout<<std::endl;

		}
		std::cout<<"----------------------------------"<<std::endl;
	}
	assert(remap_vc!=-1); // picked up a remap vc !!

	if(m_vc_remap[vc]!=-1)
	{
		std::cout<<"\n----------------------------------\n@"
				<<curTick()<<" R"<<m_router->get_id()
				<<" I"<<m_id<<" A REMAP " <<vc<<"->"<<remap_vc
				<<" sender "<< m_in_link->getLinkSource()->getObjectId()<<std::endl;
		activeRemap(std::cout);

		std::cout<<"----------------------------------"<<std::endl;
	}
	assert(m_vc_remap[vc]==-1); // actual vc has not remapped yet !! doesn't hold with reuse

	m_vc_remap[vc]=remap_vc; //update mapping
	m_vc_remap_inverse[remap_vc].push_back(vc); //get the reverse pair

	return remap_vc;
};

int
InputUnit_d::vcRemapPolicyReuseVnet(int vc,flit_d* flit)
{

	int remap_vc = -1;
	int init_vc = 0;
	int end_vc = m_router->get_num_vcs();

	#if USE_VC_REUSE==1
		#if USE_NON_ATOMIC_VC_ALLOC==1
		// fix it to combine fast signal and non atomic vc allocation
		assert(false && "\n\nIMPOSSIBLE TO USE NON_ATOMIC_VC_ALLOC WITH VC_REUSE\n\n\n");
		#endif
	// check for ch reuse first

	for(int i=init_vc;i<end_vc;i++)
	{
		if(m_reused_remap_vc_usable[i]==true)
		{
			//std::cout<<"reusable"<<std::endl;
			m_reused_remap_vc_used[i] = true;
			m_reused_remap_vc_usable[i] = false;
			remap_vc = i; //use it
		    m_router->add_buffer_reused_count();//used for statistics
			break;
		}
	}
	#endif

	if(remap_vc==-1)
	{
		for(int i=init_vc;i<end_vc;i++)
		{
			if(m_vcs[i]->get_state() == IDLE_)
			{
				assert(getCongestion(i)==0);
				remap_vc=i;

				#if USE_VC_REUSE==1
				//VC reuse statistics
				if(m_vcs[i]->getReuseCycle()==m_router->curCycle())
				{
					m_router->add_buffer_reused_count();//used for statistics
				}
				#endif
				break;
			}
		}
	}
	if(remap_vc==-1)
	{
		std::cout<<"\n----------------------------------\n@"
				<<curTick()<<" R"<<m_router->get_id()
				<<" I"<<m_id<<" A REMAP " <<vc<<"->"<<remap_vc
				<<" VCinit: "<<init_vc<<" VCend: "<<end_vc
				<<std::endl;
		activeRemap(std::cout);
		for(int i=init_vc;i<end_vc+1;i++)
		{
			std::cout<<"\t\tm_vcs["<<i<<"]->getCongestion() "<< m_vcs[i]->getCongestion()<<" ";
			if(m_vcs[i]->getCongestion()>0)
				std::cout<<m_vcs[i]->peekTopFlit()->get_type()<<" "
						<<m_vcs[i]->get_state()<<std::endl;
			else
				std::cout<<std::endl;

		}
		std::cout<<"----------------------------------"<<std::endl;
	}
	assert(remap_vc!=-1); // picked up a remap vc !!

	if(m_vc_remap[vc]!=-1)
	{
		std::cout<<"\n----------------------------------\n@"
				<<curTick()<<" R"<<m_router->get_id()
				<<" I"<<m_id<<" A REMAP " <<vc<<"->"<<remap_vc
				<<" sender "<< m_in_link->getLinkSource()->getObjectId()<<std::endl;
		activeRemap(std::cout);

		std::cout<<"----------------------------------"<<std::endl;
	}
	assert(m_vc_remap[vc]==-1); // actual vc has not remapped yet !! doesn't hold with reuse

	m_vc_remap[vc]=remap_vc; //update mapping
	m_vc_remap_inverse[remap_vc].push_back(vc); //get the reverse pair

	return remap_vc;
}
