#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <iostream>
#include <string>
#include <sstream>

#include <numeric>
#include <algorithm>
#include <functional>

#include "mem/ruby/network/orion/NetworkPower.hh"
#include "mem/ruby/network/orion/OrionConfig.hh"
#include "mem/ruby/network/orion/OrionLink.hh"
#include "mem/ruby/network/orion/OrionRouter.hh"

#include "base/types.hh"
#include "base/stl_helpers.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/SWallocator_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Switch_d.hh"

#include "mem/ruby/network/orion/TechParameter.hh"



#if USE_VOQ == 0
	#if USE_ADAPTIVE_ROUTING == 0
		#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_d.hh"
		#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_d.hh"
	#else
		#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_ADAPTIVE_d.hh"
		#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_ADAPTIVE_d.hh"		
	#endif
#else
	#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_VOQ_d.hh"
	#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_VOQ_d.hh"
#endif


#define VDD	1

void
Router_d::init_sleep_model()
{
	// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);
	
/*prepare objects for sleep network*/
	stm.resize(4);
	for(int i=0;i<stm.size();i++)
		stm[i].resize(1);

	//moved to router_d.cc::init
	//init_power_model();//init orion power model in GEM5 1 for each router
	
	calculate_sleep_network(); /*FIXME block area is not computed yet*/
	
	for (int i=0;i<stm.size();i++)
	{
		for(int j=0;j<stm[i].size();j++)
		{
			stm[i][j].optimize_ST_base(); /* retrive the best sleep transistor
			given parameters in both constructor and define file*/
			stm[i][j].get_opt_st_parameters(); /*get Ion Ioff Ron of the optimal ST*/
			stm[i][j].get_router_R_requirements(); /*required R sleep value over 3 constraints*/
			stm[i][j].get_st_area(); /*get sleep transistor area*/
			stm[i][j].compute_sleep_network();
		}
	}
	//area
	stm[0][0].set_block_area(orion_cfg_ptr->router_area_in_buf());
	stm[1][0].set_block_area(orion_cfg_ptr->router_area_vcallocator());
	stm[2][0].set_block_area(orion_cfg_ptr->router_area_swallocator());
	stm[3][0].set_block_area(orion_cfg_ptr->router_area_crossbar());


// PRINT FOUND CONFIGURATION
		
	std::stringstream name("");
	for (int i=0;i<stm.size();i++)
	{
		for(int j=0;j<stm[i].size();j++)
		{
			name<<"router"<<m_id<<".block_"<<i;
			//stm[i][j].printBlockArea(std::cout,name.str());
			//stm[i][j].printBlockPower(std::cout,name.str());
			stm[i][j].printSleepTransistorConfig(std::cout,name.str());
			name.str("");
		}
	}


// moved to router_d.cc::init to allow rt power estimation for both power gating
// and dfs

/*set up additional variables for run-time power evaluation*/
//	buf_read_count_rt.resize(m_virtual_networks);
//	buf_write_count_rt.resize(m_virtual_networks);
//	vc_local_arbit_count_rt.resize(m_virtual_networks);
//	vc_global_arbit_count_rt.resize(m_virtual_networks);
//	
//	buf_read_count_rt_prev.resize(m_virtual_networks);
//	buf_write_count_rt_prev.resize(m_virtual_networks);
//	vc_local_arbit_count_rt_prev.resize(m_virtual_networks);
//	vc_global_arbit_count_rt_prev.resize(m_virtual_networks);
//	for (int i = 0; i < m_virtual_networks; i++) 
//	{
//       		 buf_read_count_rt[i] = 0;
//       		 buf_write_count_rt[i] = 0;
//       		 vc_local_arbit_count_rt[i] = 0;
//       		 vc_global_arbit_count_rt[i] = 0;
//
//       		 buf_read_count_rt_prev[i] = 0;
//       		 buf_write_count_rt_prev[i] = 0;
//       		 vc_local_arbit_count_rt_prev[i] = 0;
//       		 vc_global_arbit_count_rt_prev[i] = 0;
//	}
//	crossbar_count_rt = 0;
//	sw_local_arbit_count_rt = 0;
//	sw_global_arbit_count_rt = 0;
//
//	crossbar_count_rt_prev = 0;
//	sw_local_arbit_count_rt_prev = 0;
//	sw_global_arbit_count_rt_prev = 0;

}

/* This function initializes the orion power model embedded in GEM5. For each
 * Router_d object it init the orion_cfg_ptr and orion_rtr_ptr .
*/
void
Router_d::init_power_model()
{
    int num_active_vclass = 0;
    std::vector<bool > active_vclass_ary;
    active_vclass_ary.resize(m_virtual_networks);

    for (int i =0; i < m_virtual_networks; i++) 
    {
        active_vclass_ary[i] = (get_net_ptr())->validVirtualNetwork(i);
        if (active_vclass_ary[i]) 
		{
            num_active_vclass++;
        }
    }
    const string cfg_fn = "src/mem/ruby/network/orion/router.cfg";
    orion_cfg_ptr = new OrionConfig(cfg_fn);

    std::vector<uint32_t > vclass_type_ary;

    for (int i = 0; i < m_virtual_networks; i++) 
    {
        if (active_vclass_ary[i]) 
		{
		#if USE_PNET==0
            int temp_vc = i*m_vc_per_vnet;
            vclass_type_ary.push_back((uint32_t) 
            m_network_ptr->get_vnet_type(temp_vc));
		#else // USE_PNET==1
			//only a specific vnet has to be considered per each router
			//depending on its m_minor_id
            if(m_minor_id==i)
			{
				int temp_vc = i*m_vc_per_vnet;
            	vclass_type_ary.push_back((uint32_t) 
            	m_network_ptr->get_vnet_type(temp_vc));
			}
		#endif
        }
    }
	#if USE_PNET==0
    assert(vclass_type_ary.size() == num_active_vclass);
	#else
	//single vnet is used for each router in the PNET impl
    assert(vclass_type_ary.size() == 1);
	num_active_vclass=1;
	#endif

std::cout<<"num_active_vclass:"<<num_active_vclass
			<<" vclass_type_ary:"<<vclass_type_ary.size()
			<<" m_vc_per_vnet:"<<m_vc_per_vnet
			<<std::endl;
    orion_rtr_ptr = new OrionRouter(
        m_input_unit.size(),
        m_output_unit.size(),
        num_active_vclass,
        vclass_type_ary,
        m_vc_per_vnet,
        m_network_ptr->getBuffersPerDataVC(), /*input buffered*/
        m_network_ptr->getBuffersPerCtrlVC(), /*input buffered*/
        (m_network_ptr->getNiFlitSize() * 8), /*flith width in bits*/
        orion_cfg_ptr
    );

}

void 
Router_d::calculate_sleep_network()
{

        // Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);


    //Network Activities from garnet
    calculate_performance_numbers();

    // Number of virtual networks/message classes declared in Ruby
    // maybe greater than active virtual networks.
    // Estimate active virtual networks for correct power estimates
    int num_active_vclass = 0;
    std::vector<bool > active_vclass_ary;
    active_vclass_ary.resize(m_virtual_networks);

    std::vector<double > vc_local_arbit_count_active;
    std::vector<double > vc_global_arbit_count_active;
    std::vector<double > buf_read_count_active;
    std::vector<double > buf_write_count_active;

    for (int i =0; i < m_virtual_networks; i++) {

        active_vclass_ary[i] = (get_net_ptr())->validVirtualNetwork(i);
        if (active_vclass_ary[i]) {
            num_active_vclass++;
            vc_local_arbit_count_active.push_back(vc_local_arbit_count[i]);
            vc_global_arbit_count_active.push_back(vc_global_arbit_count[i]);
            buf_read_count_active.push_back(buf_read_count[i]);
            buf_write_count_active.push_back(buf_write_count[i]);
        }
        else {
            // Inactive vclass
            assert(vc_global_arbit_count[i] == 0);
            assert(vc_local_arbit_count[i] == 0);
        }
    }



    static double freq_Hz;
    freq_Hz = this->router_frequency; 

    uint32_t num_in_port = m_input_unit.size();
    //uint32_t num_out_port = m_output_unit.size();
    uint32_t num_vclass = num_active_vclass;

    uint32_t num_vc_per_vclass = m_vc_per_vnet;

    /* compute Ron, Ceq for the logic blocks. We focus on the block actually
     * available using orion and garnet, i.e. buffers, crossbar, vcallocator,
     * swallocator
    */
    //Power variables to extract Ceq for some blocks
    double Pbuf_wr_dyn = 0.0; double Pbuf_rd_dyn = 0.0;
    double Pvc_arb_local_dyn = 0.0; double Pvc_arb_global_dyn = 0.0;
    double Psw_arb_local_dyn = 0.0;  double Psw_arb_global_dyn = 0.0;
    double Pxbar_dyn = 0.0; 

    double Pbuf_sta = 0.0; 
    double Pvc_arb_sta = 0.0;
    double Psw_arb_sta = 0.0;
    double Pxbar_sta = 0.0;

    //Dynamic Power

    for (int i = 0; i < num_vclass; i++) 
    {
        Pbuf_wr_dyn += orion_rtr_ptr->calc_dynamic_energy_buf(i, WRITE_MODE, true)* freq_Hz;
        Pbuf_rd_dyn += orion_rtr_ptr->calc_dynamic_energy_buf(i, READ_MODE, true)* freq_Hz;

        Pvc_arb_local_dyn += orion_rtr_ptr->calc_dynamic_energy_local_vc_arb(i,num_vc_per_vclass/2, true) * freq_Hz;
        Pvc_arb_global_dyn += orion_rtr_ptr->calc_dynamic_energy_global_vc_arb(i,num_in_port/2, true) * freq_Hz;
    }
    std::cout<<Pbuf_wr_dyn<<" "<<Pbuf_rd_dyn<<std::endl;

    Psw_arb_local_dyn = orion_rtr_ptr->calc_dynamic_energy_local_sw_arb(num_vclass*num_vc_per_vclass/2, true) * freq_Hz;
    Psw_arb_global_dyn = orion_rtr_ptr->calc_dynamic_energy_global_sw_arb(num_in_port/2, true) * freq_Hz;

    Pxbar_dyn = orion_rtr_ptr->calc_dynamic_energy_xbar(true) * freq_Hz;

    m_clk_power = orion_rtr_ptr->calc_dynamic_energy_clock()*freq_Hz;

    // Static Power
    Pbuf_sta = orion_rtr_ptr->get_static_power_buf();
    Pvc_arb_sta = orion_rtr_ptr->get_static_power_va();
    Psw_arb_sta = orion_rtr_ptr->get_static_power_sa();
    Pxbar_sta = orion_rtr_ptr->get_static_power_xbar();


	std::vector<std::pair<double,double>> pwr;
	pwr.push_back(std::pair<double,double>(Pbuf_wr_dyn+Pbuf_rd_dyn ,Pbuf_sta));
	pwr.push_back(std::pair<double,double>(Pvc_arb_local_dyn+Pvc_arb_global_dyn,Pvc_arb_sta));
	pwr.push_back(std::pair<double,double>(Psw_arb_local_dyn+Psw_arb_global_dyn,Psw_arb_sta));
	pwr.push_back(std::pair<double,double>(Pxbar_dyn ,Pxbar_sta));
		
	for(int i=0;i<pwr.size();i++)
	{
		for(int j=0;j<stm[i].size();j++)
		{
			//set statistics for each block
			stm[i][j].set_block_power_dyn(pwr[i].first);
			stm[i][j].set_block_power_static(pwr[i].second);
			stm[i][j].set_block_Imax( (pwr[i].first)/VDD);
			stm[i][j].set_block_Ceq( (pwr[i].first)/(VDD*VDD*freq_Hz));
			stm[i][j].set_block_Rswitch( (pwr[i].first)/ ( ( (pwr[i].first)/VDD)*( (pwr[i].first)/VDD) ));
		}
	}
}

void
Router_d::init_rt_logger_model()
{
	logger.resize(4);

// logger for aggregate buffer
	std::stringstream str("");
	str<<"Buffer_Router_"<<m_id;
	logger[0].push_back(SleepNetLogManager(str.str(),
			0, /*this is the log id inside the log vector*/
			this,
			std::bind(&Router_d::getActualPowerStateInBufMinusEps,this,std::placeholders::_1),
			std::bind(&Router_d::calculate_performance_number_buf,this,false),
			std::bind(&Router_d::calculate_power_buf,this,std::placeholders::_1,false),
			false /*print active or not*/,
			false /*autoreload wakeup event*/,
			100000/*epoch length in tick*/,
			&stm[0][0]));

	//logger[0][0].scheduleEvent(1000);

// logger for aggregate vc allocator
	str.str("");
	str<<"VCallocator_Router_"<<m_id;
	logger[1].push_back(SleepNetLogManager(str.str(),
			0, /*this is the log id inside the log vector*/
			this,
			std::bind(&Router_d::getActualPowerStateVAMinusEps,this,std::placeholders::_1),
			std::bind(&Router_d::calculate_performance_number_va,this,false),
			std::bind(&Router_d::calculate_power_va,this,std::placeholders::_1,false),
			false,
			false,
			100000,
			&stm[1][0]));

	//logger[1][0].scheduleEvent(1000);

// logger for aggregate sw allocator
	str.str("");
	str<<"SWallocator_Router_"<<m_id;
	logger[2].push_back(SleepNetLogManager(str.str(),
			0, /*this is the log id inside the log vector*/
			this,
			std::bind(&Router_d::getActualPowerStateSAMinusEps,this,std::placeholders::_1),
			std::bind(&Router_d::calculate_performance_number_sa,this,false),
			std::bind(&Router_d::calculate_power_sa,this,std::placeholders::_1,false),
			false,
			false,
			100000,
			&stm[2][0]));

	//logger[2][0].scheduleEvent(1000);

// logger for aggregate crossbar allocator
	str.str("");
	str<<"SW_Router_"<<m_id;
	logger[3].push_back(SleepNetLogManager(str.str(),
			0, /*this is the log id inside the log vector*/
			this,
			std::bind(&Router_d::getActualPowerStateSWMinusEps,this,std::placeholders::_1),
			std::bind(&Router_d::calculate_performance_number_sw,this,false),
			std::bind(&Router_d::calculate_power_sw,this,std::placeholders::_1,false),
			false,
			false,
			100000,
			&stm[3][0]));

	//logger[3][0].scheduleEvent(1000);

}

/* FOLLOWING funcs to collect stats on specific logic blocks on a per epoch basis */
void
Router_d::calculate_performance_number_buf(bool _per_in_port /*or unified*/)
{
	// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);
	for (int j = 0; j < m_virtual_networks; j++) 
	{
        buf_read_count_rt_prev[j]=buf_read_count_rt[j];
		buf_write_count_rt_prev[j]=buf_write_count_rt[j];
		buf_read_count_rt[j]=0;
		buf_write_count_rt[j]=0;
       	for (int i = 0; i < m_input_unit.size(); i++) 
       	{
       	 	buf_read_count_rt[j] += m_input_unit[i]->get_buf_read_count(j);
       	 	buf_write_count_rt[j] += m_input_unit[i]->get_buf_write_count(j);
       	}
	}
}

void
Router_d::calculate_performance_number_va(bool _per_in_port /*or unified*/)
{
		// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);

	for (int j = 0; j < m_virtual_networks; j++) 
	{
       	 	vc_local_arbit_count_rt_prev[j]=vc_local_arbit_count_rt[j];
       	 	vc_local_arbit_count_rt[j]  = m_vc_alloc->get_local_arbit_count(j);
       	 
       		vc_global_arbit_count_rt_prev[j]=vc_global_arbit_count_rt[j];
       	 	vc_global_arbit_count_rt[j] = m_vc_alloc->get_global_arbit_count(j);
	}
}

void
Router_d::calculate_performance_number_sa(bool _per_vnet /*or unified*/)
{
	// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);

	sw_local_arbit_count_rt_prev=sw_local_arbit_count_rt; 
	sw_local_arbit_count_rt = m_sw_alloc->get_local_arbit_count();
	
	sw_global_arbit_count_rt_prev =sw_global_arbit_count_rt ;
	sw_global_arbit_count_rt = m_sw_alloc->get_global_arbit_count();

}

void
Router_d::calculate_performance_number_sw(bool _per_vnet /*or unified*/)
{	// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);

	crossbar_count_rt_prev=crossbar_count_rt;
	crossbar_count_rt = m_switch->get_crossbar_count();

}

void
Router_d::calculate_performance_numbers_run_time(bool _per_in_port_buf,
						bool _per_in_port_va,
						bool _per_vnet_sa,
						bool _per_vnet_crb)
{	// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);

	for (int j = 0; j < m_virtual_networks; j++) 
	{
		buf_read_count_rt_prev[j]=buf_read_count_rt[j];
		buf_write_count_rt_prev[j]=buf_write_count_rt[j];
		buf_read_count_rt[j]=0;
		buf_write_count_rt[j]=0;

       		for (int i = 0; i < m_input_unit.size(); i++) 
       		{
       	 		buf_read_count_rt[j] += m_input_unit[i]->get_buf_read_count(j);
       	 		buf_write_count_rt[j] += m_input_unit[i]->get_buf_write_count(j);
       	 	}
       	 	vc_local_arbit_count_rt_prev[j]=vc_local_arbit_count_rt[j];
       	 	vc_local_arbit_count_rt[j]  = m_vc_alloc->get_local_arbit_count(j);
       	 
       		vc_global_arbit_count_rt_prev[j]=vc_global_arbit_count_rt[j];
       	 	vc_global_arbit_count_rt[j] = m_vc_alloc->get_global_arbit_count(j);
	}
	sw_local_arbit_count_rt_prev=sw_local_arbit_count_rt; 
	sw_local_arbit_count_rt = m_sw_alloc->get_local_arbit_count();
	
	sw_global_arbit_count_rt_prev =sw_global_arbit_count_rt ;
	sw_global_arbit_count_rt = m_sw_alloc->get_global_arbit_count();

	crossbar_count_rt_prev=crossbar_count_rt;
	crossbar_count_rt = m_switch->get_crossbar_count();
}

/* FOLLOWING funcs evaluate power on specific logic blocks on a per epoch basis */
PowerContainerRT 
Router_d::calculate_power_buf(Tick startEpoch, bool _per_in_port /*or unified*/)
{
	// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);

    //Network Activities from garnet
    double sim_cycles;
    sim_cycles = (curTick() - startEpoch)/clockPeriod();
    // Number of virtual networks/message classes declared in Ruby
    // maybe greater than active virtual networks.
    // Estimate active virtual networks for correct power estimates
    int num_active_vclass = 0;
    std::vector<bool > active_vclass_ary;
    active_vclass_ary.resize(m_virtual_networks);

    std::vector<uint32_t > buf_read_count_active;
    std::vector<uint32_t > buf_write_count_active;

    for (int i =0; i < m_virtual_networks; i++) {

        active_vclass_ary[i] = (get_net_ptr())->validVirtualNetwork(i);
        if (active_vclass_ary[i]) 
	{
            num_active_vclass++;
            buf_read_count_active.push_back(buf_read_count_rt[i]-buf_read_count_rt_prev[i]);
            buf_write_count_active.push_back(buf_write_count_rt[i]-buf_write_count_rt_prev[i]);
        }
        else {
            // Inactive vclass
            assert(vc_global_arbit_count[i] == 0);
            assert(vc_local_arbit_count[i] == 0);
        }
    }
	//if(m_id==0)
	//std::cout<<std::accumulate(buf_read_count_active.begin(),buf_read_count_active.end(),0)<<
	//	" "<<std::accumulate(buf_read_count_active.begin(),buf_read_count_active.end(),0)<<std::endl;

    double freq_Hz;
    freq_Hz=this->router_frequency;

    uint32_t num_vclass = num_active_vclass;

    //Power Calculation
    PowerContainerRT res;
	res.PstaticBlock=0.0;
	res.PdynBlock=0.0;
	res.PtotBlock=0.0;
    //Dynamic Power

    // Note: For each active arbiter in vc_arb or sw_arb of size T:1,
    // assuming half the requests (T/2) are high on average.
    // TODO: estimate expected value of requests from simulation.
uint32_t read=0,write=0;
    for (int i = 0; i < num_vclass; i++) 
    {
        // Buffer Write
        res.PdynBlock +=
            orion_rtr_ptr->calc_dynamic_energy_buf(i, WRITE_MODE, false)*
                (buf_write_count_active[i]/sim_cycles)*freq_Hz;
        // Buffer Read
        res.PdynBlock +=
            orion_rtr_ptr->calc_dynamic_energy_buf(i, READ_MODE, false)*
                (buf_read_count_active[i]/sim_cycles)*freq_Hz;
	read+=buf_read_count_active[i];
	write+=buf_write_count_active[i];
	
    }
/*	std::cout<<" eread = "<<orion_rtr_ptr->calc_dynamic_energy_buf(0, READ_MODE, false)
    		<<" max_eread = "<<orion_rtr_ptr->calc_dynamic_energy_buf(0, READ_MODE, true)
    		<<" ewrite = "<<orion_rtr_ptr->calc_dynamic_energy_buf(0, WRITE_MODE, false)
    		<<" max_ewrite = "<<orion_rtr_ptr->calc_dynamic_energy_buf(0, WRITE_MODE, true)
		<<std::endl;

    std::cout<<" reads = "<<read<<" writes = "<<write<<std::endl;
*/
    res.PstaticBlock = orion_rtr_ptr->get_static_power_buf();

	res.PtotBlock=res.PstaticBlock+res.PdynBlock;





    return res;
}

PowerContainerRT
Router_d::calculate_power_va(Tick startEpoch,bool _per_in_port /*or unified*/)
{
	// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);

    //Network Activities from garnet
    double sim_cycles;
    sim_cycles = (curTick() - startEpoch)/clockPeriod();

    // Number of virtual networks/message classes declared in Ruby
    // maybe greater than active virtual networks.
    // Estimate active virtual networks for correct power estimates
    int num_active_vclass = 0;
    std::vector<bool > active_vclass_ary;
    active_vclass_ary.resize(m_virtual_networks);

    std::vector<uint32_t > vc_local_arbit_count_active;
    std::vector<uint32_t > vc_global_arbit_count_active;
    
    for (int i =0; i < m_virtual_networks; i++) 
    {
        active_vclass_ary[i] = (get_net_ptr())->validVirtualNetwork(i);
        if (active_vclass_ary[i]) 
	{
            num_active_vclass++;
            vc_local_arbit_count_active.push_back(vc_local_arbit_count_rt[i]-vc_local_arbit_count_rt_prev[i]);
            vc_global_arbit_count_active.push_back(vc_global_arbit_count_rt[i]-vc_global_arbit_count_rt_prev[i]);
        }
        else {
            // Inactive vclass
            assert(vc_global_arbit_count[i] == 0);
            assert(vc_local_arbit_count[i] == 0);
        }
    }

    double freq_Hz;
    freq_Hz=this->router_frequency;

    uint32_t num_in_port = m_input_unit.size();
    //uint32_t num_out_port = m_output_unit.size();
    uint32_t num_vc_per_vclass = m_vc_per_vnet;
    uint32_t num_vclass = num_active_vclass;

      //Power Calculation
    PowerContainerRT res;
	res.PstaticBlock=0.0;
	res.PdynBlock=0.0;
	res.PtotBlock=0.0;  

    //Dynamic Power

    // Note: For each active arbiter in vc_arb or sw_arb of size T:1,
    // assuming half the requests (T/2) are high on average.
    // TODO: estimate expected value of requests from simulation.

    for (int i = 0; i < num_vclass; i++) 
    {
        // VC arbitration local
        // Each input VC arbitrates for one output VC (in its vclass)
        // at its output port.
        // Arbiter size: num_vc_per_vclass:1
        res.PdynBlock +=
            orion_rtr_ptr->calc_dynamic_energy_local_vc_arb(i,
                num_vc_per_vclass/2, false)*
                    (vc_local_arbit_count_active[i]/sim_cycles)*
                    freq_Hz;

        // VC arbitration global
        // Each output VC chooses one input VC out of all possible requesting
        // VCs (within vclass) at all input ports
        // Arbiter size: num_in_port*num_vc_per_vclass:1
        // Round-robin at each input VC for outvcs in the local stage will
        // try to keep outvc conflicts to the minimum.
        // Assuming conflicts due to request for same outvc from
        // num_in_port/2 requests.
        // TODO: use garnet to estimate this
        res.PdynBlock +=
            orion_rtr_ptr->calc_dynamic_energy_global_vc_arb(i,
                num_in_port/2, false)*
                    (vc_global_arbit_count_active[i]/sim_cycles)*
                    freq_Hz;
    }

    res.PstaticBlock = orion_rtr_ptr->get_static_power_va();

    res.PtotBlock=res.PdynBlock+res.PstaticBlock;

    return res;


}
    
PowerContainerRT
Router_d::calculate_power_sa(Tick startEpoch,bool _per_vnet/*or unified*/)
{
	// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);

    //Network Activities from garnet
    double sim_cycles;
    sim_cycles = (curTick() - startEpoch)/clockPeriod();

    // Number of virtual networks/message classes declared in Ruby
    // maybe greater than active virtual networks.
    // Estimate active virtual networks for correct power estimates
    int num_active_vclass = 0;
    std::vector<bool > active_vclass_ary;
    active_vclass_ary.resize(m_virtual_networks);

    uint32_t  sw_local_arbit_count_active = sw_local_arbit_count_rt - sw_local_arbit_count_rt_prev;
    uint32_t  sw_global_arbit_count_active = sw_global_arbit_count_rt - sw_global_arbit_count_rt_prev;

    for (int i =0; i < m_virtual_networks; i++) {

        active_vclass_ary[i] = (get_net_ptr())->validVirtualNetwork(i);
        if (active_vclass_ary[i]) 
	{
            num_active_vclass++;
        }
        else {
            // Inactive vclass
            assert(vc_global_arbit_count[i] == 0);
            assert(vc_local_arbit_count[i] == 0);
        }
    }

    double freq_Hz;
    freq_Hz=this->router_frequency;

    uint32_t num_in_port = m_input_unit.size();
    //uint32_t num_out_port = m_output_unit.size();
    uint32_t num_vc_per_vclass = m_vc_per_vnet;
    uint32_t num_vclass = num_active_vclass;

    //Power Calculation
    PowerContainerRT res;
	res.PstaticBlock=0.0;
	res.PdynBlock=0.0;
	res.PtotBlock=0.0; 

    //Dynamic Power

    // Note: For each active arbiter in vc_arb or sw_arb of size T:1,
    // assuming half the requests (T/2) are high on average.
    // TODO: estimate expected value of requests from simulation.

    // Switch Allocation Local
    // Each input port chooses one input VC as requestor
    // Arbiter size: num_vclass*num_vc_per_vclass:1
    res.PdynBlock =
        orion_rtr_ptr->calc_dynamic_energy_local_sw_arb(
            num_vclass*num_vc_per_vclass/2, false)*
            (sw_local_arbit_count_active/sim_cycles)*
            freq_Hz;

    // Switch Allocation Global
    // Each output port chooses one input port as winner
    // Arbiter size: num_in_port:1
    res.PdynBlock =
        orion_rtr_ptr->calc_dynamic_energy_global_sw_arb(
            num_in_port/2, false)*
                (sw_global_arbit_count_active/sim_cycles)*
                freq_Hz;

    res.PstaticBlock = orion_rtr_ptr->get_static_power_sa();
    res.PtotBlock=res.PstaticBlock+res.PdynBlock;
    return res;

}

PowerContainerRT
Router_d::calculate_power_sw(Tick startEpoch, bool _per_vnet /*or unified*/)
{
	// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);

    //Network Activities from garnet
    double sim_cycles;
    sim_cycles = (curTick() - startEpoch)/clockPeriod();

    uint32_t  crossbar_count_active = crossbar_count_rt - crossbar_count_rt_prev;

    double freq_Hz;
    freq_Hz=this->router_frequency;

    //Power Calculation
    PowerContainerRT res;
	res.PstaticBlock=0.0;
	res.PdynBlock=0.0;
	res.PtotBlock=0.0; 
    // Crossbar
    res.PdynBlock =
        orion_rtr_ptr->calc_dynamic_energy_xbar(false)*
            (crossbar_count_active/sim_cycles)*freq_Hz;

    // Total
    res.PstaticBlock = orion_rtr_ptr->get_static_power_xbar();
    res.PtotBlock=res.PdynBlock+res.PstaticBlock;
    return res;
}

PowerContainerRT
Router_d::calculate_power_run_time(Tick startEpoch,double VddActual,
					bool _per_in_port_buf,
					bool _per_in_port_va,
					bool _per_vnet_sa,
					bool _per_vnet_crb)
{
	// Orion Initialization
    assert(orion_cfg_ptr!=NULL && orion_rtr_ptr!=NULL);

    //Network Activities from garnet
    double sim_cycles;
    sim_cycles = (curTick() - startEpoch)/clockPeriod();

    // Number of virtual networks/message classes declared in Ruby
    // maybe greater than active virtual networks.
    // Estimate active virtual networks for correct power estimates
    int num_active_vclass = 0;
    std::vector<bool > active_vclass_ary;
    active_vclass_ary.resize(m_virtual_networks);

    std::vector<uint32_t > vc_local_arbit_count_active;
    std::vector<uint32_t > vc_global_arbit_count_active;
    std::vector<uint32_t > buf_read_count_active;
    std::vector<uint32_t > buf_write_count_active;
    uint32_t  sw_local_arbit_count_active = sw_local_arbit_count_rt - sw_local_arbit_count_rt_prev;
    uint32_t  sw_global_arbit_count_active = sw_global_arbit_count_rt - sw_global_arbit_count_rt_prev;
    uint32_t  crossbar_count_active = crossbar_count_rt - crossbar_count_rt_prev;

    for (int i =0; i < m_virtual_networks; i++) {

        active_vclass_ary[i] = (get_net_ptr())->validVirtualNetwork(i);
        if (active_vclass_ary[i]) 
	{
            num_active_vclass++;
            vc_local_arbit_count_active.push_back(vc_local_arbit_count_rt[i]-vc_local_arbit_count_rt_prev[i]);
            vc_global_arbit_count_active.push_back(vc_global_arbit_count_rt[i]-vc_global_arbit_count_rt_prev[i]);
            buf_read_count_active.push_back(buf_read_count_rt[i]-buf_read_count_rt_prev[i]);
            buf_write_count_active.push_back(buf_write_count_rt[i]-buf_write_count_rt_prev[i]);
        }
        else 
		{
            // Inactive vclass
            assert(vc_global_arbit_count[i] == 0);
            assert(vc_local_arbit_count[i] == 0);
        }
    }

    double freq_Hz;
    freq_Hz=this->router_frequency;

    uint32_t num_in_port = m_input_unit.size();
    //uint32_t num_out_port = m_output_unit.size();
    uint32_t num_vc_per_vclass = m_vc_per_vnet;
    uint32_t num_vclass = num_active_vclass;

    //Power Calculation
    PowerContainerRT res;
	res.PstaticBlock=0.0;
	res.PdynBlock=0.0;
	res.PtotBlock=0.0; 
    //Power Calculation
    double Pbuf_wr_dyn = 0.0;
    double Pbuf_rd_dyn = 0.0;
    double Pvc_arb_local_dyn = 0.0;
    double Pvc_arb_global_dyn = 0.0;
    double Psw_arb_local_dyn = 0.0;
    double Psw_arb_global_dyn = 0.0;
    double Pxbar_dyn = 0.0;
    double Ptotal_dyn = 0.0;

    double Pbuf_sta = 0.0;
    double Pvc_arb_sta = 0.0;
    double Psw_arb_sta = 0.0;
    double Pxbar_sta = 0.0;
    double Ptotal_sta = 0.0;



    //Dynamic Power

    // Note: For each active arbiter in vc_arb or sw_arb of size T:1,
    // assuming half the requests (T/2) are high on average.
    // TODO: estimate expected value of requests from simulation.
#if USE_PNET==1
	num_vclass=1;
#endif
    for (int i = 0; i < num_vclass; i++) {
        // Buffer Write
        Pbuf_wr_dyn +=
            orion_rtr_ptr->calc_dynamic_energy_buf(i, WRITE_MODE, false)*
                (buf_write_count_active[i]/sim_cycles)*freq_Hz;

        // Buffer Read
        Pbuf_rd_dyn +=
            orion_rtr_ptr->calc_dynamic_energy_buf(i, READ_MODE, false)*
                (buf_read_count_active[i]/sim_cycles)*freq_Hz;

        // VC arbitration local
        // Each input VC arbitrates for one output VC (in its vclass)
        // at its output port.
        // Arbiter size: num_vc_per_vclass:1
        Pvc_arb_local_dyn +=
            orion_rtr_ptr->calc_dynamic_energy_local_vc_arb(i,
                num_vc_per_vclass/2, false)*
                    (vc_local_arbit_count_active[i]/sim_cycles)*
                    freq_Hz;

        // VC arbitration global
        // Each output VC chooses one input VC out of all possible requesting
        // VCs (within vclass) at all input ports
        // Arbiter size: num_in_port*num_vc_per_vclass:1
        // Round-robin at each input VC for outvcs in the local stage will
        // try to keep outvc conflicts to the minimum.
        // Assuming conflicts due to request for same outvc from
        // num_in_port/2 requests.
        // TODO: use garnet to estimate this
        Pvc_arb_global_dyn +=
            orion_rtr_ptr->calc_dynamic_energy_global_vc_arb(i,
                num_in_port/2, false)*
                    (vc_global_arbit_count_active[i]/sim_cycles)*
                    freq_Hz;
    }

    // Switch Allocation Local
    // Each input port chooses one input VC as requestor
    // Arbiter size: num_vclass*num_vc_per_vclass:1
    Psw_arb_local_dyn =
        orion_rtr_ptr->calc_dynamic_energy_local_sw_arb(
            num_vclass*num_vc_per_vclass/2, false)*
            (sw_local_arbit_count_active/sim_cycles)*
            freq_Hz;

    // Switch Allocation Global
    // Each output port chooses one input port as winner
    // Arbiter size: num_in_port:1
    Psw_arb_global_dyn =
        orion_rtr_ptr->calc_dynamic_energy_global_sw_arb(
            num_in_port/2, false)*
                (sw_global_arbit_count_active/sim_cycles)*
                freq_Hz;

    // Crossbar
    Pxbar_dyn =
        orion_rtr_ptr->calc_dynamic_energy_xbar(false)*
            (crossbar_count_active/sim_cycles)*freq_Hz;

    // Total
    Ptotal_dyn = Pbuf_wr_dyn + Pbuf_rd_dyn +
                 Pvc_arb_local_dyn + Pvc_arb_global_dyn +
                 Psw_arb_local_dyn + Psw_arb_global_dyn +
                 Pxbar_dyn;

    m_power_dyn = Ptotal_dyn;
    
    // Clock Power
    m_clk_power = orion_rtr_ptr->calc_dynamic_energy_clock()*freq_Hz;

    // Static Power
    Pbuf_sta = orion_rtr_ptr->get_static_power_buf();
    Pvc_arb_sta = orion_rtr_ptr->get_static_power_va();
    Psw_arb_sta = orion_rtr_ptr->get_static_power_sa();
    Pxbar_sta = orion_rtr_ptr->get_static_power_xbar();

    Ptotal_sta += Pbuf_sta + Pvc_arb_sta + Psw_arb_sta + Pxbar_sta;

    m_power_sta = Ptotal_sta;

//	///////////////////////////////////////////////////////////////////////////////////////
//	///////////////////////////////////////////////////////////////////////////////////////
//	// compute pwr for buf
//
//	static double dynAvg=0; static double statAvg=0;
//	static long temp_counter=0;
//	if(m_id==5)
//	{
//		dynAvg+=(Pbuf_wr_dyn+Pbuf_rd_dyn);
//		statAvg=Pbuf_sta;
//		temp_counter++;
//		std::cout<<"@"<<curTick()	<<" bufDyn:"<<(Pbuf_wr_dyn+Pbuf_rd_dyn)
//									<<" bufStat:"<<Pbuf_sta
//									<<" dynAvg:"<<dynAvg
//									<<" statAvg:"<<statAvg
//									<<" counter:"<<temp_counter
//									<< std::endl;
//	}
//
//	///////////////////////////////////////////////////////////////////////////////////////
//	///////////////////////////////////////////////////////////////////////////////////////

	double m_tech_ptr_vdd=orion_cfg_ptr->get_tech_param_ptr()->get_vdd();

	double temp_vdd_square_scale= (VddActual*VddActual)/(m_tech_ptr_vdd*m_tech_ptr_vdd );
	double temp_vdd_scale=VddActual/m_tech_ptr_vdd;

	//std::cout<<"temp_vdd_square_scale: "<< temp_vdd_square_scale<<" temp_vdd_scale: "<<temp_vdd_scale<<std::endl;

    res.PstaticBlock=Ptotal_sta*temp_vdd_scale;
    res.PdynBlock=m_power_dyn*temp_vdd_square_scale;
	res.PtotBlock=res.PstaticBlock+res.PdynBlock; 
	res.PdynClock=m_clk_power*temp_vdd_square_scale;

    return res;

}


