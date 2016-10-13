#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <iostream>
#include <string>
#include <sstream>

#include <numeric>
#include <algorithm>
#include <functional>

//#include "mem/ruby/network/orion/NetworkPower.hh"
//#include "mem/ruby/network/orion/OrionConfig.hh"
//#include "mem/ruby/network/orion/OrionLink.hh"
//#include "mem/ruby/network/orion/OrionRouter.hh"

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
#include "config/use_voq.hh"

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


void
Router_d::setPowerStateInBuf(bool is_on_,Tick when,int in_port)
{
	//assert allows for right semantic, since state is set now but evaluated
	//on the right tick at the beginning 
	assert(when>0);
	assert(m_input_unit.size() > in_port && in_port >=0);

	m_input_unit[in_port]->setNextPowerState(is_on_);	//set new state
	m_input_unit[in_port]->setTickChangePowerState(when);	//set evaluation tick
	m_input_unit[in_port]->scheduleEvent(curTick()+when);	//sched event on correct tick (curTick()+when)
};
void 
Router_d::setPowerStateOutBuf(bool is_on_,Tick when,int port_num)
{
	panic("No power gating support for output buffer objects\n");
};

void 
Router_d::setPowerStateVA(bool is_on_,Tick when)
{
	//assert allows for right semantic, since state is set now but evaluated
	//on the right tick at the beginning 
	assert(when>0);
	m_vc_alloc->setNextPowerState(is_on_);	//set new state
	m_vc_alloc->setTickChangePowerState(curTick()+when);	//set evaluation tick
	m_vc_alloc->scheduleEvent(when);	//sched event on correct tick (curTick()+when)
};
void 
Router_d::setPowerStateSA(bool is_on_,Tick when)
{
	//assert allows for right semantic, since state is set now but evaluated
	//on the right tick at the beginning 
	assert(when>0);
	m_sw_alloc->setNextPowerState(is_on_);	//set new state
	m_sw_alloc->setTickChangePowerState(curTick()+when);	//set evaluation tick
	m_sw_alloc->scheduleEvent(when);	//sched event on correct tick (curTick()+when)
};
void 
Router_d::setPowerStateSW(bool is_on_,Tick when)
{    	
	//assert allows for right semantic, since state is set now but evaluated
	//on the right tick at the beginning. AT LEAST 1 CLOCK TICK DELAY 
	assert(when>0);
	m_switch->setNextPowerState(is_on_);
	m_switch->setTickChangePowerState(curTick()+when);	//set evaluation tick
	m_switch->scheduleEvent(when);	//sched event on correct tick (curTick()+when)    
};


bool
Router_d::getPowerStateInBuf(Tick when,int in_port)
{
	assert(when>=0);
	assert(m_input_unit.size() > in_port && in_port >=0);

	bool is_on=true;
	
	if(m_input_unit[in_port]->getTickChangePowerState()==0)
	{// eventually already updated this cycle return the actual value
		is_on=m_input_unit[in_port]->getPowerState();
	}
	else
	{
		if(when>=m_switch->getTickChangePowerState())
		{//not yet updated or will be updated later but before inquery Tick 
			is_on=!m_input_unit[in_port]->getPowerState();
		}
		else
		{//the update is subsequent with respect to inquery Tick
			is_on=m_input_unit[in_port]->getPowerState();
		}
	}
	return is_on;

	
}

bool
Router_d::getPowerStateOutBuf(Tick when,int out_port)
{
	panic("No power gating support for output buffer objects\n");
}

bool
Router_d::getPowerStateVA(Tick when)
{
	assert(when>=0);
	bool is_on=true;
	
	if(m_vc_alloc->getTickChangePowerState()==0)
	{// eventually already updated this cycle return the actual value
		is_on=m_vc_alloc->getPowerState();
	}
	else
	{
		if(when>=m_switch->getTickChangePowerState())
		{//not yet updated or will be updated later but before inquery Tick 
			is_on=!m_vc_alloc->getPowerState();
		}
		else
		{//the update is subsequent with respect to inquery Tick
			is_on=m_vc_alloc->getPowerState();
		}
	}
	return is_on;
}

bool
Router_d::getPowerStateSA(Tick when)
{
	assert(when>=0);
	bool is_on=true;
	
	if(m_sw_alloc->getTickChangePowerState()==0)
	{// eventually already updated this cycle return the actual value
		is_on=m_sw_alloc->getPowerState();
	}
	else
	{
		if(when>=m_switch->getTickChangePowerState())
		{//not yet updated or will be updated later but before inquery Tick 
			is_on=!m_sw_alloc->getPowerState();
		}
		else
		{//the update is subsequent with respect to inquery Tick
			is_on=m_sw_alloc->getPowerState();
		}
	}
	return is_on;
}

bool
Router_d::getPowerStateSW(Tick when)
{
	assert(when>=0);
	bool is_on=true;
	
	if(m_switch->getTickChangePowerState()==0)
	{// eventually already updated this cycle return the actual value
		is_on=m_switch->getPowerState();
	}
	else
	{
		if(when>=m_switch->getTickChangePowerState())
		{//not yet updated or will be updated later but before inquery Tick 
			is_on=!m_switch->getPowerState();
		}
		else
		{//the update is subsequent with respect to inquery Tick
			is_on=m_switch->getPowerState();
		}
	}
	return is_on;
}

bool
Router_d::getNextPowerStateInBuf(Tick when,int in_port)
{
	assert(when>=0);
	return m_input_unit[in_port]->getNextPowerState();
}

bool
Router_d::getNextPowerStateOutBuf(Tick when,int out_port)
{	
	panic("No power gating support for output buffer objects\n");
}

bool
Router_d::getNextPowerStateVA(Tick when)
{
	assert(when>=0);
	return m_vc_alloc->getNextPowerState();
}

bool
Router_d::getNextPowerStateSA(Tick when)
{
	assert(when>=0);
	return m_sw_alloc->getNextPowerState();
}

bool
Router_d::getNextPowerStateSW(Tick when)
{
	assert(when>=0);
	return m_switch->getNextPowerState();
}


bool
Router_d::getActualPowerStateInBuf(int in_port)
{
	assert(m_input_unit.size() > in_port && in_port >=0);
	return m_input_unit[in_port]->getActualPowerState();
}

bool
Router_d::getActualPowerStateOutBuf(int out_port)
{
	panic("No power gating support for output buffer objects\n");
}

bool
Router_d::getActualPowerStateVA(int useless)
{
	return m_vc_alloc->getActualPowerState();
}

bool
Router_d::getActualPowerStateSA(int useless)
{
	return m_sw_alloc->getActualPowerState();
}

bool
Router_d::getActualPowerStateSW(int useless)
{
	return m_switch->getActualPowerState();
}

bool
Router_d::getActualPowerStateInBufMinusEps(int in_port)
{
	assert(m_input_unit.size() > in_port && in_port >=0);
	return m_input_unit[in_port]->getActualPowerStateMinusEps();
}

bool
Router_d::getActualPowerStateOutBufMinusEps(int out_port)
{
	panic("No power gating support for output buffer objects\n");
}

bool
Router_d::getActualPowerStateVAMinusEps(int useless)
{
	return m_vc_alloc->getActualPowerStateMinusEps();
}

bool
Router_d::getActualPowerStateSAMinusEps(int useless)
{
	return m_sw_alloc->getActualPowerStateMinusEps();
}

bool
Router_d::getActualPowerStateSWMinusEps(int useless)
{
	return m_switch->getActualPowerStateMinusEps();
}




Tick 
Router_d::getTickChangePowerStateInBuf(int in_port)
{
	assert(m_input_unit.size()>in_port && in_port>=0);
	return m_input_unit[in_port]->getTickChangePowerState();
}
Tick 
Router_d::getTickChangePowerStateOutBuf(int out_port)
{
	panic("No power gating support for output buffer objects\n");
}
Tick
Router_d::getTickChangePowerStateVA()
{
	return m_vc_alloc->getTickChangePowerState();
}
Tick
Router_d::getTickChangePowerStateSA()
{
	return m_sw_alloc->getTickChangePowerState();
}
Tick
Router_d::getTickChangePowerStateSW()
{
	return m_switch->getTickChangePowerState();
}
