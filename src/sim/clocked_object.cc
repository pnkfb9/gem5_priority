/*
 * Copyright (c) 2013 Politecnico di Milano
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
 * Authors: Federico Terraneo, Davide Zoni
 */
#include "config/use_boost_cpp2011.hh"
#include <limits>
#include <list>
#include "sim/clocked_object.hh"
#include "base/statistics.hh"
#include "base/callback.hh"

#if USE_BOOST_CPP2011 == 1
	#include <boost/date_time.hpp>
#endif

using namespace std;

#if USE_BOOST_CPP2011 == 1
	using namespace boost::posix_time;
#endif

//#define PRINT_EVENT_RESCHEDULING //FIXME: transform in a debug flag

namespace {

class PeriodChangeEvent : public Event
{
public:
    PeriodChangeEvent(ClockedObject *o, Tick period)
        : Event(Default_Pri, AutoDelete), o(o), period(period) {}
        
    virtual void process()
    {
        //Really has to be changePeriod() and not doChangePeriod()
        o->changePeriod(period);
    }
    
private:
    ClockedObject *o;
    Tick period;
};

// This code resets all clockedObject frequency averages every time
// m5.stats.reset() is called in python

class ClockedObjectStatsResetCallback : Callback
{
public:
    
    virtual void process();
    
    static void addClockedObject(ClockedObject *co);
    
private:
    
    void addClockedObjectImpl(ClockedObject *co);
    
    vector<ClockedObject*> obj;
    static ClockedObjectStatsResetCallback *cb;
};

void ClockedObjectStatsResetCallback::process()
{
    //for(auto co : obj) co->resetAverageFreqStats();
    for(auto co=obj.begin();co!=obj.end();co++) (*co)->resetAverageFreqStats();
}

void ClockedObjectStatsResetCallback::addClockedObject(ClockedObject* co)
{
    if(!cb)
    {
        cb=new ClockedObjectStatsResetCallback;
        Stats::registerResetCallback(cb);
    }
    cb->addClockedObjectImpl(co);
}

void ClockedObjectStatsResetCallback::addClockedObjectImpl(ClockedObject* co)
{
    obj.push_back(co);
}

ClockedObjectStatsResetCallback *ClockedObjectStatsResetCallback::cb=0;

}

ClockedObject::ClockedObject(const ClockedObjectParams* p) :
        SimObject(p), tick(0), cycle(0), lastPeriodChange(0),
        lastPeriodChangeTick(0), statsBeginTime(0), weightedFreqSum(0.0),
        clock(p->clock)
    {
        if (clock == 0) {
            fatal("%s has a clock period of zero\n", name());
        }
        ClockedObjectStatsResetCallback::addClockedObject(this);
    }

void
ClockedObject::doChangePeriod(Tick period)
{  
    //To avoid clock glitches changing the router clock period has to be
    //aligned on a clock edge. Otherwise the computations below will fail
    assert(curTick() == clockEdge());
    
    Tick oldPeriod = clockPeriod();
    if(period == oldPeriod) return;
    
    #ifdef PRINT_EVENT_RESCHEDULING
    ptime t1(microsec_clock::local_time());
    #endif //PRINT_EVENT_RESCHEDULING

    list<Event*> routerEvents;
    eventq->removeIfMatch(routerEvents,getObjectId());
    
    #ifdef PRINT_EVENT_RESCHEDULING
    cout << "@" << curTick() << " " << getObjectId() << " has " << routerEvents.size() << " events." << endl;
    #endif //PRINT_EVENT_RESCHEDULING

    // Try to reschedule events so that two events scheduled in different time
    // points be not transiently scheduled in the same time point while the list
    // is traversed. Not doing this may cause assertion failures when dealing
    // with ConsumerEvent
    if(period < oldPeriod)
    {
        Tick prev = 0;
        list<Event*>::iterator it;
        for(it=routerEvents.begin();it!=routerEvents.end();++it)
        {
            Event *e = *it;
            Tick cyclesToGo = (e->when() - curTick()) / oldPeriod;
            //Assert that the previous computation is exact, not approximated
            assert((curTick() + cyclesToGo * oldPeriod) == e->when());

            e->rescheduleTo(curTick() + cyclesToGo * period);

            //Check the list remains orederd, as merge() needs an ordered list
            assert(e->when() >= prev);
            prev = e->when();
        }
    } else {
        Tick prev = numeric_limits<Tick>::max();
        list<Event*>::reverse_iterator it;
        for(it=routerEvents.rbegin();it!=routerEvents.rend();++it)
        {
            Event *e = *it;
            Tick cyclesToGo = (e->when() - curTick()) / oldPeriod;
            //Assert that the previous computation is exact, not approximated
            assert((curTick() + cyclesToGo * oldPeriod) == e->when());

            e->rescheduleTo(curTick() + cyclesToGo * period);

            //Check the list remains orederd, as merge() needs an ordered list
            assert(e->when() <= prev);
            prev = e->when();
        }
    }
    
    eventq->merge(routerEvents);
    update();
    this->clock = period;
    
    #ifdef PRINT_EVENT_RESCHEDULING
    ptime t2(microsec_clock::local_time());
    cout << "Time = " << (t2-t1) << endl; 
    #endif //PRINT_EVENT_RESCHEDULING
}

void
ClockedObject::changePeriod(Tick period)
{
    // Classes that want to support period change must overload getObjectId()
    // to return an unique id, and all the events they spawn must return the
    // same value when getObjectId() is called on them.
    assert(getObjectId()!="");
    
    Tick cc = curCycle();

    // Having two period change in the same cycle is not supported as the
    // one that will take effect will depend on the ordering of events in
    // the simulation queue, which may lead to non determinism
	// NOTE: Two changes in the same clock cycle are allowed only at tick 0 during
	// configuration
	//std::cout<<"@"<<curTick()<<" "<<this<<" "<<getObjectId()<<std::endl;
	if(curTick()>0)
	    assert(lastPeriodChange != cc);
	
    
    Tick cur = curTick();
    if(cur == clockEdge())
    {
        // Update period change stats
        weightedFreqSum += (SimClock::Float::s / clockPeriod()) * (cur - max(lastPeriodChangeTick,statsBeginTime));
        lastPeriodChangeTick=cur;
        
        lastPeriodChange = cc;
        doChangePeriod(period);
    } else eventq->schedule(new PeriodChangeEvent(this,period),clockEdge());
}

double
ClockedObject::averageFrequency()
{
    Tick cur = curTick();
    double temp = weightedFreqSum;
    temp += (SimClock::Float::s / clockPeriod()) * (cur - max(lastPeriodChangeTick,statsBeginTime));
    temp /= cur-statsBeginTime;
    return temp;
}


void
ClockedObject::resetAverageFreqStats()
{
    weightedFreqSum = 0.0;
    statsBeginTime = curTick();
}


MasterObject::~MasterObject() {}
