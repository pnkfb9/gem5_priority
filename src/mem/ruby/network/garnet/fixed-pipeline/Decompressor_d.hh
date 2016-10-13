#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_DECOMPRESSOR_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_DECOMPRESSOR_D_HH__
#endif

#include <iostream>
#include <queue>
#include <vector>
#include <utility>
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"


class NetworkMessage;
class MessageBuffer;
class flitBuffer_d;
class NetworkInterface_d;
class flit_d;

class Decompressor_d :  public ClockedObject, public Consumer
{
public:

	Decompressor_d(int id,int virtual_networks, GarnetNetwork_d *network_ptr, NetworkInterface_d *nic);
	~Decompressor_d();
  
    void wakeup();
   	int getDecompressionCycles();
   	bool isIdle(int vc);
   	bool isFinishedDecompression(int vc);
   	void fillDecompressor(int vc,flit_d* t_flit);
    void print(std::ostream& out) const;
    
private:
	GarnetNetwork_d *m_net_ptr;
    int m_virtual_networks, m_num_vcs, m_vc_per_vnet;
    NodeID m_id;
    int m_resize;
    std::vector<std::queue<flit_d*>> m_decompressor_queues;
    std::queue<int> m_vc_reqs;
    NetworkInterface_d *m_nic;
    std::vector<Tick> m_decompressor_time;
    std::vector<int> m_requests;
    std::vector<int> m_reserved_vcs;
    std::vector<flit_d*> m_current_flit;

    //compressor tracer
    std::ofstream* m_decompressor_tracer;
    
public:

	int
	get_id()
	{
		return m_id;
	}    	
}; 	
