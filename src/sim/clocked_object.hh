/*
 * Copyright (c) 2012 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
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
 * Authors: Andreas Hansson
 */

/**
 * @file
 * ClockedObject declaration and implementation.
 */

#ifndef __SIM_CLOCKED_OBJECT_HH__
#define __SIM_CLOCKED_OBJECT_HH__

#include <utility>
#include "base/intmath.hh"
#include "base/misc.hh"
#include "params/ClockedObject.hh"
#include "sim/core.hh"
#include "sim/sim_object.hh"
#include "sim/periodchangeable.hh"

/**
 * The ClockedObject class extends the SimObject with a clock and
 * accessor functions to relate ticks to the cycles of the object.
 */
class ClockedObject : public SimObject, public PeriodChangeable
{

  private:

    // the tick value of the next clock edge (>= curTick()) at the
    // time of the last call to update()
    mutable Tick tick;

    // The cycle counter value corresponding to the current value of
    // 'tick'
    mutable Cycles cycle;
    
    // Clock cycle when the last period change happened
    Tick lastPeriodChange;
    Tick lastPeriodChangeTick;
    Tick statsBeginTime;
    double weightedFreqSum;
    
    /**
     * Prevent inadvertent use of the copy constructor and assignment
     * operator by making them private.
     */
    ClockedObject(ClockedObject&);
    ClockedObject& operator=(ClockedObject&);

    /**
     *  Align cycle and tick to the next clock edge if not already done.
     */
    void update() const
    {
        // both tick and cycle are up-to-date and we are done, note
        // that the >= is important as it captures cases where tick
        // has already passed curTick()
        if (tick >= curTick())
            return;

        // optimise for the common case and see if the tick should be
        // advanced by a single clock period
        tick += clock;
        ++cycle;

        // see if we are done at this point
        if (tick >= curTick())
            return;

        // if not, we have to recalculate the cycle and tick, we
        // perform the calculations in terms of relative cycles to
        // allow changes to the clock period in the future
        Cycles elapsedCycles(divCeil(curTick() - tick, clock));
        cycle += elapsedCycles;
        tick += elapsedCycles * clock;
    }
 
    void doChangePeriod(Tick period);

  protected:

    // Clock period in ticks
    Tick clock;

    /**
     * Create a clocked object and set the clock based on the
     * parameters.
     */
    ClockedObject(const ClockedObjectParams* p);

    /**
     * Virtual destructor due to inheritance.
     */
    virtual ~ClockedObject() { }

    /**
     * Reset the object's clock using the current global tick value. Likely
     * to be used only when the global clock is reset. Currently, this done
     * only when Ruby is done warming up the memory system.
     */
    void resetClock() const
    {
        Cycles elapsedCycles(divCeil(curTick(), clock));
        cycle = elapsedCycles;
        tick = elapsedCycles * clock;
    }
    
  public:
      
    virtual void changePeriod(Tick period);
    
    /**
     * Derived classes that want to support frequency change need to overload
     * this function to return an unique id, and the getObjectId() of the
     * events that they spawn shall return the same value. This allows
     * moving already scheduled events in time when frequency changes are
     * requested.
     * @return an unique id for the clocked object 
     */
    virtual std::string getObjectId() const { return ""; }

    /**
     * Determine the tick when a cycle begins, by default the current
     * one, but the argument also enables the caller to determine a
     * future cycle.
     *
     * @param cycles The number of cycles into the future
     *
     * @return The tick when the clock edge occurs
     */
    virtual Tick clockEdge(Cycles cycles = Cycles(0)) const
    {
        // align tick to the next clock edge
        update();

        // figure out when this future cycle is
        return tick + clock * cycles;
    }

    /**
     * Determine the current cycle, corresponding to a tick aligned to
     * a clock edge.
     * 
     * I.e., it returns a counter of clock edges.
     *
     * @return The current cycle count
     */
    inline Cycles curCycle() const
    {
        // align cycle to the next clock edge.
        update();

        return cycle;
    }

    double averageFrequency();
    void resetAverageFreqStats();
    
    /**
     * Based on the clock of the object, determine the tick when the
     * next cycle begins, in other words, return the next clock edge.
     *
     * @return The tick when the next cycle starts
     */
    Tick nextCycle() const
    { return clockEdge(); }

    inline uint64_t frequency() const { return SimClock::Frequency / clock; }

    virtual Tick clockPeriod() const { return clock; }

    inline Cycles ticksToCycles(Tick tick) const
    { return Cycles(tick / clock); }
    
};

/**
 * Only classes that derive from ClockedObject can derive from this
 */
class MasterObject
{
public:
    void addSlaveObject(ClockedObject *slaveObject)
    {
        assert(slaveObject);
        ClockedObject *thisObj=dynamic_cast<ClockedObject*>(this);
        assert(thisObj);
        if(slaveObjects.insert(slaveObject).second)
            std::cout<<slaveObject->getObjectId()
                     <<" is now a slave of "<<thisObj->getObjectId()
                     <<std::endl;
    }
    
    void changeSlaveObjectPeriod(Tick period)
    {
        for(auto it=slaveObjects.begin();it!=slaveObjects.end();++it)
            (*it)->changePeriod(period);
    }
    
    virtual ~MasterObject()=0;
private:
    //Holds devices that always change frequency whenever the core does,
    //that currently are the L1Cache_controller and the Sequencer
    //Using a set as it automatically removes duplicates
    std::set<ClockedObject*> slaveObjects;  
};

#endif //__SIM_CLOCKED_OBJECT_HH__
