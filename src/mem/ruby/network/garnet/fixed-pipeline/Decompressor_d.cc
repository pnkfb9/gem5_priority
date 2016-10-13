#include <cassert>
#include <cmath>
#include <sstream>
#include <fstream>
#include "mem/protocol/Types.hh"
#include "mem/ruby/system/MachineID.hh"
#include "base/cast.hh"
#include "base/stl_helpers.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Decompressor_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flit_d.hh"
#include "config/use_spurious_vc_vnet_reuse.hh"
#include "config/debug_wh_vc_sa.hh"
#include "config/use_synthetic_traffic.hh"
#include "config/use_adaptive_routing.hh"
#define DEBUG_PRINT_NOISE 0
#define MIX_REAL_NOISE_TRAFFIC 0

#define TRACER_DECOMPRESSOR "m5out/decompressors/"
#define DECOMPRESSION_CYCLES 5
#define DECOMPRESSOR_IDLE 1
#define DECOMPRESSOR_BUSY 0
#define DECOMPRESSION 1 //1: 1 Decompressor per VC, 2: 1 Decompressor per vnet, 3: 1 Decompressor per NI

#define USE_DEBUG 0
using namespace std;
using m5::stl_helpers::deletePointers;

static ClockedObjectParams *makeParam(const GarnetNetwork_d *network_ptr)
{
    //FIXME: verify!!
    ClockedObjectParams *result=new ClockedObjectParams;
    result->name=network_ptr->params()->name;
    result->pyobj=network_ptr->params()->pyobj;
    result->eventq=network_ptr->params()->eventq;
    result->clock=network_ptr->clockPeriod();
    return result;
}

Decompressor_d::Decompressor_d(int id,int virtual_networks,GarnetNetwork_d *network_ptr, NetworkInterface_d *nic)
:ClockedObject(makeParam(network_ptr)), Consumer(this)
{
	m_id=id;
    m_net_ptr = network_ptr;
    m_virtual_networks  = virtual_networks;
    m_vc_per_vnet = m_net_ptr->getVCsPerVnet();
    m_num_vcs=m_virtual_networks*m_vc_per_vnet;
    m_nic=nic;
    m_resize=-1;

    #if USE_DEBUG == 1
  
    stringstream m_path;
    m_path << TRACER_DECOMPRESSOR;

      stringstream cmd;
      cmd << "mkdir -p " << m_path.str();
      string cmd_str = cmd.str();
      assert(system(cmd_str.c_str()) != -1);
    
    stringstream decompressor_filename;
    decompressor_filename << m_path.str() << "decompressor" << m_id << ".txt";
    m_decompressor_tracer=new ofstream(decompressor_filename.str());
    stringstream ss;
    
    #endif
    
    #if DECOMPRESSION == 1 
    	
  		m_resize=m_num_vcs;

      #if USE_DEBUG == 1
      ss<<"CYCLE";
      for(int i=0;i<m_resize;i++)
      { 
        ss<<";VC"<<i;
      }
      #endif
    #endif

    #if DECOMPRESSION == 2

   		m_resize=m_virtual_networks;

      #if USE_DEBUG == 1
      
      ss<<"CYCLE";
      
      for(int i=0;i<m_resize;i++)
      {
       ss<<";VNET"<<i;
      }
      #endif
    #endif

   	#if DECOMPRESSION == 3
   	
   		m_resize=1;
      #if USE_DEBUG == 1
        
        ss<<"CYCLE;NI";
      
      #endif
   
   	#endif	

     #if USE_DEBUG == 1
      
      assert(m_decompressor_tracer->is_open());
     *m_decompressor_tracer<<ss.str()<<std::endl;
     
      #endif

    m_decompressor_queues.resize(m_resize);
    m_decompressor_time.resize(m_resize);
    m_requests.resize(m_resize);
    m_current_flit.resize(m_resize);
    
    for(int i=0;i<m_resize;i++)
    {
    	m_decompressor_time[i]=-1;							
    	m_current_flit[i]=NULL;							
    	m_requests[i]=DECOMPRESSOR_IDLE;
    }
}

Decompressor_d::~Decompressor_d()
{
	deletePointers(m_current_flit);
  m_decompressor_tracer->close();
  assert(!m_decompressor_tracer->is_open());
    
  delete m_decompressor_tracer;
	delete m_net_ptr;
	delete m_nic;

}

int
Decompressor_d::getDecompressionCycles()
{
	return DECOMPRESSION_CYCLES;
}

void 
Decompressor_d::wakeup()
{
	#if USE_DEBUG == 1
		
		stringstream update;
		update<<curCycle();
	  
  for(int i=0;i<m_resize;i++)
  {
    update<<";"<<isIdle(i);
  }
  *m_decompressor_tracer<<update.str()<<std::endl;
  this->scheduleEvent(1);
	#endif

	for(int i=0;i<m_resize;i++)
	{
		if(isIdle(i)) 
		{   
			assert(m_current_flit[i]==NULL);
			assert(m_decompressor_time[i]==-1);
			if(!m_decompressor_queues[i].empty())
			{
				m_current_flit[i]=m_decompressor_queues[i].front();
				m_decompressor_queues[i].pop();
				if(m_current_flit[i]->is_zero_comp())
				{
					m_decompressor_time[i]=curTick()+1000;
					m_requests[i]=DECOMPRESSOR_BUSY;
                                	scheduleEvent(1);
				
				}
				else
				{	
					m_decompressor_time[i]=curTick()+(clockPeriod()*getDecompressionCycles());
					m_requests[i]=DECOMPRESSOR_BUSY;
                                	scheduleEvent(getDecompressionCycles());

				}
				
				
			}
		}else{

		if(isFinishedDecompression(i))
		{   
			
			assert(m_current_flit[i]!=NULL);
			assert(m_decompressor_time[i]!=-1);
			flit_d* t_flit=m_current_flit[i];
			assert(t_flit!=NULL);
			m_current_flit[i]=NULL;
			m_decompressor_time[i]=-1;
			m_requests[i]=DECOMPRESSOR_IDLE;
			assert(t_flit->get_vnet()>=0&&t_flit->get_vnet()<m_virtual_networks);
			assert(t_flit->get_msg_ptr()>0);
			
			m_nic->getOutNode()[t_flit->get_vnet()]->enqueue(t_flit->get_msg_ptr(), 1);
			int vnet = t_flit->get_vnet();

		        if(t_flit->get_type() == HEAD_TAIL_)
                                m_net_ptr->increment_received_short_pkt(vnet);
                        else if(t_flit->get_type() == TAIL_)
                                m_net_ptr->increment_received_long_pkt(vnet);
	
			//int vnet = t_flit->get_vnet();
			m_net_ptr->increment_received_flits(vnet);
            int network_delay = m_net_ptr->curCycle() - t_flit->get_enqueue_time();
            int queueing_delay = t_flit->get_delay();
            m_net_ptr->increment_network_latency(network_delay, vnet);
            m_net_ptr->increment_queueing_latency(queueing_delay, vnet);
			delete t_flit;
			m_nic->scheduleEvent(1);
			
		}
		
		}		
		
	}
	scheduleEvent(1); 
}

void
Decompressor_d::fillDecompressor(int vc,flit_d* t_flit)
{
	int slot=-1;
	assert(vc>=0&&vc<m_num_vcs);
	assert(t_flit!=NULL);
	
	#if DECOMPRESSION == 1
		
		slot=vc;

	#endif
	#if DECOMPRESSION == 2
		
		slot=t_flit->get_vnet();

	#endif
	#if DECOMPRESSION == 3
		slot=0;
	#endif

	m_decompressor_queues[slot].push(t_flit);
    scheduleEvent(1);
}

bool
Decompressor_d::isIdle(int vc)
{
	
	
	 if(m_requests[vc]==DECOMPRESSOR_IDLE)
    {
      assert(m_decompressor_time[vc]==-1);
      assert(m_current_flit[vc]==NULL);
      return true;

    }
    else
    {

      return false; 
    
    }   

}

bool
Decompressor_d::isFinishedDecompression(int vc)
{
	
	
	Tick current=curTick();
  
  if((m_decompressor_time[vc]<=current&&m_decompressor_time[vc]!=-1))
  {
  	
  	assert(m_requests[vc]==DECOMPRESSOR_BUSY);
  	assert(m_current_flit[vc]!=NULL);
    return true;
  }
  else
  {
    return false;
  }
}

void
Decompressor_d::print(std::ostream& out) const
{
     out << "[Decompressor]";
}
