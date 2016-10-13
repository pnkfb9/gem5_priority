#include "simple_power_gating_policy.hh"
#include <iostream>

SimplePowerGatingPolicy::SimplePowerGatingPolicy(std::string policy_name, Router_d *router)
:Consumer(router)
{
	this->m_name=policy_name;
	this->m_router_ptr=router;

	init_logger();
}

void
SimplePowerGatingPolicy::init_logger()
{
	m_router_ptr->setLoggerAutoSched(3,0,false);
	m_router_ptr->setLoggerPrintStatus(3,0,false);
}

void
SimplePowerGatingPolicy::policy_logic()
{
	bool sw_actual_state=m_router_ptr->getActualPowerStateSW(0);
	bool sw_next_state=!m_router_ptr->getPowerStateSW(0);

	
	uint32_t congestion=m_router_ptr->getCongestion(); //actual router congestion

	
/* set power gating actuation delay */	
	uint32_t cmd_delay=1;
	if(sw_actual_state==OFF)
		cmd_delay=5; // FIXME this is in ticks, set as 5t=5RC of the evaluated network 
	
/* set power gating next state and actuator delay */
	m_router_ptr->setPowerStateSW(sw_next_state, cmd_delay);



/*
	if(cmd_delay>1)
		m_router_ptr->setLoggerSchedWakeUp(3,0,1);
	m_router_ptr->setLoggerSchedWakeUp(3,0,cmd_delay);
*/	
	/*if(m_router_ptr->curCycle()==1000000+m_router_ptr->getInitialPolicyDelay())
	{
		//std::cout<<std::endl<<"@"<<curTick()<<" "<<m_router_ptr->curCycle()<<"########## print log csv ########"<<std::endl<<std::endl;
		//m_router_ptr->printLogger(std::cout,3,0);
		m_router_ptr->printLoggerCSV(std::string("/home/davide/Documents/prova_log.txt"),3,0);
	}*/

	
	// old way to report data for power gating on switch
	//std::cout<<"done"<<std::endl;
	//m_router_ptr->printLoggerCSV(std::string("./m5out/power_gating.txt"),3,0);
	
	f.open("./m5out/power_gating.txt",std::fstream::app|std::fstream::out);
	assert(f.is_open());
	f<<curTick()<<" "
		<<congestion<<" "
		<<m_router_ptr->getActualPowerStateSW(0)<<" "
		<<m_router_ptr->get_dynamic_power()<<" "/* total router power */
		<<m_router_ptr->get_static_power()<<std::endl;
	f.close();

	scheduleEvent(10); // 10000 @ 1e12 means	every 10ns 100MHz

//	std::cout<<"#############################################"<<std::endl;
//	std::cout<<"@"<<m_router_ptr->curCycle() <<" "<<curTick()<<std::endl
//	<<"curCycle()"<<m_router_ptr->curCycle()<<std::endl
//	<<"nextTickChange "<<m_router_ptr->getTickChangePowerStateSW()<<std::endl
//	<<"power_state "<<m_router_ptr->getPowerStateSW()<<std::endl
//	<<"power_next_state "<<m_router_ptr->getNextPowerStateSW()<<std::endl
//	<<"log at "<<m_router_ptr->curCycle()+1<<std::endl;
}

void
SimplePowerGatingPolicy::wakeup()
{
	//schedule policy tasks here
	policy_logic();
}
