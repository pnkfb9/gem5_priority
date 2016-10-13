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
#include <iostream>

#include <typeinfo>

#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/SWallocator_d.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_PNET_Container_d.hh"

#include "config/use_spurious_vc_vnet_reuse.hh"
#include "config/use_pnet.hh"
#include "config/use_handshake_resynch.hh"
#include "config/actual_dvfs_policy.hh"

#include "config/use_lrc.hh"

#define RESYNCH_FIFO_SLOTS 7

NetworkLink_d::NetworkLink_d(const Params *p)
    : ClockedObject(p), Consumer(this)
{
    link_source=0;
    link_consumer=0;
    m_latency = p->link_latency;
    channel_width = p->channel_width;
    m_id = p->link_id;
    linkBuffer = new flitBuffer_d();
    m_link_utilized = 0;
	
	#if USE_SPURIOUS_VC_VNET_REUSE==1
	m_vc_load.resize(p->vcs_per_vnet * (p->virt_nets+p->virt_nets_spurious));

    for (int i = 0; i < (p->vcs_per_vnet * (p->virt_nets+p->virt_nets_spurious)); i++) {
        m_vc_load[i] = 0;
    }
	#else
	m_vc_load.resize(p->vcs_per_vnet * p->virt_nets);

    for (int i = 0; i < (p->vcs_per_vnet * p->virt_nets); i++) {
        m_vc_load[i] = 0;
    }

	#endif

    stat_start_time=0;
    
    this->setEventPrio(Event::NOC_NetworkLink_Pri);

	this->eq_resynch = p->eventq;
	
	resync=NULL;
	#if USE_PNET==0
	//if pnet impl is used wait to set objid since the container ptr is
	//required!!
    objid=objidGenerator++;
	std::ostringstream ss;
    ss << typeid(this).name() << objid;
    m_objectId = ss.str();
	#endif
	//std::cout<<m_objectId<<std::endl;
}

void
NetworkLink_d::setObjectId()
{
	#if USE_PNET==1
	//if pnet impl is used wait to set objid since the container ptr is
	//required!!
	std::ostringstream ss;
	ss << typeid(m_container_ptr).name()<<m_container_ptr->get_id()<<"."<<typeid(this).name() << getMinorId();
    m_objectId = ss.str();
	#else
		assert(false&&"\n\nImpossible to use this function without the use_pnet==1\n\n");
	#endif

}
NetworkLink_d::~NetworkLink_d()
{
	delete resync;
    delete linkBuffer;
}

void
NetworkLink_d::testAndSetResynch()
{
// getting the fifo resynch slots	
	m_resynch_fifo_slots=0;
	if(link_consumer!=NULL && link_source!=NULL)
	{//select the resynch
		assert(resync==NULL); //FIXME
		assert(this->eq_resynch!=NULL);
		std::string actual_handshake_policy = USE_HANDSHAKE_RESYNCH;
		std::string actual_noc_dvfs = ACTUAL_DVFS_POLICY;
		
		NetworkInterface_d* dest_temp= dynamic_cast<NetworkInterface_d*>(link_consumer);
		NetworkInterface_d* src_temp= dynamic_cast<NetworkInterface_d*>(link_source);

		// at least one router is src or consumer for each link	
		if(src_temp==NULL)
		{
			InputUnit_d* ip_temp=dynamic_cast<InputUnit_d*>(link_source);
			if(ip_temp!=NULL)
				m_resynch_fifo_slots=ip_temp->get_router()->getNumFifoResynchSlots();
			else
			{
				OutputUnit_d* op_temp=dynamic_cast<OutputUnit_d*>(link_source);
				if(op_temp!=NULL)
					m_resynch_fifo_slots=op_temp->get_router()->getNumFifoResynchSlots();

			}
		}
		if(m_resynch_fifo_slots==0)
		{
			InputUnit_d* ip_temp=dynamic_cast<InputUnit_d*>(link_consumer);
			if(ip_temp!=NULL)
				m_resynch_fifo_slots=ip_temp->get_router()->getNumFifoResynchSlots();
			else
			{
				OutputUnit_d* op_temp=dynamic_cast<OutputUnit_d*>(link_consumer);
				if(op_temp!=NULL)
					m_resynch_fifo_slots=op_temp->get_router()->getNumFifoResynchSlots();

			}

		}
		assert(m_resynch_fifo_slots>0);
/////end getting fifo resynch slots/////////////////////////////////////////////////////////

		//std::cout<< typeid(*link_consumer).name()<<" "
		//			<<typeid(*link_source).name()
		//			<<std::endl;
		assert( !(dest_temp!=NULL && src_temp!=NULL) );

		if(dest_temp!=NULL || src_temp!=NULL)
		{// NIC <-> router
			if(actual_handshake_policy=="HANDSHAKE_FOR_ALL")	
		    {	
				resync=new HandshakeResynchronizer(this->eq_resynch);
				//std::cout<<" HandshakeResynchronizer"<<std::endl;
			}
			else if(actual_handshake_policy=="HANDSHAKE_NIC_DUMMY_NOC")
			{
				resync=new HandshakeResynchronizer(this->eq_resynch);
				//std::cout<<" HandshakeResynchronizer"<<std::endl;
			}
			else if(actual_handshake_policy=="FIFO_FOR_ALL")	
		    {	
				resync=new FifoResynchronizer(this->eq_resynch,m_resynch_fifo_slots/*RESYNCH_FIFO_SLOTS*/,this );
				//std::cout<<" FifoResynchronizer"<<std::endl;
			}
			else if(actual_handshake_policy=="FIFO_NIC_DUMMY_NOC")
			{
				resync=new FifoResynchronizer(this->eq_resynch,m_resynch_fifo_slots/*RESYNCH_FIFO_SLOTS*/,this);
				//std::cout<<" FifoResynchronizer"<<std::endl;
			}
			else if(actual_handshake_policy=="DUMMY_FOR_ALL")
			{
				assert( (actual_noc_dvfs=="NOCHANGE_SENSING"||actual_noc_dvfs=="ALL_PNET_NOCHANGE")&&"Impossible to use dummy resynch with DFS or DVFS for NoC");
				resync=new DummyResynchronizer(this->eq_resynch);
				//std::cout<<" DummyResynchronizer"<<std::endl;				
			}
			else	
				panic("\n\nHANDSHAKE error\n\n");	
		}
		else
		{// router <-> router (resynch depends on the flag value)
			if(actual_handshake_policy=="HANDSHAKE_FOR_ALL")	
		    {	
				resync=new HandshakeResynchronizer(this->eq_resynch);
				//std::cout<<" HandshakeResynchronizer"<<std::endl;
			}
			else if(actual_handshake_policy=="HANDSHAKE_NIC_DUMMY_NOC")
			{				
				#if USE_PNET==0
				assert(actual_noc_dvfs=="NOCHANGE_SENSING"&&"Impossible to use dummy resynch with DFS or DVFS for NoC");
				#endif	
				resync=new DummyResynchronizer(this->eq_resynch);
				//std::cout<<" DummyResynchronizer"<<std::endl;
			}
			else if(actual_handshake_policy=="FIFO_FOR_ALL")	
		    {	
				resync=new FifoResynchronizer(this->eq_resynch,m_resynch_fifo_slots/*RESYNCH_FIFO_SLOTS*/,this);
				//std::cout<<" HandshakeResynchronizer"<<std::endl;
			}
			else if(actual_handshake_policy=="FIFO_NIC_DUMMY_NOC")
			{
				#if USE_PNET==0
				assert(actual_noc_dvfs=="NOCHANGE_SENSING"&&"Impossible to use dummy resynch with DFS or DVFS for NoC");
				#endif
				resync=new DummyResynchronizer(this->eq_resynch);
				//std::cout<<" DummyResynchronizer"<<std::endl;
			}
			else if(actual_handshake_policy=="DUMMY_FOR_ALL")
			{
				assert((actual_noc_dvfs=="NOCHANGE_SENSING"||actual_noc_dvfs=="ALL_PNET_NOCHANGE")&&"Impossible to use dummy resynch with DFS or DVFS for NoC");
				resync=new DummyResynchronizer(this->eq_resynch);
				//std::cout<<" DummyResynchronizer"<<std::endl;				
			}
			else	
				panic("\n\nHANDSHAKE error\n\n");	
		}
	}
}

void
NetworkLink_d::setLinkConsumer(Consumer *consumer)
{
    link_consumer = consumer;	
	testAndSetResynch();
}

void
NetworkLink_d::setLinkSource(Consumer *source)
{
    link_source = source;
	testAndSetResynch();
}

void
NetworkLink_d::setSourceQueue(flitBuffer_d *srcQueue)
{
    link_srcQueue = srcQueue;
}

void
NetworkLink_d::wakeup()
{
	assert(resync!=NULL && "Some resynch hasn't initialized yet, but we'are simulating !!!");
    if (link_srcQueue->isReady(curCycle())) 
	{
        flit_d *t_flit = link_srcQueue->getTopFlit();

        const ClockedObject *o=this->getLinkConsumer()->getClockedObject();
        Tick delay=o->clockEdge()==curTick() ? 1 : 0;
        
        /**
         * When moving a flit through the NoC, adjust
         * its time to that of the consumer every time
         * a link is traversed, as each device, router
         * or NetworkInterface, can run at a different
         * frequency and the curCycle() of one device
         * has no relation to the one on the other end
         * of the link.
         */
        t_flit->set_time(o->curCycle() + delay);
        linkBuffer->insert(t_flit);
	 
		assert(linkBuffer->getCongestion()<=m_resynch_fifo_slots);

		//LRC comes into the game here to schedule for the DATA link only, not
		//on the credit	link, an event on the SA instead of IP!!!!
		// NOTE that if dest is a NIC or an OutputPort_d schedule event on the
		// link_consumer normally!!!!!
		#if USE_LRC==1
			//check if link_consumer is an IP, NIC or OP
			//NetworkInterface_d* dest_temp= dynamic_cast<NetworkInterface_d*>(link_consumer);
			//OutputUnit_d* op_temp=dynamic_cast<OutputUnit_d*>(link_consumer);
			InputUnit_d* ip_temp=dynamic_cast<InputUnit_d*>(link_consumer);
			if(ip_temp!=NULL)
				ip_temp->get_router()->getSWallocatorUnit()->scheduleEvent(delay);
			else// dest is a NIC or an OP
				link_consumer->scheduleEvent(delay);
		#else
        	link_consumer->scheduleEvent(delay);
		#endif

        m_link_utilized++;
        m_vc_load[t_flit->get_vc()]++;

        resync->scheduleRequestPropagation();
	}
	else
	{
		assert(false && "\n\nPutting data to NetworkLink that is not ready to receive them!!\n\n" );
	}
}

void
NetworkLink_d::clearStats()
{
    stat_start_time=curCycle();
}

std::vector<int>
NetworkLink_d::getVcLoad()
{
    return m_vc_load;
}

int
NetworkLink_d::getLinkUtilization()
{
    return m_link_utilized;
}

NetworkLink_d *
NetworkLink_dParams::create()
{
    return new NetworkLink_d(this);
}

CreditLink_d *
CreditLink_dParams::create()
{
    return new CreditLink_d(this);
}

int NetworkLink_d::objidGenerator=0;
