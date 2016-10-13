#include <cassert>
#include <cmath>
#include <sstream>
#include <fstream>
#include <bitset>
//#include <boost/random.hpp>
#include "mem/protocol/Types.hh"
#include "mem/ruby/system/MachineID.hh"
#include "base/cast.hh"
#include "base/stl_helpers.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Compressor_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flit_d.hh"
#include "config/use_spurious_vc_vnet_reuse.hh"
#include "config/debug_wh_vc_sa.hh"
#include "config/use_synthetic_traffic.hh"
#include "config/use_adaptive_routing.hh"
#define DEBUG_PRINT_NOISE 0
#define MIX_REAL_NOISE_TRAFFIC 0
#define FLIT_CONTENT_LENGHT 8
#define NUM_FLITS_PER_PACKET 8
#define COMPRESSION_CYCLES 5
#define TRACER_COMPRESSOR "m5out/compressors/"
#define COMPRESSOR_IDLE 1
#define COMPRESSOR_BUSY 0

#define RATIO_75 1
#define RATIO_50 2
#define RATIO_25 4

//metrics for policy decision (are in part hardware dependant)

#define LINK_COST 1
#define ROUTER_COST 5 // 5 pipeline stages 
#define COMPRESSION_COST 5
#define DECOMPRESSION_COST 5
#define NUM_FLITS 9
#define AVG_COMP_RATIO 0.2
//for compression algorithm selection

#define DELTA_CMPR 1    //constants for policy identification
#define ZC_CMPR 2
#define PARALLEL_CMPR 3
#define COMPRESSION 1 //1: 1 Compressor per VC, 2: 1 Compressor per vnet, 3:1 Compressor per NI
#define NUM_CORES 1

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

Compressor_d::Compressor_d(int id,int virtual_networks,GarnetNetwork_d *network_ptr, NetworkInterface_d *nic,std::string actual_policy)
:ClockedObject(makeParam(network_ptr)), Consumer(this)
{
    m_id=id;
    m_net_ptr = network_ptr;
    m_virtual_networks  = virtual_networks;
    m_vc_per_vnet = m_net_ptr->getVCsPerVnet();
    m_num_vcs=m_virtual_networks*m_vc_per_vnet;
    m_nic=nic;
    m_resize=-1;

    m_actual_policy=getActualPolicy(actual_policy);
    m_uncompressed_flits=0;
    m_compressed_flits=0;
    m_saves=0;
    m_saves_per_cycles.resize(m_virtual_networks);
    m_start_sleep_time.resize(m_virtual_networks);
    m_is_asleep.resize(m_virtual_networks);
    m_compression_cycles_spent.resize(m_virtual_networks);
    
	#if USE_DEBUG == 1

    stringstream m_path;
    m_path << TRACER_COMPRESSOR;

      stringstream cmd;
      cmd << "mkdir -p " << m_path.str();
      string cmd_str = cmd.str();
      assert(system(cmd_str.c_str()) != -1);

    stringstream compressor_filename;
    compressor_filename << m_path.str() << "compressor" << m_id << ".txt";
    m_compressor_tracer=new ofstream(compressor_filename.str());
    stringstream ss;

    #endif




  #if COMPRESSION == 1 // 1 Comp. per VC

    m_resize=m_num_vcs;

    #if USE_DEBUG == 1
    ss<<"CYCLE";
    for(int i=0;i<m_resize;i++)
    {
      ss<<";VC"<<i;
    }
    #endif

  #endif

  #if COMPRESSION == 2 // 1 Comp. per VNET

    m_resize=m_virtual_networks;

     #if USE_DEBUG == 1
      ss<<"CYCLE";
      for(int i=0;i<m_resize;i++)
      {
        ss<<";VNET"<<i;
      }
      #endif

  #endif

  #if COMPRESSION == 3 // 1 Comp. per NI

      m_resize=1;

      #if USE_DEBUG == 1
            ss<<"CYCLE;NI";
      #endif

  #endif

      #if USE_DEBUG == 1

      assert(m_compressor_tracer->is_open());
     *m_compressor_tracer<<ss.str()<<std::endl;

      #endif

      m_compressor_buffers.resize(m_resize);
      m_compressor_time.resize(m_resize);
      m_requests.resize(m_resize);
      m_slack_queues.resize(m_resize);
      m_reserved_vcs.resize(m_resize); // -1 stays per unused.
	is_zero_compression_ready = false;
      for(int i=0;i<m_resize;i++)
      {		
          
          m_compressor_buffers[i] = new flitBuffer_d();
          m_compressor_time[i]=-1;
          m_reserved_vcs[i]=-1;
	  m_slack_queues[i].push(0);		  
          m_requests[i]=COMPRESSOR_IDLE;

      }

      switch(m_actual_policy)
      {
        case DELTA_CMPR:
		

          m_base.resize(FLIT_CONTENT_LENGHT); // 8 bytes lenght.
          m_body_flits.resize(NUM_FLITS_PER_PACKET-1); //7 elements each one of 8 elements(8 bytes).
          m_compression_mask.resize(FLIT_CONTENT_LENGHT*(NUM_FLITS_PER_PACKET-1));//bitmask of 56 bits

          for(int i=0;i<NUM_FLITS_PER_PACKET-1;i++)
          {
            m_body_flits[i].resize(FLIT_CONTENT_LENGHT);   //each element is 8 elements(8 bytes).
          }

        break;

        case ZC_CMPR:
	 

          m_zero_compression_mask.resize(NUM_FLITS_PER_PACKET*FLIT_CONTENT_LENGHT); //8 booleans to store if a flit is compressed or not. (1=compressed, 0=uncompressed)
          m_packet_offsets.resize(NUM_FLITS_PER_PACKET*FLIT_CONTENT_LENGHT); //8 integers to store the offset, used in decompression phase to reconstruct original message.
          m_zero_body_flits.resize(NUM_FLITS_PER_PACKET);//8 1-byte vector for flits content
          for(int i=0;i<NUM_FLITS_PER_PACKET;i++)
          {
            m_zero_body_flits[i].resize(FLIT_CONTENT_LENGHT);   //each element is 8 elements(8 bytes).
          }

        break;
	case PARALLEL_CMPR:
     		
	 
          m_base.resize(FLIT_CONTENT_LENGHT); // 8 bytes lenght.
          m_body_flits.resize(NUM_FLITS_PER_PACKET-1); //7 elements each one of 8 elements(8 bytes).
          m_compression_mask.resize(FLIT_CONTENT_LENGHT*(NUM_FLITS_PER_PACKET-1));//bitmask of 56 bits
	  m_zero_compression_mask.resize(NUM_FLITS_PER_PACKET*FLIT_CONTENT_LENGHT); //8 booleans to store if a flit is compressed or not. (1=compressed, 0=uncompressed)
          m_packet_offsets.resize(NUM_FLITS_PER_PACKET*FLIT_CONTENT_LENGHT); //8 integers to store the offset, used in decompression phase to reconstruct original message.
          m_zero_body_flits.resize(NUM_FLITS_PER_PACKET);//8 1-byte vector for flits content
          for(int i=0;i<NUM_FLITS_PER_PACKET;i++)
          {
           if(i < NUM_FLITS_PER_PACKET - 1)m_body_flits[i].resize(FLIT_CONTENT_LENGHT);   //each element is 8 elements(8 bytes).

            m_zero_body_flits[i].resize(FLIT_CONTENT_LENGHT);   //each element is 8 elements(8 bytes).
          }
			//std::cout<<"resize fatta"<<std::endl;
	break;
      }

      for(int i=0; i<m_virtual_networks;i++)
      {
        m_saves_per_cycles[i]=0.0;
        m_start_sleep_time[i]=0;
        m_is_asleep[i]=false;
        m_compression_cycles_spent[i]=0;
      }
}

 Compressor_d::~Compressor_d()
 {
    deletePointers(m_compressor_buffers);

    m_compressor_tracer->close();
    assert(!m_compressor_tracer->is_open());

    delete m_compressor_tracer;
    delete m_nic;
    delete m_net_ptr;
 }

/**
*This method wakes up the compressor. It checks primarily if there's a free vc in the nic to copy compressed flits to.
*Second it checks if compressor is actually busy compressing something (check on timing). If one of the two checks fails, compression for that vc will fail.
*/
void
Compressor_d::wakeup()
{
stringstream update;
update<<curCycle();

  #if COMPRESSION == 1
  assert(m_resize==m_num_vcs);
  #endif

  #if COMPRESSION == 2


 assert(m_resize==m_virtual_networks);

  #endif

  #if COMPRESSION == 3
  assert(m_resize==1);
  #endif

  //tracer to print the usage of compressors
  #if USE_DEBUG == 1

  for(int i=0;i<m_resize;i++)
  {
    update<<";"<<isIdle(i);
  }
  *m_compressor_tracer<<update.str()<<std::endl;

  #endif

  for(int i=0;i<m_resize;i++)
  {
    if(isFinishedCompression(i))
    {
       assert(m_requests[i]==COMPRESSOR_BUSY);

       this->compress(i);
       m_requests[i]=COMPRESSOR_IDLE;
    }
  }

          m_nic->scheduleEvent(1);
          #if USE_DEBUG == 1
          this->scheduleEvent(1);
          #endif
}




void
Compressor_d::fillCompressor(int vc,flit_d* flit)
{
  int slot=-1;

  #if COMPRESSION == 1
  slot=vc;
  assert(slot>=0&&slot<m_num_vcs);
  #endif

  #if COMPRESSION == 2

  slot=m_nic->get_vnet(vc);
  assert(slot>=0&&slot<m_virtual_networks);

  #endif

  #if COMPRESSION == 3
  slot=0;
  #endif

  m_compressor_buffers[slot]->insert(flit);
}

/**
*Method that returns the number of compression cycles.
**/
int
Compressor_d::getCompressionCycles()
{
  return COMPRESSION_CYCLES;
}

/**
This method returns true if the vc (passed as parameter) is free.
**/
bool
Compressor_d::isIdle(int vc)
{
  int slot=-1;

  #if COMPRESSION == 1
  slot=vc;
  assert(slot>=0&&slot<m_num_vcs);
  #endif

  #if COMPRESSION == 2

  slot=m_nic->get_vnet(vc);
  assert(slot>=0&&slot<m_virtual_networks);

  #endif

  #if COMPRESSION == 3

  slot=0;

  #endif

    if(m_compressor_time[slot]==-1&&m_compressor_buffers[slot]->getCongestion()==0)
    {

      return true;

    }
    else
    {

      return false;

    }
}



void
Compressor_d::prepareForCompression(int vc_in,Tick busy_time,int vc_out)
{
    int slot=-1;

  #if COMPRESSION == 1
    slot=vc_in;
  #endif

  #if COMPRESSION == 2
   slot=m_nic->get_vnet(vc_in);
  #endif

  #if COMPRESSION == 3
    slot=0;
  #endif

   assert(m_compressor_time[slot]==-1);
   assert(vc_out>=0&&vc_out<m_num_vcs);
   assert(busy_time>0);

  m_compressor_time[slot]=busy_time;
  m_requests[slot]=COMPRESSOR_BUSY;
  m_reserved_vcs[slot]=vc_out;

}
void
Compressor_d::print(std::ostream& out) const
{
     out << "[Compressor]";
}


bool
Compressor_d::isFinishedCompression(int slot)
{


   #if COMPRESSION == 1
  assert(slot>=0&&slot<m_num_vcs);
  #endif

  #if COMPRESSION == 2
  assert(slot>=0&&slot<m_virtual_networks);
  #endif

  #if COMPRESSION == 3
  assert(slot==0);
  #endif

  Tick current=curTick();

  if((m_compressor_time[slot]<=current&&m_compressor_time[slot]!=-1)&&m_compressor_buffers[slot]->getCongestion()>0)
  {
    return true;
  }
  else
  {
    return false;
  }

}
void
Compressor_d::calculateCompressionMask()
{
  int offset=0;
 

      for(int flits=0;flits<NUM_FLITS_PER_PACKET-1;flits++)
        {
          for(int bytes=0;bytes<FLIT_CONTENT_LENGHT;bytes++)
            {
              m_compression_mask[offset]=subtract(m_base[bytes],m_body_flits[flits][bytes]);
              offset++;
            }
        }

  

}
void
Compressor_d::compress(int vc_in) //with this type of configuration, we assume that once the compressor has finished, it flushes its content immediately
{

  int vc,size;
  #if COMPRESSION == 1
  assert(vc_in>=0&&vc_in<m_num_vcs);
  #endif

  #if COMPRESSION == 2
  assert(vc_in>=0&&vc_in<m_virtual_networks);
  #endif

  #if COMPRESSION == 3
  assert(vc_in==0);
  #endif

  size=m_compressor_buffers[vc_in]->getCongestion();
  vc=m_reserved_vcs[vc_in];

  for(int i=0; i<size; i++)//qui devo fare la compressione delta
  {

    int vnet=m_compressor_buffers[vc_in]->peekTopFlit()->get_vnet();

     
     
     m_nic->getNIBuffers()[vc]->insert(m_compressor_buffers[vc_in]->getTopFlit());


  }

  assert(m_compressor_buffers[vc_in]->getCongestion()==0);

  m_compressor_time[vc_in]=-1; //-1 = unused
  m_reserved_vcs[vc_in]=-1;    //-1 = unused
  m_requests[vc_in]=COMPRESSOR_IDLE;
}

void
Compressor_d::extractBase(/*flit_d* base_flit*/MsgPtr message_ptr)
{
  assert(message_ptr.get()!= NULL);

  DataBlock content;

  ResponseMsg* resp_message=dynamic_cast<ResponseMsg*>(message_ptr.get());
  RequestMsg* req_message;

  if(resp_message!=NULL)
  {

    content=resp_message->getDataBlk();

  }
  else
  {
    req_message=dynamic_cast<RequestMsg*>(message_ptr.get());

    content=req_message->getDataBlk();

  }

  for(int i=0;i<FLIT_CONTENT_LENGHT;i++)
  {

    m_base[i]=content.getByte(i);

  }


}

void
Compressor_d::extractFlitsContent(/*flit_d* base_flit*/MsgPtr message_ptr)
{
  assert(message_ptr!=0);
  ResponseMsg* resp_message=dynamic_cast< ResponseMsg* >(message_ptr.get());
  RequestMsg* req_message;



  DataBlock content;



  int offset=8;

  if(resp_message!=NULL)
    {

      content=resp_message->getDataBlk();

    }
    else
    {
      req_message=dynamic_cast<RequestMsg*>(message_ptr.get());

      content=req_message->getDataBlk();

    }

  switch(m_actual_policy)
  {
    case DELTA_CMPR:

      for(int i=0;i<NUM_FLITS_PER_PACKET-1;i++)
        {
          for(int j=0;j<FLIT_CONTENT_LENGHT;j++)
            {

                m_body_flits[i][j]=content.getByte(offset);
                offset++;

            }
        }

    break;

    case ZC_CMPR:

    for(int i=0;i<NUM_FLITS_PER_PACKET;i++)
      {
          for(int j=0;j<FLIT_CONTENT_LENGHT;j++)
            {

              m_zero_body_flits[i][j]=content.getByte(offset);
              offset++;

            }
      }
     
    break;

    case PARALLEL_CMPR:
	
    for(int i=0;i<NUM_FLITS_PER_PACKET;i++)
      {
          for(int j=0;j<FLIT_CONTENT_LENGHT;j++)
            {
		uint8_t block =content.getByte(offset);
		if(i < NUM_FLITS_PER_PACKET-1) m_body_flits[i][j]=block;
		
              m_zero_body_flits[i][j]=block;
              offset++;

            }
      }


    break;

  }


}

void
Compressor_d::resetCompressionMask()
{

  switch(m_actual_policy)
  {
    case DELTA_CMPR:

    for(int i=0;i<FLIT_CONTENT_LENGHT*(NUM_FLITS_PER_PACKET-1);i++)
      {
        m_compression_mask[i]=false;
      }

    break;

    case ZC_CMPR:

    for(int i=0;i<NUM_FLITS_PER_PACKET;i++)
      {
        m_zero_compression_mask[i]=false;
      }

    break;
    case PARALLEL_CMPR:
     for(int i=0;i<FLIT_CONTENT_LENGHT*(NUM_FLITS_PER_PACKET-1);i++)
      {
        if(i < NUM_FLITS_PER_PACKET)m_zero_compression_mask[i]=false;
	m_compression_mask[i]=false;
      }

   break;
  }

}

void
Compressor_d::printCompressionMask()
{
std::stringstream ss;
int offset=0;
  for(int i=0;i<NUM_FLITS_PER_PACKET-1;i++)
  {
      for(int j=0;j<8;j++)
      {
          ss<<m_compression_mask[offset];
          offset++;
      }
      ss<<" ";
  }
  ss<<"\n";
  std::cout<<ss.str()<<std::endl;
  ss.str("");
}

void
Compressor_d::printBase()
{
  std::stringstream ss;


      for(int j=0;j<8;j++)
      {
          ss<<m_base[j];

      }



  std::cout<<ss.str()<<std::endl;
  ss.str("");
}


bool
Compressor_d::subtract(uint8_t op_1, uint8_t op_2)//will rename it
{

  uint8_t result;

  result=op_1 ^ op_2; //XOR operation between two operands

  if(result) return true; //1 diffs
  return false; //0 no diffs

}

int
Compressor_d::countBits(int ratio)
{
  int counter=0,result=0,offset=0,start_bit=0;

    if(ratio==1)
      {
        start_bit=2;
      }
      else
      {
       start_bit=8-8/ratio;
      }

    for(int i=0;i<NUM_FLITS_PER_PACKET-1;i++)
    {
      for(int j=start_bit;j<8;j++)
      {
            counter=counter||m_compression_mask[j+offset];
      }
      offset+=8;

      if(!counter)
      {
        result++;
      }
      else
      {
       counter=0;
      }

    }
    return result; //returns the number of flit that can be compressed at a given ratio (25,50,75 percent)
}

int
Compressor_d::analyzeMessage(MsgPtr message)
{
  int saves=0;

    if(getActualPolicy() == DELTA_CMPR)  //Delta Compression enabled
    {
      saves=deltaCompression(message);
    }

    if(getActualPolicy() == ZC_CMPR)  //Zero content Compression enabled
    {
      saves=zeroContentCompression(message);
    }
    
    if(getActualPolicy() == PARALLEL_CMPR)
    {
      saves=parallelCompression(message);
    }

  return saves;
}

int
Compressor_d::deltaCompression(MsgPtr message)
{
int saves_25=0,saves_50=0,saves_75=0,saves=0;

  extractBase(message); //now the base is obtained from the datablock. (8 bytes)

  extractFlitsContent(message);//now other body flits are obtained from the datablock (8*7 bytes)

  resetCompressionMask(); //reset the compression mask. set all zeroes.

  calculateCompressionMask();

  //calculating how many flits can be saved
  saves_25=(countBits(RATIO_25)*2)/8;

  saves_50=(countBits(RATIO_50)*4)/8;

  saves_75=(countBits(RATIO_75)*6)/8;

//DEVO IMPLEMENTARE/CERCARE UN METODO DI MAX TRA NUMERI


          if(saves_25>=saves_50&&saves_25>=saves_75)
          {
            saves=saves_25;
          }
          else
          {
            if(saves_50>=saves_25&&saves_50>=saves_75)
            {
              saves=saves_50;
            }
            else
            {
              if(saves_75>=saves_25&&saves_75>=saves_50)
              {
              saves=saves_75;
              }
            else
              {
                saves=0;
              }
            }
          }

          return saves;
}

int
Compressor_d::zeroContentCompression(MsgPtr message)
{
  int saves=0,index=0,saved_bytes=0,temp_offset=0;
  std::string result="";
  //define zero content compression
  extractFlitsContent(message);//now other body flits are obtained from the datablock (8*8 bytes)
  resetCompressionMask(); //reset the compression mask. set all zeroes.
  int result_length = 0;
  for(int i=0;i<NUM_FLITS_PER_PACKET;i++)
  {
    for(int j=0;j<FLIT_CONTENT_LENGHT;j++)
    { 
      
     //temp_offset=findMSB(m_zero_body_flits[i][j]);//salvo l'offset del MSB del byte
      //m_packet_offsets[index]=temp_offset;
      
      if((int)m_zero_body_flits[i][j]==0)//no zeroes before the number
      {
        m_zero_compression_mask[index]=1;
	result_length++;
      }
      else
      {
        m_zero_compression_mask[index]=0;
	result_length+=8;
      }
      
    }
    index++;
  }

  saved_bytes=NUM_FLITS_PER_PACKET*FLIT_CONTENT_LENGHT-(result_length/8);// 64 bytes (message) - number of resulting bytes after compression
  saves=saved_bytes/8 -1; //numero di flit che risparmio (ogni flit ha 8 byte)
  
  return saves;
}


int
Compressor_d::parallelCompression(MsgPtr message)
{
 //std::cout<<" parallel"<<std::endl;
 int index=0,saved_bytes=0,temp_offset=0,saves_25=0,saves_50=0,saves_75=0,zero_saves=0,delta_saves=0;
  std::string result="";
  extractBase(message); //now the base is obtained from the datablock. (8 bytes)
  //std::cout<<"base extracted\n";
  extractFlitsContent(message);//now other body flits are obtained from the datablock (8*7 bytes)
  //std::cout<<"flit content extracted\n";
  resetCompressionMask(); //reset the compression mask. set all zeroes.
  //std::cout<<"reset com mask done.";
  calculateCompressionMask();
//std::cout<<"calculated"<<std::endl;
int result_length=0;
  for(int i=0;i<NUM_FLITS_PER_PACKET;i++)
  {
    for(int j=0;j<FLIT_CONTENT_LENGHT;j++)
    {

      //temp_offset=findMSB(m_zero_body_flits[i][j]);//salvo l'offset del MSB del byte
      //m_packet_offsets[index]=temp_offset;

      if((int)m_zero_body_flits[i][j]==0)//no zeroes before the number
      {
        m_zero_compression_mask[index]=1;
	result_length++;	
      }
      else
      {
        m_zero_compression_mask[index]=0;
	result_length+=8;

      }
     
    }
    index++;
  }
///std::cout<<"calcolato zero comp saves: ";
  saved_bytes=NUM_FLITS_PER_PACKET*FLIT_CONTENT_LENGHT-(result_length/8);// 64 bytes (message) - number of resulting bytes after compression
  zero_saves=saved_bytes/8 -1; //numero di flit che risparmio (ogni flit ha 8 byte)
  //std::cout<<zero_saves;	
  saves_25=(countBits(RATIO_25)*2)/8;

  saves_50=(countBits(RATIO_50)*4)/8;

  saves_75=(countBits(RATIO_75)*6)/8;

//DEVO IMPLEMENTARE/CERCARE UN METODO DI MAX TRA NUMERI


          if(saves_25>=saves_50&&saves_25>=saves_75)
          {
            delta_saves=saves_25;
          }
          else
          {
            if(saves_50>=saves_25&&saves_50>=saves_75)
            {
              delta_saves=saves_50;
            }
            else
            {
              if(saves_75>=saves_25&&saves_75>=saves_50)
              {
              delta_saves=saves_75;
              }
            else
              {
                delta_saves=0;
              }
            }
          }
	
if(delta_saves > zero_saves)
{
	m_net_ptr->increment_delta_compressions();
	return delta_saves;
}
else
{       is_zero_compression_ready=true;
	m_net_ptr->increment_zero_compressions();
	return zero_saves;
}

}


int
Compressor_d::getActualPolicy(std::string policy)
{
  if(policy == "delta_cmpr")
  {
    return DELTA_CMPR;
  }
  if(policy == "zero_content_cmpr")
  {
    return ZC_CMPR;
  }
  if(policy == "parallel_cmpr")
  {
    return PARALLEL_CMPR;
  }
  return -1;
}
int
Compressor_d::getActualPolicy()
{
  return m_actual_policy;
}

//method used to find the offset of the MSB
int
Compressor_d::findMSB(uint8_t byte)
{
  std::bitset<8> content(byte);
  int offset=0;
  for(int i=content.size()-1;i>=0;i--)
  {
    if(!content.test(i))
    {
      offset++;
    }
    else
    {
      break;
    }
  }
  return offset; //returns the position of the MSB in the byte
}

//method used to crop a byte eliminating its MS 0's
std::string
Compressor_d::cropByte(uint8_t byte, int offset)
{
  std::bitset<8>content(byte);
  std::string result("");
  bool current=false;
  for(int i=offset;i>=0;i--)
  {
    current=content[i];
    if(current == 1)result+=std::string("1");
     if(current == 0)result+=std::string("0");
     //stores the current new bit in the result string
  }
return result;
}

void
Compressor_d::incrementUncompressedFlits(int value){

  m_uncompressed_flits+=value;
}
void
Compressor_d::incrementCompressedFlits(int value){

  m_compressed_flits+=value;
}

double
Compressor_d::getCompressionRatio(){

  return (double)m_compressed_flits/(double)m_uncompressed_flits;
}
double
Compressor_d::getAVGSaves()
{
  return ((double)m_saves)/((double)m_compressed_flits/8);
}

void
Compressor_d::incrementSaves(int value){

  m_saves+=value;
}

void
Compressor_d::stdDev(double value)
{
  m_std_dev+=pow((value - m_avg),2);
}

double
Compressor_d::getStdDev()
{
  return sqrt(m_std_dev/((double)m_compressed_flits/8));
}

int
Compressor_d::getStatisticSaves()
{
  std::normal_distribution<double> distribution(3.88,2.48);
  int number;

  number=(int)abs(distribution(generator));

  return number;
}

void
Compressor_d::updateSavesPerCycles(int saves, int vnet)
{
  m_saves_per_cycles[vnet]+=(double)saves;
}

double
Compressor_d::getSavesPerCycles(int vnet)
{
if(m_compression_cycles_spent[vnet]!=0)return m_saves_per_cycles[vnet]/m_compression_cycles_spent[vnet];
return 0;
}

void
Compressor_d::goAsleep(int vnet, int cycle)
{
  assert(m_is_asleep[vnet]!=true);
  m_is_asleep[vnet]=true;
  m_start_sleep_time[vnet]=cycle;
}

bool
Compressor_d::checkWakeup(int vnet,int cycle,int threshold)
{
  if(cycle - m_start_sleep_time[vnet] <= threshold) return true;
  return false;
}

bool
Compressor_d::isAsleep(int vnet)
{
  return m_is_asleep[vnet];
}

void
Compressor_d::updateCyclesSpent(int vnet)
{
  m_compression_cycles_spent[vnet]+=COMPRESSION_CYCLES;
}

void
Compressor_d::updateCyclesSpent(int vnet,int value)
{
  m_compression_cycles_spent[vnet]+=value;
}


int64_t
Compressor_d::getCyclesSpent(int vnet)
{
return m_compression_cycles_spent[vnet];
}

bool
Compressor_d::isEffective(int hops, int routers)
{
int baseline_flit_cost = 0,baseline_cost = 0;
int compression_flit_cost = 0,compression_cost = 0;
int  num_comp_flits = 0;

baseline_flit_cost = hops*LINK_COST + routers*ROUTER_COST;
baseline_cost = (NUM_FLITS-1)*baseline_flit_cost;

compression_flit_cost = hops*LINK_COST + routers*ROUTER_COST;

num_comp_flits = (1-AVG_COMP_RATIO)*NUM_FLITS;

//here have to think how can pass the num_flit compressed without compress (probably pass the actual compression ratio at runtime and make a prediction)
compression_cost = (num_comp_flits-1)*compression_flit_cost + COMPRESSION_COST + DECOMPRESSION_COST;

if((compression_cost <= baseline_cost)&&(hops > 1)) return true; //<= means that with the same cost, is better to send compressed data, hops > 1 because I decide to compress only packets going outside

return false;

}

std::string
Compressor_d::getCompressionTypes()
{
std::stringstream ss;

ss<<"delta:"<<delta_counter<<"\nzero:"<<zero_counter;
std::string recap(ss.str());
return recap;
}

bool
Compressor_d::isZCReady()
{
	return is_zero_compression_ready;
}

void
Compressor_d::reset_ready_bit()
{
is_zero_compression_ready = false;
}

void
Compressor_d::updateSlackQueue(int vc, int hops)
{
	if(m_slack_queues[vc].size()<31)
	{
	m_slack_queues[vc].push(hops);
	}
	else
	{
	m_slack_queues[vc].pop();
	m_slack_queues[vc].push(hops);
	}
	
}
int
Compressor_d::getPacketSlack(int vc,int hops)
{
	
	int max=0;
	std::queue<int> temp = m_slack_queues[vc];
	for(int i=0;i<temp.size();i++)
	{
	int elem = temp.front();
	
	if(elem > max ) max=elem;
	}
	return hops - max;

}
