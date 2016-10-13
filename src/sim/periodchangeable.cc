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

#include <iostream>
#include <cmath>
#include <cassert>
#include <set>
#include "sim/core.hh"
#include "sim/eventq_impl.hh"
#include "periodchangeable.hh"
#include "base/callback.hh"
#include "base/statistics.hh"

using namespace std;

//
// class PeriodChangeable
//

PeriodChangeable::~PeriodChangeable() {}

//
// class FrequencyIsland
//

void FrequencyIsland::changePeriod(Tick period)
{
    for(auto it=objects.begin();it!=objects.end();++it)
        (*it)->changePeriod(period);
}

Tick FrequencyIsland::clockPeriod() const
{
    assert(objects.empty()==false);
    return objects.front()->clockPeriod();
}

Tick FrequencyIsland::clockEdge(Cycles cycles) const
{
    assert(objects.empty()==false);
    return objects.front()->clockEdge(cycles);
}

void FrequencyIsland::addObject(PeriodChangeable* object)
{
    assert(object!=this);
    objects.push_back(object);
}

void FrequencyIsland::removeObject(PeriodChangeable* object)
{
	auto it = std::find(objects.begin(), objects.end(), object);
	if(it != objects.end())
    objects.erase(it);
	//assert(false);
    //objects.remove(object);
}

void FrequencyIsland::clear()
{
    objects.clear();
}

////
//// class PllModel
////
//
//PllModel::PllModel(PeriodChangeable* object, EventQueue* eq)
//        : EventManager(eq), object(object), plle(new PllEvent(this)) {}
//
//void PllModel::changePeriod(Tick period)
//{
//    plle->start(period);
//}
//
//Tick PllModel::clockPeriod() const
//{
//    return object->clockPeriod();
//}
//
//Tick PllModel::clockEdge(Cycles cycles) const
//{
//    return object->clockEdge(cycles);
//}
//
//PeriodChangeable *PllModel::getParent()
//{
//    return object;
//}
//
//PllModel::~PllModel()
//{
//    delete plle;
//}
//
//
////
//// class PllModel::PllEvent
////
//
//static const double w=1e8; //With these values of w and e the response settles in ~100ns
//static const double e=0.7; //0<e<1. Note: 1 does not work, so don't try.
//static const int multiplier=1;//16;
//
//PllModel::PllEvent::PllEvent(PllModel *model)
//        : model(model), inProgress(false) {}
//
//void PllModel::PllEvent::start(Tick period)
//{
//    if(inProgress) model->deschedule(this);
//    inProgress=true;
//    
//    cout<<"--> changing period from "<<model->object->clockPeriod()
//        <<" to "<<period<< " ("<<SimClock::Float::Hz<<")"<<endl;
//    
//    pStart=static_cast<double>(model->object->clockPeriod())*SimClock::Float::Hz;
//    pEnd=static_cast<double>(period)*SimClock::Float::Hz;
//    tStart=t=model->object->clockEdge(Cycles(0));
//    tEnd=t+static_cast<Tick>(7.0/(w*e*SimClock::Float::Hz));
//    
//    cout<<"--> t="<<t<<" tEnd="<<tEnd<<endl;
//    
//    if(t==curTick()) process();
//    else model->schedule(this,t);
//}
//
//void PllModel::PllEvent::process()
//{
//    assert(curTick()==model->object->clockEdge(Cycles(0)));
//    assert(curTick()==t);
//    
//    if(t>=tEnd)
//    {
//        inProgress=false;
//        model->object->changePeriod(static_cast<Tick>(pEnd/SimClock::Float::Hz+0.5));
//        
//        cout<<"--> @ t="<<curTick()<<" end."<<endl;
//        return;
//    }
//    
//    double tf=static_cast<double>(t-tStart)*SimClock::Float::Hz;
//    double pf=pStart+(pEnd-pStart)*(1-1/(sqrt(1-e*e))*exp(-e*w*tf)*sin(w*tf*sqrt(1-e*e)+acos(e)));
//    Tick p=static_cast<Tick>(pf/SimClock::Float::Hz+0.5);
//    t+=multiplier*p;
//    
//    cout<<curTick()<<" "<<p<<endl;
//    
//    model->object->changePeriod(p);
//    model->schedule(this,t); 
//}
//
//PllModel::PllEvent::~PllEvent()
//{
//    if(inProgress) model->deschedule(this);
//}

//
// class AdvancedPllModel
//

//#define DUMP

AdvancedPllModel::AdvancedPllModel(PeriodChangeable* object, EventQueue* eq)
        : EventManager(eq), object(object), plle(0), first(true)
{
    #ifdef DUMP
    stringstream ss;
    ss<<"m5out/advanced_pll_freq"<<id++<<".txt";
    pll_f.open(ss.str());
    #endif //DUMP
    vreg=new VoltageRegulatorModel();
}

void AdvancedPllModel::changePeriod(Tick period)
{
    if(first)
    {
        first=false;
        plle=new PllEvent(this);
    }
    plle->changePeriod(period);
}

Tick AdvancedPllModel::clockPeriod() const
{
    return object->clockPeriod();
}

Tick AdvancedPllModel::clockEdge(Cycles cycles) const
{
    return object->clockEdge(cycles);
}

PeriodChangeable *AdvancedPllModel::getParent()
{
    return object;
}

AdvancedPllModel::~AdvancedPllModel()
{
    if(plle) delete plle;
}


//
// class AdvancedPllModel::PllEvent
//

//From Scilab code: e=0.6; w=4e6; s=1/(1+2*e*%s/w+%s^2/w^2); tf2ss(syslin('c',s))
const double AdvancedPllModel::a12=4194304;
const double AdvancedPllModel::a21=-3814697.3;
const double AdvancedPllModel::a22=-4800000;
const double AdvancedPllModel::b2=4000000;
const double AdvancedPllModel::c1=0.9536743;
const int AdvancedPllModel::multiplier2=8; //Run PLL model every 8 clock cycles
int AdvancedPllModel::id=0;

AdvancedPllModel::PllEvent::PllEvent(AdvancedPllModel *model)
        : model(model)
{
    y=u=SimClock::Float::s/static_cast<double>(model->object->clockPeriod());
    double h=static_cast<double>(multiplier2)/y; //Integration step
    x1=y/c1;
    x2=(h*a21*x1+h*b2*u)/(-h*h*a12*a21+h*a22);
    model->schedule(this,model->object->clockEdge(Cycles(0)));
}

void AdvancedPllModel::PllEvent::changePeriod(Tick period)
{
    u=SimClock::Float::s/static_cast<double>(period);
}

void AdvancedPllModel::PllEvent::process()
{
    assert(curTick()==model->object->clockEdge(Cycles(0)));
    
    double h=static_cast<double>(multiplier2)/y; //Integration step
    double uActual=model->getVoltageRegulator()->advance(u);
    x2=(x2+h*a21*x1+h*b2*uActual)/(1-h*h*a21*a12-h*a22);
    x1=x1+h*a12*x2;
    y=c1*x1;
    //Prevent clock to become less than 30MHz, because
    //if it becomes zero or negative, the variable integration
    //step will fail. This is not unrealistic as any VCO has
    //a minimum frequency. There is a tradeoff between this
    //value, the k value and the maximum step change that
    //can be simulated without incurring in simulation errors.
    y=max(30e6,y);

    Tick p=static_cast<Tick>(SimClock::Float::s/y+0.5);
    model->object->changePeriod(p);
   
    #ifdef DUMP
   	model->getPllFile()<<curTick()<<" "<<p<<endl;
    #endif //DUMP
    
    model->schedule(this,curTick()+multiplier2*p);
}

AdvancedPllModel::PllEvent::~PllEvent()
{
    model->deschedule(this);
}

/*
 * This class resets the statistics of the average frequency
 */
class VoltageRegulatorStatsResetCallback : public Callback
{
public:
    virtual void process()
    {
        //for(auto vreg : vregs) vreg->resetAverageVoltageStats();
        for(auto vreg=vregs.begin();vreg!=vregs.end();vreg++) (*vreg)->resetAverageVoltageStats();
    }
    
    static VoltageRegulatorStatsResetCallback& instance()
    {
        static VoltageRegulatorStatsResetCallback *singleton=0;
        if(singleton==0) singleton=new VoltageRegulatorStatsResetCallback;
        return *singleton;
    }
    
    void add(VoltageRegulatorBase *vreg) { vregs.insert(vreg); }
    
    void remove(VoltageRegulatorBase *vreg) { vregs.erase(vreg); }
    
private:
    VoltageRegulatorStatsResetCallback()
    {
        Stats::registerResetCallback(this);
    }
    
    set<VoltageRegulatorBase *> vregs;
};

//
// class VoltageRegulatorBase
//

VoltageRegulatorBase::VoltageRegulatorBase()
{
    VoltageRegulatorStatsResetCallback::instance().add(this);
}

VoltageRegulatorBase::~VoltageRegulatorBase()
{
    VoltageRegulatorStatsResetCallback::instance().remove(this);
}

//
// class DummyVoltageRegulator
//

double DummyVoltageRegulator::advance(double frequency) { return frequency; }
double DummyVoltageRegulator::voltage() { return 1.1; }
double DummyVoltageRegulator::averageVoltage() { return 1.1; }
void DummyVoltageRegulator::resetAverageVoltageStats() {}

//
// class VoltageRegulatorModel
//

//From Scilab code: e=0.6; w=2e6; s=1/(1+2*e*%s/w+%s^2/w^2); tf2ss(syslin('c',s))
const double VoltageRegulatorModel::a12=2097152;
const double VoltageRegulatorModel::a21=-1907348.6;
const double VoltageRegulatorModel::a22=-2400000;
const double VoltageRegulatorModel::b2=2000000;
const double VoltageRegulatorModel::c1=0.9536743;
const double VoltageRegulatorModel::settlingV=2e-6; //Settling time for interlock
const double VoltageRegulatorModel::settlingF=1e-6; //Settling time for interlock
int VoltageRegulatorModel::id=0;

VoltageRegulatorModel::VoltageRegulatorModel() : first(true) 
{
    #ifdef DUMP
    stringstream ss;
    ss<<"m5out/vreg_voltage"<<id++<<".txt";
    vregf.open(ss.str());
    #endif //DUMP
    y=0.0;
    weightedVoltSum=0.0;
    statsBeginTime=curTick();
}

double VoltageRegulatorModel::advance(double frequency)
{
    Tick cur=curTick();
    if(first)
    {
        first=false;
        prevTick=cur;
        y=u=f2v(frequency);
        double h=1.0/frequency; //Integration step
        x1=y/c1;
        x2=(h*a21*x1+h*b2*u)/(-h*h*a12*a21+h*a22);
        fdelay=vdelay=0;
        fOld=frequency;
        vOld=u;
    } else {
        assert(cur>prevTick);
        
        double vNew=f2v(frequency);
        if(vNew>vOld)
        {
            fdelay=cur+settlingV*SimClock::Float::s;//Delay freq by voltage settling time
        } else if(vNew<vOld) {
            vdelay=cur+settlingF*SimClock::Float::s;//Delay voltage by frequency settling time
        }
        vOld=vNew;
        if(cur>=vdelay) u=vNew;
        if(cur>=fdelay) fOld=frequency;
        
        double h=static_cast<double>(cur-prevTick)/SimClock::Float::s; //Integration step
        weightedVoltSum += y * (cur - max(prevTick,statsBeginTime));
        prevTick=cur;
        
        x2=(x2+h*a21*x1+h*b2*u)/(1-h*h*a21*a12-h*a22);
        x1=x1+h*a12*x2;
        y=c1*x1;
    }
    #ifdef DUMP
   	vregf<<curTick()<<" "<<y<<endl;
    #endif //DUMP
    return fOld;
}

double VoltageRegulatorModel::voltage()
{
    return y;
}

double VoltageRegulatorModel::averageVoltage()
{
    Tick cur = curTick();
    double temp = weightedVoltSum;
    temp += y * (cur - max(prevTick,statsBeginTime));
    temp /= cur-statsBeginTime;
    return temp;
}

void VoltageRegulatorModel::resetAverageVoltageStats()
{
    weightedVoltSum = 0.0;
    statsBeginTime = curTick();
}

double VoltageRegulatorModel::f2v(double f)
{
    if(f<4e8) return 0.8;       //0.8 if less than 400MHz
    else if(f<8e8) return 0.9;  //0.9 if less than 800MHz
    else if(f<12e8) return 1.0; //1.0 if less than 1.2GHz
    else return 1.1;            //1.1 if upper
}

VoltageRegulatorModel::~VoltageRegulatorModel() {}
