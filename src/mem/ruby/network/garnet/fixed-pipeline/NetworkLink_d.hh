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

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_LINK_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_LINK_D_HH__

#include <iostream>
#include <vector>
#include <limits>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Resynchronizer.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/network/orion/NetworkPower.hh"
#include "params/NetworkLink_d.hh"
#include "sim/clocked_object.hh"
#include "config/use_pnet.hh"

class GarnetNetwork_d;
class NetworkLink_PNET_Container_d;


class NetworkLink_d : public ClockedObject, public Consumer
{

	//#if USE_PNET==1
	private:
	  	int m_minor_id;
		NetworkLink_PNET_Container_d* m_container_ptr;
	public:
		void setMinorId(int minor){m_minor_id=minor;}
		int getMinorId(){return m_minor_id;};
		void setPnetContainerPtr(NetworkLink_PNET_Container_d* ptr){m_container_ptr=ptr;}
		void setObjectId();
	//#endif
  public:
	std::string getObjectId() const { return m_objectId; }

    typedef NetworkLink_dParams Params;
    NetworkLink_d(const Params *p);
    ~NetworkLink_d();

    void setLinkConsumer(Consumer *consumer);
	void setLinkSource(Consumer *consumer);

    void setSourceQueue(flitBuffer_d *srcQueue);
    void print(std::ostream& out) const{}
    int getLinkUtilization();
    std::vector<int> getVcLoad();
    int get_id(){return m_id;}
    double get_dynamic_power(){return m_power_dyn;}
    double get_static_power(){return m_power_sta;}
    void wakeup();

    double calculate_power();
    
    void clearStats();
    Time getStatStartTime() const { return stat_start_time; }

    inline bool isReady(Time curTime) { return linkBuffer->isReady(curTime); }
    inline flit_d* peekLink()       { return linkBuffer->peekTopFlit(); }
    inline flit_d* consumeLink()    { return linkBuffer->getTopFlit(); }
	inline int getLinkBufferStoredFlit(){ return linkBuffer->getCongestion();}
    void init_net_ptr(GarnetNetwork_d* net_ptr)
    {
        m_net_ptr = net_ptr;
    }
   	inline GarnetNetwork_d* getNetPtr(){return m_net_ptr;}
    Resynchronizer *getReshnchronizer() { return resync; }
	
    Consumer* getLinkConsumer() { return link_consumer; }
	Consumer* getLinkSource() { return link_source; }
	int getLinkLatency(){return m_latency;}
  private:
    
    Time stat_start_time; //a replacement for m_ruby_start which is DVFS aware
    Resynchronizer *resync;
    
    /*
     * These classes already have m_id, which unfortunately is not unique.
     * In fact, a router is connected with another object in the noc (be it
     * another router, or a network interface) via four network links: two to
     * achieve bidirectionality, plus two credit links, and guess what?
     * m_id for all of them is the same. Go figure...
     * This counter is incremented every time the class is instantiated, so
     * that getObjectId() can return a *real* unique id.
     */
    static int objidGenerator;

	/* test and eventually insert the right resynchronizer. Always the handshake
 	* resynch between router and NI, while depending on the required NoC
 	* frequency islands select between dummy and handshake resynch between routers
	*/
	void testAndSetResynch();
	EventQueue *eq_resynch; //saving the eventqueue to be used to init the resynch
    
  protected:
    int m_id;
    int m_latency;
    int channel_width;
    std::string m_objectId;
    int objid; //Generated from objidGenerator

	int m_resynch_fifo_slots;

    GarnetNetwork_d *m_net_ptr;
    flitBuffer_d *linkBuffer;
    Consumer *link_consumer;
	Consumer *link_source;

    flitBuffer_d *link_srcQueue;
    int m_link_utilized;
    std::vector<int> m_vc_load;
    int m_flit_width;

    double m_power_dyn;
    double m_power_sta;


};

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_LINK_D_HH__
