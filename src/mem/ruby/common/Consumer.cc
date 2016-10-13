/*
 * Copyright (c) 2012 Mark D. Hill and David A. Wood
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
 */

#include <sstream>
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"   

Consumer::Consumer(ClockedObject *_em)
        : changeState(0),powerState(ON),nextPowerState(ON),
		m_last_scheduled_wakeup(0), m_last_wakeup(0),       
        em(_em), prio(Event::Default_Pri) {}

void
Consumer::scheduleEvent(Time timeDelta)
{
    scheduleEventAbsolute(timeDelta + em->curCycle());
}

void
Consumer::scheduleEventAbsolute(Time timeAbs)
{
    Tick t = em->clockEdge() + em->clockPeriod() * (timeAbs - em->curCycle());
    if (!alreadyScheduled(t)) {
        // This wakeup is not redundant
        ConsumerEvent *evt = new ConsumerEvent(this,prio);
	#if USE_LRC==1
		assert(timeAbs >= em->curCycle() || em->clockEdge() >= curTick());
	#else
    	assert(timeAbs >= em->curCycle() || em->clockEdge() >= curTick());
	#endif
        em->schedule(evt, t);
        insertScheduledWakeupTime(t);
    }
}


bool 
Consumer::getActualPowerState()
{
	bool is_off=false;
	if(!getPowerState())
	{
		is_off=true;
	}
	if( (getTickChangePowerState()==curTick()) 
		&& getPowerState() 
		&& !getNextPowerState() )
	{	// consumer is switching off but not yet 
		// happened due to eventqueue order
		is_off=true;
	}
	if( (getTickChangePowerState()==curTick())
		&& !getPowerState() 
		&& getNextPowerState() )
	{	// switch is switching on but not yet happened due to eventqueue order
		//use this to avoid losing one cycle
		is_off=false;
	}
	return !is_off;
}

bool 
Consumer::getActualPowerStateMinusEps()
{

	bool is_off=false;
	if(!getPowerState())
	{
		is_off=true;
	}
	if( (getTickChangePowerState()==curTick()) 
		&& getPowerState() != getNextPowerState() )
	{	// consumer is switching but not updated this clock
		is_off=getPowerState();
	}
	if( (getTickChangePowerState()==curTick())
		&& getPowerState() == getNextPowerState() )
	{	// switch is switching already updated this cycle
		is_off=!getPowerState();
	}
	//std::cout<<"@"<<curTick()
	//	<<" getPowerState() "<<getPowerState()
	//	<<" getNextPowerState() "<<  getNextPowerState()
	//	<<" getTickChangePowerState() "<< getTickChangePowerState()
	//	<<" return state is_off "<< is_off<<std::endl;
	return !is_off;
}


