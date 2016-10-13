/*
#include <octave/oct.h>
#include <octave/octave.h>
#include <octave/parse.h>
#include <octave/toplev.h>
*/
#include "DVFSPolicyO3Core.h"
#include "config/core_dvfs_policy.hh"
#include "sim/dvfs.hh"
#include <iostream>
#include <fstream>
#include <unistd.h>

using namespace std;

#define PLL //Change frequency using PLL

/**
 * Duty cycle policy
 */
class DutyCycleCores : public DVFSPolicyO3Core
{
public:
    DutyCycleCores(EventQueue *eventq) : DVFSPolicyO3Core(eventq), event(this),
#ifdef PLL
            pll(&cores,eventq),
#endif //PLL
            toggle(0),plusduty(0),minusduty(0),plusperiod(0),minusperiod(0) {}
        
    virtual void addCoreImpl(BaseCPU* cpu)
    {
        if(cores.empty())
        {
            ifstream in("freq_core.txt");
            if(in)
            {
                string descr;
                getline(in,descr);
                in>>plusduty>>minusduty>>plusperiod>>minusperiod;
                cout<<"Period data loaded from freq_core.txt ("
                    <<plusduty<<","<<minusduty<<","
                    <<plusperiod<<","<<minusperiod<<")"<<endl;
            }
            if(plusduty!=0 && minusduty!=0 && plusperiod!=0 && minusperiod!=0)
                eventq->schedule(&event,initialDelay);
            else cout<<"DFS policy on CPU disabled in freq_core.txt"<<endl;
        }
        cores.addObject(cpu);
        #ifdef PLL
        cpu->setVoltageRegulator(pll.getVoltageRegulator());
        #else //PLL
        cpu->setVoltageRegulator(new DummyVoltageRegulator);
        #endif //PLL
    }
    
    void dvfsEvent()
    {
        Tick p = toggle ? minusperiod : plusperiod;
            
//        cout<<"--> @"<<curTick()<<" Changing period to "<<p
//            <<" ("<<cores.getObjList().size()<<" items)"<<endl;

#ifdef PLL
        pll.changePeriod(p);
#else //PLL
        cores.changePeriod(p);
#endif //PLL
        eventq->schedule(&event,curTick()+(toggle ? minusduty : plusduty));
        toggle^=1;
    }
    
    virtual ~DutyCycleCores()
    {
        //To prevent an assertion at gem5 exit
        if(event.scheduled()) eventq->deschedule(&event);
    }
    
private:
    EventWrapper<DutyCycleCores, &DutyCycleCores::dvfsEvent> event;
    FrequencyIsland cores;
#ifdef PLL
    AdvancedPllModel pll;
#endif //PLL
    bool toggle;
    Tick plusduty;
    Tick minusduty;
    Tick plusperiod;
    Tick minusperiod;
    static const Tick initialDelay=10000ull;
};

/**
 * Duty cycle policy
 */
class DutyCycleOneCore : public DVFSPolicyO3Core
{
public:
    DutyCycleOneCore(EventQueue *eventq) : DVFSPolicyO3Core(eventq), event(this),
#ifdef PLL
            pll(&cores,eventq),
#endif //PLL
            toggle(0),plusduty(0),minusduty(0),plusperiod(0),minusperiod(0),
            id(-1),first(true) {}
        
    virtual void addCoreImpl(BaseCPU* cpu)
    {
        if(first)
        {
            first=false;
            ifstream in("freq_core.txt");
            if(in)
            {
                string descr;
                getline(in,descr);
                in>>plusduty>>minusduty>>plusperiod>>minusperiod>>id;
                cout<<"Period data loaded from freq_core.txt ("
                    <<plusduty<<","<<minusduty<<","
                    <<plusperiod<<","<<minusperiod<<","<<id<<")"<<endl;
            }
            if(plusduty!=0 && minusduty!=0 && plusperiod!=0 && minusperiod!=0
               && id!=-1) eventq->schedule(&event,initialDelay);
            else cout<<"DFS policy on CPU disabled in freq_core.txt"<<endl;
        }
        if(cpu->cpuId()==id) cores.addObject(cpu);
        #ifdef PLL
        if(cpu->cpuId()==id) cpu->setVoltageRegulator(pll.getVoltageRegulator());
        else cpu->setVoltageRegulator(new DummyVoltageRegulator);
        #else //PLL
        cpu->setVoltageRegulator(new DummyVoltageRegulator);
        #endif //PLL
    }
    
    void dvfsEvent()
    {
        Tick p = toggle ? minusperiod : plusperiod;
            
//        cout<<"--> @"<<curTick()<<" Changing period to "<<p
//            <<" ("<<cores.getObjList().size()<<" items)"<<endl;

#ifdef PLL
        pll.changePeriod(p);
#else //PLL
        cores.changePeriod(p);
#endif //PLL
        eventq->schedule(&event,curTick()+(toggle ? minusduty : plusduty));
        toggle^=1;
    }
    
    virtual ~DutyCycleOneCore()
    {
        //To prevent an assertion at gem5 exit
        if(event.scheduled()) eventq->deschedule(&event);
    }
    
private:
    EventWrapper<DutyCycleOneCore, &DutyCycleOneCore::dvfsEvent> event;
    FrequencyIsland cores;
#ifdef PLL
    AdvancedPllModel pll;
#endif //PLL
    bool toggle;
    Tick plusduty;
    Tick minusduty;
    Tick plusperiod;
    Tick minusperiod;
    int id;
    bool first;
    static const Tick initialDelay=10000ull;
};

/**
 * One freq change all cores from 2GHz (default) to 1GHz after 280ms
 */
class OneChange : public DVFSPolicyO3Core
{
public:
    OneChange(EventQueue *eventq) : DVFSPolicyO3Core(eventq), event(this),
        pll(&cores,eventq), first(true), second(true) {}
        
    virtual void addCoreImpl(BaseCPU* cpu)
    {
        if(first)
        {
            first=false;
            eventq->schedule(&event,1000ull);
        }
        cores.addObject(cpu);
        cpu->setVoltageRegulator(pll.getVoltageRegulator());
    }
    
    void dvfsEvent()
    {
        if(second)
        {
            second=false;
            eventq->schedule(&event,280000000000ull); //280ms
            pll.changePeriod(500); //This is important!
        } else pll.changePeriod(1000); //period=1ns 1GHz
    }
    
    virtual ~OneChange()
    {
        //To prevent an assertion at gem5 exit
        if(event.scheduled()) eventq->deschedule(&event);
    }
    
private:
    EventWrapper<OneChange, &OneChange::dvfsEvent> event;
    FrequencyIsland cores;
    AdvancedPllModel pll;
    bool first;
    bool second;
};

/**
 * Octave policy
 */
/*
class OctavePolicy : public DVFSPolicyO3Core
{
public:
    OctavePolicy(EventQueue *eventq) : DVFSPolicyO3Core(eventq), event(this),
        first(true)
    {
        const char * argvv[]={"","--silent"};
        octave_main(2,(char **)argvv,true);
        char path[4096];
        if(getcwd(path,sizeof(path))==NULL) assert(false);
        string cwd=string(path);
        octave_value_list args;
        args(0)=cwd;
        feval("cd",args);
    }
        
    virtual void addCoreImpl(BaseCPU* cpu)
    {
        if(first)
        {
            first=false;
            eventq->schedule(&event,1000ull);
        }
        auto pll=new AdvancedPllModel(cpu,eventq);
        cpu->setVoltageRegulator(pll->getVoltageRegulator());
        plls[cpu->cpuId()]=pll;
    }
    
    void dvfsEvent()
    {
		// replaced since not supported at upv (c++2011)
        //for(auto it=plls.begin();it!=plls.end();++it)
        //    it->second->changePeriod(500); //This is important!

		for(int i=0;i<plls.size();i++)
            (plls.at(i))->changePeriod(500); //This is important!

        Dvfs *dvfs=Dvfs::instance();
        assert(dvfs);
        dvfs->registerThermalPolicyCallback(
            bind(&OctavePolicy::runOctaveFunction,this,placeholders::_1));
    }
    
    void runOctaveFunction(Dvfs *dvfs)
    {
        octave_value_list args;
        Matrix temperature(1,dvfs->getNumCpus());
        for(int i=0;i<dvfs->getNumCpus();i++)
            temperature(0,i)=dvfs->getCpuTemperature(i);
        args(0)=temperature;
        octave_value_list result=feval("dvfs_core",args,1);
        Matrix frequency=result(0).matrix_value();
        cout<<"======================================================"<<endl;
        for(int i=0;i<dvfs->getNumCpus();i++)
        {
            auto it=plls.find(i);
            assert(it!=plls.end());
            double f=frequency(0,i);
            //Sanity check, freq > 10MHz. If the octave file isn't found or
            //doesn't return the correct type, the MAtrix class subscript
            //returns zero, so if this fails check your octave file
            assert(f>10e6);
            Tick p=static_cast<Tick>(SimClock::Float::s/f+0.5);
            it->second->changePeriod(p);
            cout<<p<<" ";
        }
        cout<<endl<<"======================================================"<<endl;
    }
    
    virtual ~OctavePolicy()
    {
        //To prevent an assertion at gem5 exit
        if(event.scheduled()) eventq->deschedule(&event);
        do_octave_atexit();
    }
    
private:
    EventWrapper<OctavePolicy, &OctavePolicy::dvfsEvent> event;
    //FrequencyIsland *cores;
    map<int,AdvancedPllModel*> plls;
    bool first;
};
*/

//
// class DVFSPolicyO3Core
//

void DVFSPolicyO3Core::addCore(BaseCPU* core, EventQueue* eventq)
{
    string option=CORE_DVFS_POLICY;
    static DVFSPolicyO3Core *policy=0;
    if(policy==0)
    {
        if(option=="DUTY_CYCLE_ALL") policy=new DutyCycleCores(eventq);
        else if(option=="DUTY_CYCLE_ONE") policy=new DutyCycleOneCore(eventq);
        else if(option=="ONE_CHANGE") policy=new OneChange(eventq);
        //else if(option=="OCTAVE_POLICY") policy=new OctavePolicy(eventq);
    }
    if(policy==0) panic("No DVFS policy for cores specified");
    policy->addCoreImpl(core);
}

DVFSPolicyO3Core::DVFSPolicyO3Core(EventQueue* eventq) : eventq(eventq) {}

void DVFSPolicyO3Core::addCoreImpl(BaseCPU* cpu) {}

DVFSPolicyO3Core::~DVFSPolicyO3Core() {}
