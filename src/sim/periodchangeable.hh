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

#include <vector>
#include <list>
#include <fstream>
#include <set>
#include "sim/eventq.hh"
#include "base/types.hh"

#ifndef PERIODCHANGEABLE_HH
#define	PERIODCHANGEABLE_HH

class VoltageRegulatorBase;

class PeriodChangeable
{
public:
    virtual void changePeriod(Tick period)=0;
    
    virtual Tick clockPeriod() const=0;
    
    virtual Tick clockEdge(Cycles cycles) const=0;
    
    virtual ~PeriodChangeable();
};

class FrequencyIsland : public PeriodChangeable
{
public:
    virtual void changePeriod(Tick period);
    
    Tick clockPeriod() const;
    
    Tick clockEdge(Cycles cycles) const;
    
    void addObject(PeriodChangeable *object);
    
    void removeObject(PeriodChangeable *object);
    
    bool empty() const { return objects.empty(); }
    
    void clear();

	std::vector<PeriodChangeable*>& getObjList(){return objects;}
    
private:
    std::vector<PeriodChangeable*> objects;
};

///**
// * DEPRECATED, this model performs simulation using periods and not frequencies.
// * This PLL is fast and efficient, but does not work well if
// * the period is changed so fast that the step response does not reach
// * steady state.
// */
//class PllModel : public PeriodChangeable, public EventManager
//{
//public:
//    PllModel(PeriodChangeable *object, EventQueue *eq);
//    
//    void changePeriod(Tick period);
//    
//    Tick clockPeriod() const;
//    
//    Tick clockEdge(Cycles cycles) const;
//    
//    PeriodChangeable *getParent();
//    
//    ~PllModel();
//    
//private:
//    PllModel(const PllModel&);
//    PllModel& operator= (const PllModel&);
//    
//    class PllEvent : public Event
//    {
//    public:
//        
//        PllEvent(PllModel *model);
//                
//        void start(Tick period);
//        
//        void process();
//        
//        ~PllEvent();
//            
//    private:
//        PllModel *model;
//        bool inProgress;
//        double pStart, pEnd;
//        Tick tStart, t, tEnd;
//    };
//    
//    PeriodChangeable *object;
//    PllEvent *plle;
//};

/**
 * This PLL uses implicit euler with a variable integration step, so it
 * works well even when the period is changed faster that the step response
 * does to reach steady state. Simulations show it can work with an integration
 * step up to 8 times the clock cycle.
 */
class AdvancedPllModel : public PeriodChangeable, public EventManager
{
public:
    AdvancedPllModel(PeriodChangeable *object, EventQueue *eq);
    
    void changePeriod(Tick period);
    
    Tick clockPeriod() const;
    
    Tick clockEdge(Cycles cycles) const;
    
    PeriodChangeable *getParent();
    
    ~AdvancedPllModel();

	std::ofstream& getPllFile() { return pll_f; }
    
    VoltageRegulatorBase *getVoltageRegulator() { return vreg; }

private:
    AdvancedPllModel(const AdvancedPllModel&);
    AdvancedPllModel& operator= (const AdvancedPllModel&);
    
    class PllEvent : public Event
    {
    public:
        
        PllEvent(AdvancedPllModel *model);
                
        void changePeriod(Tick period);
        
        void process();
        
        ~PllEvent();
         
    private:
        AdvancedPllModel *model;
        double u,y,x1,x2;
    };
    
    PeriodChangeable *object;
    VoltageRegulatorBase *vreg;
    PllEvent *plle;
    bool first;
	std::ofstream pll_f;
    static const double a12;
    static const double a21;
    static const double a22;
    static const double b2;
    static const double c1;
    static const int multiplier2;
    static int id;
};

class VoltageRegulatorBase
{
public:
    VoltageRegulatorBase();
    
    /**
     * Advance the voltage regulator model by one step
     * \param frequency the current frequency set point coming from the policy
     * \return the frequency set point for the PLL, delayed as needed in order
     * to prevent the voltage and frequency to be unsafe for the device operation
     */
    virtual double advance(double frequency)=0;
    
    virtual double voltage()=0;
    
    virtual double averageVoltage()=0;
    virtual void resetAverageVoltageStats()=0;
    
    virtual ~VoltageRegulatorBase();
};

class DummyVoltageRegulator : public VoltageRegulatorBase
{
public:
    virtual double advance(double frequency);
    
    virtual double voltage();
    
    virtual double averageVoltage();
    virtual void resetAverageVoltageStats();
};

class VoltageRegulatorModel : public VoltageRegulatorBase
{
public:
    VoltageRegulatorModel();
    
    virtual double advance(double frequency);
    
    virtual double voltage();
    
    virtual double averageVoltage();
    virtual void resetAverageVoltageStats();
    
    ~VoltageRegulatorModel();

private:
    
    double f2v(double f);
    
    VoltageRegulatorModel(const VoltageRegulatorModel&);
    VoltageRegulatorModel& operator= (const VoltageRegulatorModel&);
    
    bool first;
	std::ofstream vregf;
    static const double a12;
    static const double a21;
    static const double a22;
    static const double b2;
    static const double c1;
    static const double settlingV;
    static const double settlingF;
    static int id;
    double u,y,x1,x2;
    double fOld,vOld;
    Tick fdelay,vdelay;
    Tick prevTick;
    Tick statsBeginTime;
    double weightedVoltSum;
};

#endif //PERIODCHANGEABLE_HH
