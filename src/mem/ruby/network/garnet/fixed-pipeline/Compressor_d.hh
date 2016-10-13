#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_COMPRESSOR_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_COMPRESSOR_D_HH__
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

class Compressor_d :  public ClockedObject, public Consumer
{
public:

	Compressor_d(int id,int virtual_networks, GarnetNetwork_d *network_ptr, NetworkInterface_d *nic, std::string actual_policy);
	~Compressor_d();

    void wakeup();

    bool isIdle(int vc);
    void fillCompressor(int vc,flit_d* flit);
    void prepareForCompression(int vc_in,Tick busy_time,int vc_out);
    int getCompressionCycles();
    bool isFinishedCompression(int vc);
    void compress(int vc_in);
    void extractBase(MsgPtr message_ptr);
    void extractFlitsContent(MsgPtr message_ptr);
    void resetCompressionMask();
    void calculateCompressionMask();
    bool subtract(uint8_t op_1, uint8_t op_2);
    void printCompressionMask();
    void print(std::ostream& out) const;
    
    bool isZCReady(); 
    void reset_ready_bit();

    int countBits(int ratio);
    int analyzeMessage(MsgPtr message);
    int deltaCompression(MsgPtr message);
    int zeroContentCompression(MsgPtr message);
    int parallelCompression(MsgPtr message);
    int getActualPolicy(std::string policy);
    int getActualPolicy();
    int findMSB(uint8_t byte);
    std::string cropByte(uint8_t byte, int offset);
    void incrementUncompressedFlits(int value);
    void incrementCompressedFlits(int value);
    void incrementSaves(int value);
    void stdDev(double value);
    double getCompressionRatio();
    double getAVGSaves();
    double getStdDev();
    int getStatisticSaves();
    void printBase();
    void updateSlackQueue(int vc,int hops);
    int getPacketSlack(int vc,int hops);
		double getSavesPerCycles(int vnet);
		void updateSavesPerCycles(int saves, int vnet);
		void updateCyclesSpent(int vnet);
		void updateCyclesSpent(int vnet,int value);
		void goAsleep(int vnet,int cycle);
		bool checkWakeup(int vnet,int cycle,int threshold);
		bool isAsleep(int vnet);
		int64_t getCyclesSpent(int vnet);
	bool isEffective(int hops,int routers);
	std::string getCompressionTypes();
private:
		GarnetNetwork_d *m_net_ptr;
    int m_virtual_networks, m_num_vcs, m_vc_per_vnet;
    NodeID m_id;
    std::vector<flitBuffer_d*> m_compressor_buffers;
    NetworkInterface_d *m_nic;
    std::vector<Tick> m_compressor_time;
    std::vector<int> m_requests;
    std::vector<int> m_reserved_vcs;
    //for delta compression
    std::vector<uint8_t> m_base;
    //2 vectors below used in both techniques
    std::vector<std::vector<uint8_t>> m_body_flits;
    std::vector<std::vector<uint8_t>> m_zero_body_flits;
    std::vector<bool> m_compression_mask;
    std::vector<bool> m_zero_compression_mask;
    int m_resize;
    //for zero content compression
    int m_actual_policy;
    std::vector<int> m_packet_offsets;
	int zero_counter,delta_counter;
    int m_uncompressed_flits;
    int m_compressed_flits;
    int m_saves;
    double m_avg;
    double m_std_dev;

	bool is_zero_compression_ready;
	
std::vector<std::queue<int>> m_slack_queues;
    std::default_random_engine generator;
		std::vector<double> m_saves_per_cycles;
		std::vector<int> m_start_sleep_time;
		std::vector<bool> m_is_asleep;
		std::vector<int64_t> m_compression_cycles_spent;
    //compressor tracer
    std::ofstream* m_compressor_tracer;
public:

	int
	get_id()
	{
		return m_id;
	}
     std::vector<flitBuffer_d*>
     getModules()
     {
        return m_compressor_buffers;
     }

};
