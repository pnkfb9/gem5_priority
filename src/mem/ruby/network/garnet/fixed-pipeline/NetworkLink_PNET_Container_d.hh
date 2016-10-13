#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_LINK_PNET_CONTAINER_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_NETWORK_LINK_PNET_CONTAINER_D_HH__

#include <iostream>
#include <vector>
#include <limits>

#include <iostream>
#include <vector>
#include <limits>
#include "params/NetworkLink_PNET_Container_d.hh"
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Resynchronizer.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/network/orion/NetworkPower.hh"
#include "sim/clocked_object.hh"

class NetworkLink_PNET_Container_d : public ClockedObject, public Consumer
{
	public:
	static int objidGenerator_PNET;

    typedef NetworkLink_PNET_Container_dParams Params;
    NetworkLink_PNET_Container_d(const Params *p);
    ~NetworkLink_PNET_Container_d();

	// called locally by each router to the correct sublink
	//void setLinkConsumer(Consumer *consumer);
	//void setLinkSource(Consumer *consumer);
    //void setSourceQueue(flitBuffer_d *srcQueue);

	// TODO seems to be used for statistics
    void print(std::ostream& out) const{}
    std::vector<int> getVcLoad();
    int get_id(){return m_id;}
    

	void wakeup(){assert(false&&"\n\n Impossible to call wakeup on a container link\n\n");};


//    double calculate_power();
    

//    inline bool isReady(Time curTime) { return linkBuffer->isReady(curTime); }
//    inline flit_d* peekLink()       { return linkBuffer->peekTopFlit(); }
//    inline flit_d* consumeLink()    { return linkBuffer->getTopFlit(); }
//	inline int getLinkBufferStoredFlit(){ return linkBuffer->getCongestion();}
    void init_net_ptr(GarnetNetwork_d* net_ptr);
   	inline GarnetNetwork_d* getNetPtr(){return m_net_ptr;}
    //Resynchronizer *getReshnchronizer() { return resync; }

	// used internally by each sublink
    //Consumer* getLinkConsumer() { return link_consumer; }
	//Consumer* getLinkSource() { return link_source; }

	//TODO check not used in garnetnetwork
	//int getLinkLatency(){return m_latency;}

	std::vector<NetworkLink_d*>& getContainedLinksRef(){return m_net_link;}

	private:
		std::vector<NetworkLink_d*> m_net_link;
	public:
		// power methods for garnetnetwork_d
		double get_dynamic_power();
		double get_static_power();
		double calculate_power();

		Time getStatStartTime() const;
		int getLinkUtilization();
		void clearStats();

  protected:
	int m_virtual_networks;
    GarnetNetwork_d *m_net_ptr;

    int m_id;
    int m_latency;
    int channel_width;
    std::string m_objectId;
    int objid; //Generated from objidGenerator

	int m_resynch_fifo_slots;

    //flitBuffer_d *linkBuffer;
    //Consumer *link_consumer;
	//Consumer *link_source;

    //flitBuffer_d *link_srcQueue;
    int m_link_utilized;
    std::vector<int> m_vc_load;
    int m_flit_width;

    double m_power_dyn;
    double m_power_sta;
};
#endif
