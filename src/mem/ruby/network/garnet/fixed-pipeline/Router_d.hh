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

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_ROUTER_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_ROUTER_D_HH__

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/network/BasicRouter.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flit_d.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/network/orion/NetworkPower.hh"
#include "params/GarnetRouter_d.hh"


#include "mem/ruby/network/orion/sleep_model/sleep_transistor_model.hh"
#include "mem/ruby/network/orion/sleep_model/sleep_net_log_manager.hh"
#include "mem/ruby/network/orion/sleep_model/simple_power_gating_policy.hh"

#include "mem/ruby/network/orion/OrionConfig.hh"
#include "mem/ruby/network/orion/OrionRouter.hh"

#include "config/use_voq.hh"
#include "config/use_adaptive_routing.hh"
#include "config/use_vc_remap.hh"
#include "config/use_vc_reuse.hh"
#include "config/use_vnet_reuse.hh"
#include "config/use_slow_switch.hh"
#include "config/use_wh_vc_sa.hh"
#include "config/debug_wh_vc_sa.hh"
#include "config/use_release_path_opt.hh"
#include "config/use_spurious_vc_vnet_reuse.hh"
#include "config/use_non_atomic_vc_alloc.hh"
#include "config/use_synthetic_traffic.hh"
#include "config/use_apnea_base.hh"
#include "config/use_pnet.hh"
#include "config/use_moesi_hammer.hh"
#include "config/use_pnet.hh"


#include "config/use_lrc.hh"
#include "config/use_speculative_va_sa.hh"
#include "config/use_fast_free_signal.hh"
#include "config/use_vichar.hh"

#define DEBUG_CREDIT_RTT 0

#define FULLY_ADP_TPDS_ROUTING 1 	// use the natalie condition active or not
#define FADP_BACK_DET_TO_FADP_VC 1	// optimize det->fadp, reinsert escape pkt to adaptive vcs 


class GarnetNetwork_d;
class NetworkLink_d2;
class CreditLink_d;
class InputUnit_d;
class OutputUnit_d;
class Router_PNET_Container_d;

#if USE_VOQ == 0
	#if USE_ADAPTIVE_ROUTING == 0
		class RoutingUnit_d;
		class VCallocator_d;
	#else
		class RoutingUnit_ADAPTIVE_d;
		class VCallocator_ADAPTIVE_d;
	#endif
#else
	class RoutingUnit_VOQ_d;
	class VCallocator_VOQ_d;
#endif


class SWallocator_d;
class Switch_d;
class FaultModel;

class Router_d : public BasicRouter
{
  public:
    typedef GarnetRouter_dParams Params;
    Router_d(const Params *p);

    ~Router_d();

    void init();
    void addInPort(NetworkLink_d *link, CreditLink_d *credit_link);
    void addOutPort(NetworkLink_d *link, const NetDest& routing_table_entry,
                    int link_weight, CreditLink_d *credit_link);

    int get_num_vcs()       { return m_num_vcs; }
    int get_num_vnets()     { return m_virtual_networks; }
    int get_vc_per_vnet()   { return m_vc_per_vnet; }
    int get_num_inports()   { return m_input_unit.size(); }
    int get_num_outports()  { return m_output_unit.size(); }
    int get_id()            { return m_id; }

	//#if USE_PNET==1
	private:
		int m_minor_id;
		Router_PNET_Container_d* m_container_ptr;
	public:
		void setMinorId(int minor){m_minor_id=minor;}
		int getMinorId(){return m_minor_id;};
		void setObjectId();
		void setPnetContainerPtr(Router_PNET_Container_d* ptr){m_container_ptr=ptr;}
	//#endif

    virtual std::string getObjectId() const { return m_objectId; }

    void init_net_ptr(GarnetNetwork_d* net_ptr) 
    { 
        m_network_ptr = net_ptr; 
    }

    GarnetNetwork_d* get_net_ptr()                  { return m_network_ptr; }
    std::vector<InputUnit_d *>& get_inputUnit_ref()   { return m_input_unit; }
    std::vector<OutputUnit_d *>& get_outputUnit_ref() { return m_output_unit; }

    void update_sw_winner(int inport, flit_d *t_flit);
    void update_incredit(int in_port, int in_vc, int credit);
    void route_req(flit_d *t_flit, InputUnit_d* in_unit, int invc);
    void vcarb_req();
    void swarb_req();
    void printFaultVector(std::ostream& out);
    void printAggregateFaultProbability(std::ostream& out);

    double calculate_power();
    double calculate_power_resettable();

    void calculate_performance_numbers(); /*this is on standard vars*/
    void calculate_performance_numbers_resettable();
    void reset_performance_numbers();

/* START added func for the sleep model evaluation and st_model logger
 * (implemented in ./orion/sleep_model/sleep_network_power.[cc,hh])
 */
    /*first func to be called in router_d::init after all the router_d init funcs*/
    void init_sleep_model();
    //compute the power for the gated blocks
    void init_power_model();
    /*this is for sleep transistor model implemented in orion/networkpower.cc*/
    void calculate_sleep_network();

    void init_rt_logger_model();

/* these funcs use rt_vars to be independent from standard functions used for the
 * baseline orion model
 */
    void calculate_performance_number_buf(bool _per_in_port=false /*or unified*/);
    void calculate_performance_number_va(bool _per_in_port=false /*or unified*/);
    void calculate_performance_number_sa(bool _per_vnet=false /*or unified*/);
    void calculate_performance_number_sw(bool _per_vnet=false /*or unified*/);
    void calculate_performance_numbers_run_time(bool _per_in_port_buf=false,
						bool _per_in_port_va=false,
						bool _per_vnet_sa=false,
						bool _per_vnet_crb=false);

    PowerContainerRT /*double*/ calculate_power_buf(Tick startEpoch,bool _per_in_port=false /*or unified*/);
    PowerContainerRT /*double*/ calculate_power_va(Tick startEpoch,bool _per_in_port=false /*or unified*/);
    PowerContainerRT /*double*/ calculate_power_sa(Tick startEpoch,bool _per_vnet=false /*or unified*/);
    PowerContainerRT /*double*/ calculate_power_sw(Tick startEpoch,bool _per_vnet=false /*or unified*/);
    PowerContainerRT /*double*/ calculate_power_run_time(Tick startEpoch,double VddActual=1.0,bool _per_in_port_buf=false,
						bool _per_in_port_va=false,
						bool _per_vnet_sa=false,
						bool _per_vnet_crb=false);


    long get_frequency(){return router_frequency;};
	long get_frequency_adaptive(){return router_frequency_adaptive;};
    void set_frequency_adaptive(long new_freq){router_frequency_adaptive=new_freq;};

    virtual void changePeriod(Tick period);
    
    void setVoltageRegulator(VoltageRegulatorBase *vreg)
    {
        assert(this->vreg==0);
        this->vreg=vreg;
    }
    
    double averageVoltage()
    {
        assert(vreg);
        return vreg->averageVoltage();
    }

/*
	implementation in sleep_model/router_d_power_state_control.[cc,hh]
	POWER POLICY FUNCTIONS
*/
    void setPowerStateInBuf(bool is_on_,Tick when,int in_port);
    void setPowerStateOutBuf(bool is_on_,Tick when,int out_port);
    void setPowerStateVA(bool is_on_,Tick when);
    void setPowerStateSA(bool is_on_,Tick when);
    void setPowerStateSW(bool is_on_,Tick when);


    bool getPowerStateInBuf(Tick when=0,int in_port=0);
    bool getPowerStateOutBuf(Tick when=0,int out_port=0);
    bool getPowerStateVA(Tick when=0);
    bool getPowerStateSA(Tick when=0);
    bool getPowerStateSW(Tick when=0);

    bool getNextPowerStateInBuf(Tick when=0,int in_port=0);
    bool getNextPowerStateOutBuf(Tick when=0,int out_port=0);
    bool getNextPowerStateVA(Tick when=0);
    bool getNextPowerStateSA(Tick when=0);
    bool getNextPowerStateSW(Tick when=0);

	// retrive actual power state for each block
    bool getActualPowerStateInBuf(int in_port=0);
    bool getActualPowerStateOutBuf(int out_port=0);
    bool getActualPowerStateVA(int useless=0); //for std::bind
    bool getActualPowerStateSA(int useless=0);
    bool getActualPowerStateSW(int useless=0);
 	
	// retrive actual power state for each block an eps before this tick
    bool getActualPowerStateInBufMinusEps(int in_port=0);
    bool getActualPowerStateOutBufMinusEps(int out_port=0);
    bool getActualPowerStateVAMinusEps(int useless=0); //for std::bind
    bool getActualPowerStateSAMinusEps(int useless=0);
    bool getActualPowerStateSWMinusEps(int useless=0);       

    Tick getTickChangePowerStateInBuf(int in_port=0);
    Tick getTickChangePowerStateOutBuf(int out_port=0);
    Tick getTickChangePowerStateVA();
    Tick getTickChangePowerStateSA();
    Tick getTickChangePowerStateSW();
/*
	LOGGER FUNCTIONS
	implementation in sleep_model/router_d_logger_control.[cc,hh]

	block ={0 InBuf,1 VA,2 SA,3 SW}
	subblock ={0 InBuf,1 VA,2 SA,3 SW}
*/
	void setLoggerEpochInterval(int block, int subblock,uint32_t interval=1);	
	void setLoggerStartEpoch(int block, int subblock,Tick start=curTick());
	void setLoggerSchedWakeUp(int block, int subblock, uint32_t delay=1);
	void setLoggerAutoSched(int block, int subblock, bool auto_sched=false);
	void setLoggerPrintStatus(int block, int subblock, bool print=false);

	void pushBackLoggerEpoch(int block, int subblock);

	bool getLoggerAutoSched(int block, int subblock);
	uint32_t getLoggerEpochInterval(int block, int subblock);
	bool getLoggerPrintStatus(int block, int subblock);

	void printLogger(std::ostream& out, int block, int subblock);
	void printLoggerCSV(std::string filename, int block, int subblock);

	void setEpochIntervalLogger(int,int, uint32_t=1);
	/*{buf,va,sa,sw}, which block,interval*/

	uint32_t getInitialPolicyDelay(){return this->initial_policy_delay;};
/*END run-time power (and sleep model) functions*/

    double get_dynamic_power(){return m_power_dyn;}
    double get_static_power(){return m_power_sta;}
    double get_clk_power(){return m_clk_power;}
    bool get_fault_vector(int temperature, float fault_vector[]){ 
        return m_network_ptr->fault_model->fault_vector(m_id, temperature, 
                                                        fault_vector); 
    }
    bool get_aggregate_fault_probability(int temperature, 
                                         float *aggregate_fault_prob){
        return m_network_ptr->fault_model->fault_prob(m_id, temperature, 
                                                      aggregate_fault_prob);
    }
    
    

    int getCongestion() const; //Sampled number of flits of this router only
    int getFullCongestion();
    void printFullCongestionData(std::ostream& dump); //Also neighbors
    void printRouterPower(std::ostream& outfreq, double VddActual=1.0);
	void computeRouterPower(double VddActual=1.0);
	/*new functions to evaluate power using power gating actuator*/
	void printSWPower(std::ostream& dump); // for the Switch


	double getFinalAvgTotalPower(){return avg_pwr_total/avg_pwr_tick_elapsed;}
	double getFinalAvgClockPower(){return avg_pwr_clock/avg_pwr_tick_elapsed;}
	double getFinalAvgStaticPower(){return avg_pwr_static/avg_pwr_tick_elapsed;}
	double getFinalAvgDynamicPower(){return avg_pwr_dynamic/avg_pwr_tick_elapsed;}

	void updateFinalAvgPwr(PowerContainerRT res_pwr)
	{
		Tick tmp_duration=(curTick()-last_power_calc_all_file);
		assert(tmp_duration>=0);
		avg_pwr_total+= res_pwr.PtotBlock*tmp_duration;
		avg_pwr_clock+= res_pwr.PdynClock*tmp_duration;
		avg_pwr_static+= res_pwr.PstaticBlock*tmp_duration;
		avg_pwr_dynamic+= res_pwr.PdynBlock*tmp_duration;

		//std::cout<<"avg_pwr_total("<<avg_pwr_total
		//		<<"( += res_pwr.PtotBlock("<<res_pwr.PtotBlock
		//		<<") *tmp_duration("<<tmp_duration<<")"<<std::endl;

		avg_pwr_tick_elapsed+=tmp_duration;
	}

	Tick getAvgPwrTickElapsed(){return avg_pwr_tick_elapsed;}

	//Speculative statistics
	static float global_arbit_count;
        float local_arbit_count;

	float getLocalCount(){return local_arbit_count;}
	void incLocalCount(){local_arbit_count++;}	
	void decLocalCount(){local_arbit_count--;}
	static void incGlobalCountStatic(){global_arbit_count++;}
	static void decGlobalCountStatic(){global_arbit_count--;}	

	static float va_global_arbit_count;
        float va_local_arbit_count;

	float getVaLocalCount(){return va_local_arbit_count;}
	void incVaLocalCount(){va_local_arbit_count++;}	
	void decVaLocalCount(){va_local_arbit_count--;}
	static void incVaGlobalCountStatic(){va_global_arbit_count++;}
	static void decVaGlobalCountStatic(){va_global_arbit_count--;}

	static float va_win_global_arbit_count;
        float va_win_local_arbit_count;

	float getVaWinLocalCount(){return va_win_local_arbit_count;}
	void incVaWinLocalCount(){va_win_local_arbit_count++;}	
	void decVaWinLocalCount(){va_win_local_arbit_count--;}
	static void incVaWinGlobalCountStatic(){va_win_global_arbit_count++;}
	static void decVaWinGlobalCountStatic(){va_win_global_arbit_count--;}

	static float sa_global_arbit_count;
        float sa_local_arbit_count;

	float getSaLocalCount(){return sa_local_arbit_count;}
	void incSaLocalCount(){sa_local_arbit_count++;}	
	void decSaLocalCount(){sa_local_arbit_count--;}
	static void incSaGlobalCountStatic(){sa_global_arbit_count++;}
	static void decSaGlobalCountStatic(){sa_global_arbit_count--;}

	static float saspec_global_arbit_count;
        float saspec_local_arbit_count;

	float getSaSpecLocalCount(){return saspec_local_arbit_count;}
	void incSaSpecLocalCount(){saspec_local_arbit_count++;}	
	void decSaSpecLocalCount(){saspec_local_arbit_count--;}
	static void incSaSpecGlobalCountStatic(){saspec_global_arbit_count++;}
	static void decSaSpecGlobalCountStatic(){saspec_global_arbit_count--;}

	static float sa_nocred_global_arbit_count;
        float sa_nocred_local_arbit_count;

	float getSaNoCredLocalCount(){return sa_nocred_local_arbit_count;}
	void incSaNoCredLocalCount(){sa_nocred_local_arbit_count++;}	
	static void incSaNoCredGlobalCountStatic(){sa_nocred_global_arbit_count++;}

	static float va_nobuff_global_arbit_count;
        float va_nobuff_local_arbit_count;

	float getVaNoBuffLocalCount(){return va_nobuff_local_arbit_count;}
	void incVaNoBuffLocalCount(){va_nobuff_local_arbit_count++;}	
	static void incVaNoBuffGlobalCountStatic(){va_nobuff_global_arbit_count++;}

	//END Speculative statistics

  //BUBBLE counter
	static float bubble_total_count;
	static float bubble_degradation;
	static float bubble_no_degradation;
  
	static void incBubbleTotalCountStatic(){bubble_total_count++;}
	static void incBubbleDegradationStatic(){bubble_degradation++;}
	static void incBubbleNoDegradationStatic(){bubble_no_degradation++;}
	
	//BUBBLE COUNTER end

  private:

    int m_virtual_networks, m_num_vcs, m_vc_per_vnet;
    GarnetNetwork_d *m_network_ptr;

    std::vector<double> buf_read_count;
    std::vector<double> buf_write_count;
    std::vector<double> vc_local_arbit_count;
    std::vector<double> vc_global_arbit_count;
    double sw_local_arbit_count, sw_global_arbit_count;
    double crossbar_count;


/* added for run-time power computation (??? why double ???)*/

    std::vector<double> buf_read_count_rt;
    std::vector<double> buf_write_count_rt;
    std::vector<double> vc_local_arbit_count_rt;
    std::vector<double> vc_global_arbit_count_rt;
    double sw_local_arbit_count_rt, sw_global_arbit_count_rt;
    double crossbar_count_rt;   

    std::vector<double> buf_read_count_rt_prev;
    std::vector<double> buf_write_count_rt_prev;
    std::vector<double> vc_local_arbit_count_rt_prev;
    std::vector<double> vc_global_arbit_count_rt_prev;
    double sw_local_arbit_count_rt_prev, sw_global_arbit_count_rt_prev;
    double crossbar_count_rt_prev;

 
    OrionConfig* orion_cfg_ptr;
    OrionRouter* orion_rtr_ptr;
    long router_frequency; /*Hz*/
 	long router_frequency_adaptive; /*Hz*/
    // gating network model for logic blocks 
    std::vector<std::vector<Sleep_transistor_model> > stm; 

    // loggers for dynamic policy evaluation
    std::vector<std::vector<SleepNetLogManager> > logger;

    SimplePowerGatingPolicy *powerGatingPolicy;
    uint32_t initial_policy_delay;
/*END added run-time power estimation vars*/


    std::vector<InputUnit_d *> m_input_unit;
    std::vector<OutputUnit_d *> m_output_unit;

/* manage the different definitions for the routing unit and VA_unit*/
#if USE_VOQ == 0
	#if USE_ADAPTIVE_ROUTING == 0
		RoutingUnit_d *m_routing_unit; //TODO ufficio106 HANDS
		VCallocator_d *m_vc_alloc;
	#else
		RoutingUnit_ADAPTIVE_d *m_routing_unit; //TODO ufficio106 HANDS
		VCallocator_ADAPTIVE_d *m_vc_alloc;
	#endif
#else 
	RoutingUnit_VOQ_d *m_routing_unit;
	VCallocator_VOQ_d *m_vc_alloc;
#endif
/****************************************************/
    
    SWallocator_d *m_sw_alloc;
    Switch_d *m_switch;

    double m_power_dyn;
    double m_power_sta;
    double m_clk_power;
    
    std::string m_objectId;
    VoltageRegulatorBase *vreg;
public:

	SWallocator_d* getSWallocatorUnit(){return m_sw_alloc;}
	#if USE_ADAPTIVE_ROUTING == 0
	RoutingUnit_d* getRoutingUnit(){return m_routing_unit;}		
	VCallocator_d* getVCallocatorUnit(){return m_vc_alloc;}
	#else
	RoutingUnit_ADAPTIVE_d* getRoutingUnit(){return m_routing_unit;}	
	VCallocator_ADAPTIVE_d* getVCallocatorUnit(){return m_vc_alloc;}
	#endif

	//used for power gating actuation
	Switch_d* getSwitch(){return m_switch;}

	std::vector<NetDest>& getRoutingTable();
	std::vector<int>& getRoutingTableWeights();

		class CongestionMetrics
		{
			public:
				CongestionMetrics():count(0),t_start(Cycles(-1)){};
				Tick count;
				Cycles t_start;
		};
	
	std::vector<std::vector<CongestionMetrics> >& getC1Matrix(){return c1_matrix;}
	std::vector<std::vector<CongestionMetrics> >& getC2Matrix(){return c2_matrix;}


	Tick getC1(int outport);
	Tick getC2(int outport);

	void resetInPktOutport(int outport)
	{
		assert(outport>=0&&outport<c1_in_pkt.size());
		c1_in_pkt[outport]=0;
	}
	void resetInPktVector()
	{
		for(int i=0;i<c1_in_pkt.size();i++)
			c1_in_pkt[i]=0;
	}

	void incrementInPkt(int outport,int incr=1)
	{
		assert(outport>=0&&outport<c1_in_pkt.size());
		c1_in_pkt[outport]+=incr;
	}

	int getInPktOutport(int outport)
	{
		assert(outport>=0&&outport<c1_in_pkt.size());
		return c1_in_pkt.at(outport);
	}

	void resetOutFlits(){m_outFlits=0;}
	int getOutFlits(){return m_outFlits;}
	void incrementOutFlits(){m_outFlits++;}
private:
	/*congestion metrics*/
	std::vector<std::vector<CongestionMetrics> > c1_matrix;

	std::vector<std::vector<CongestionMetrics> > c2_matrix;

	std::vector<int>  c1_in_pkt;

	int m_outFlits;

	Tick last_power_calc_all_file;
	Tick last_power_calc_sw_file;

	double avg_pwr_total;
	double avg_pwr_clock;
	double avg_pwr_static;
	double avg_pwr_dynamic;
	Tick avg_pwr_tick_elapsed;


	///////////////////////////////
	//// for the buffer usage /////
		
	std::vector<std::vector<Tick>> used_buf_inport; 
	std::vector<std::vector<Tick>> last_used_buf_inport;
	Tick last_computed_used_buf_inport_tick;
	// each time a buf is released or used in an inputport evaluate the buf used
	// on that inport times the time slot elapsed from the previous evaluation
	// interval[in tick] * #used_buf
	//////////////////////////////
	public:
		std::vector<std::vector<Tick>>& getUsedBufInport(){ return used_buf_inport;} 
		std::vector<std::vector<Tick>>& getLastUsedBufInport(){ return last_used_buf_inport;}
		
		void computeAndPrintBufUsageRunTime(std::ostream& out_f)
		{
			for(int ip_iter=0;ip_iter<get_num_inports();ip_iter++)
			{
				for(int vc_iter=0;vc_iter<m_num_vcs;vc_iter++)
				{
					Tick num = used_buf_inport[ip_iter][vc_iter]-last_used_buf_inport[ip_iter][vc_iter];
					Tick den = curTick() - last_computed_used_buf_inport_tick;
					// update last counter usages
					last_used_buf_inport[ip_iter][vc_iter]=used_buf_inport[ip_iter][vc_iter];
					//output to file
					if(vc_iter==0 && ip_iter==0)
						out_f<< (double)num/den;
					else
						out_f<<","<< (double)num/den;
				}				
			}
			last_computed_used_buf_inport_tick = curTick();
			out_f<<std::endl;
		}
	// spurious vnets for the pkt_switching methodology
	int getVirtualNetworkProtocol(){return m_virtual_networks_protocol;}
	int getVirtualNetworkSpurious(){return m_virtual_networks_spurious;}	
	private:
	int m_virtual_networks_protocol;
	int m_virtual_networks_spurious;

#if USE_SPECULATIVE_VA_SA==1
    public:	
		flit_d* spec_get_switch_buffer_flit(int inport);	
    	bool spec_get_switch_buffer_ready(int inport, Time curTime);
#endif

/* NON_ATOMIC_VC_ALLOC*/
	public:
		int get_tot_atomic_pkt() { return m_network_ptr->get_tot_atomic_pkt();}

//Function Used for DEBUG follow packet throw the pipeline
    public:
		void print_pipeline_state(flit_d* t_flit, std::string stage);
		void print_route_time(flit_d* t_flit, std::string stage);
      bool get_m_wh_path_busy(int inport, int invc, int outport);
      void set_m_wh_path_busy(bool set, int inport, int invc,  int outport);	
    //Buffer reuse statistics
    int buffer_reused_count;
    int head_and_tail_count;
    int only_headtail_count;
    int get_buffer_reused_count(){return buffer_reused_count;}
    void add_buffer_reused_count(){buffer_reused_count++;}
    int get_head_and_tail_count(){return head_and_tail_count;}
    int get_only_headtail_count(){return only_headtail_count;}
    void add_head_and_tail_count(){head_and_tail_count++;}
    void add_only_headtail_count(){only_headtail_count++;}
    
#if USE_VICHAR==1
	private:
	/////////////////////////////////////
	///// VICHAR SUPPORT ////////////////
	//NOTE: vichar bufdepth is max pkt len since it does not matter, as the
	//number of VCs since it stops floding flits when no flit slots are
	//available downstream. Thus the only interesting value is the available
	//slots per vnet and the usedSlots per vnet passed by the router.
	int totVicharSlotPerVnet;
	public:
		int getTotVicharSlotPerVnet()
		{
			assert( totVicharSlotPerVnet>0); 
			return totVicharSlotPerVnet; 
		};
	/////////////////////////////////////
#endif
#if USE_LRC == 1
	void swarb_req_LRC(); // schedule in the same cycle
	void vcarb_req_LRC(); // schedule in the same cycle
	int calledVAthisCycle;
#endif

#if USE_APNEA_BASE==1
	private:
		std::vector<int> initVcOp;
		std::vector<int> finalVcOp;
		std::vector<int> initVcIp;
		std::vector<int> finalVcIp;
	public:
		int getInitVcOp(int op){return initVcOp.at(op);checkFeasibilityVcSetOp(op);};
		int getFinalVcOp(int op){return finalVcOp.at(op);checkFeasibilityVcSetOp(op);};
		int getInitVcIp(int ip){return initVcIp.at(ip);checkFeasibilityVcSetIp(ip);};
		int getFinalVcIp(int ip){return finalVcIp.at(ip);checkFeasibilityVcSetIp(ip);};

		void setInitVcOp(int op,int val){initVcOp[op]=val;checkFeasibilityVcSetOp(op);};
		void setFinalVcOp(int op,int val){finalVcOp[op]=val;checkFeasibilityVcSetOp(op);};
		void setInitVcIp(int ip,int val){initVcIp[ip]=val;checkFeasibilityVcSetIp(ip);};
		void setFinalVcIp(int ip,int val){finalVcIp[ip]=val;checkFeasibilityVcSetIp(ip);};

		void incInitVcOp(int op){initVcOp[op]=initVcOp[op]+1;checkFeasibilityVcSetOp(op);};
		void incFinalVcOp(int op) {finalVcOp[op]=finalVcOp[op]+1;checkFeasibilityVcSetOp(op);};
		void incInitVcIp(int ip){initVcIp[ip]=initVcIp[ip]+1;checkFeasibilityVcSetIp(ip);};
		void incFinalVcIp(int ip){finalVcIp[ip]=finalVcIp[ip]+1;checkFeasibilityVcSetIp(ip);};
		
		void checkFeasibilityVcSetOp(int op){ assert(initVcOp[op]>=0 && finalVcOp[op]<=m_num_vcs);}
		void checkFeasibilityVcSetIp(int ip){ assert(initVcIp[ip]>=0 && finalVcIp[ip]<=m_num_vcs);}
#endif

	public:
		int getNumFifoResynchSlots(){return m_num_fifo_resynch_slots;}
	protected:	
		int m_num_fifo_resynch_slots;

};


#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_ROUTER_D_HH__
