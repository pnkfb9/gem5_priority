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

#ifndef DVFSPOLICY_H
#define	DVFSPOLICY_H
#include "Router_d.hh"
#include "Router_PNET_Container_d.hh"
#include "sim/core.hh"
#include <fstream>
#include <algorithm>
#include <vector>
#include <sstream>
#include <utility>
#include <map>

#include <fstream>


#define PRINT_FREQ_DVFS_ROUTERS 0
/**
 * Base class for DVFS policies. Also, this calls defines the defualt policy
 * which does nothing
 */


class DVFSPolicy
{
public:
    /**
     * Called by router_d constructor to add a router
     * \param router new router instantiated
     */
    static void addRouter(Router_d *router, EventQueue *eventq);
    
    /**
     * Needed by EventWrapper for debugging purposes
     */
    std::string name() const { return typeid(*this).name(); }
    
    virtual ~DVFSPolicy();
    
protected:
    explicit DVFSPolicy(EventQueue *eventq);
    
    virtual void addRouterImpl(Router_d *router);
   
   	double frequencyToVoltage(Tick p /*the required period*/)
	{
		if(p<1000)
			p=1000;
		if(p>10000)
			p=10000;
			
		//assert(p>=1000 && p<=10000 && "This Freq 2 Vdd func works between 100MHz and 1GHz only");
	
		double RequiredVdd=1.0;
		
		if(p<=1250 /*>=800MHz*/)
			RequiredVdd = 1.0;
		else if(p <= 2000	/*>=500*/)
			RequiredVdd = 0.9;
		else if(p <= 4000 /*>=250*/)
			RequiredVdd = 0.8;
		else /*lower than 250MHz*/
			RequiredVdd = 0.7; 
		return RequiredVdd;
	}

    EventQueue *eventq;
};

/**
 * PNET policy 
 * For each PNET a different DVFS control is applied changing both frequency and
 * voltage. The key point is that the resynchronizers are only used at the
 * borders of the topology
 * NOTE: 
 *	1) works on all routers, each PNET is a VFI!!!
 *  2) DVFS implementation through the simple policy to regulate voltage
 *  depending on the required frequency 
 */


class AllPnetDVFS : public DVFSPolicy
{
public:
    std::map<int,FrequencyIsland*> freq;
    std::map<int,AdvancedPllModel*> pll;

	
	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

	std::ifstream k_file;
	double k;
	bool is_first_time;
	Tick voltageTransientPeriod;

// START - this is for stability analysis only (NEVER USED IN SIMULATION)
	std::ofstream u_unconstr_file;
// END - this is for stability analysis only (NEVER USED IN SIMULATION)

    EventQueue *getQueue() { return eventq; }
    
    AllPnetDVFS(EventQueue *eventq)
				: DVFSPolicy(eventq),k_file("k.txt"), u_unconstr_file("u_unconstr_file.txt")
    {
		k=0;
		assert(k_file.is_open());		
		k_file>>k;
		assert(k!=0);
		is_first_time=true;
		voltageTransientPeriod=5000000; // 5us
		
		//generate PLLs and VFI in a number equal to the protocol's vnets!!
		for(int i=0;i<3;i++)
		{			
			freq[i]=new FrequencyIsland();
			pll[i]=new AdvancedPllModel((freq.find(i)->second),eventq);
		}	
	}

    virtual void addRouterImpl(Router_PNET_Container_d *router)
    {
        using namespace std;
		std::vector<Router_d*> r_pnet = router->getContainedRoutersRef();
		//assign each router to the correct pnet vfi
		for(int i=0;i<r_pnet.size();i++)
		{

			(*(freq.find(i)->second)).addObject(r_pnet.at(i));
			r_pnet.at(i)->setVoltageRegulator(pll[i]->getVoltageRegulator());
		}
		// schedule 1 event for PNET to pass the correct old u values
		if(is_first_time)
		{
			is_first_time=false;
			for(int i=0;i<r_pnet.size();i++)
				eventq->schedule(new MainDVFSEvent(this,1e8 /*initial u*/, /*VFI ptr*/(freq.find(i)->second),0,1.0 /*initial u_unconstr*/),initialDelay);
		}
        
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(AllPnetDVFS *parent, double u_old, FrequencyIsland* fi,int fi_id,double actualVdd) : 
									Event(Default_Pri, AutoDelete), parent(parent),u_old(u_old),fi(fi),fi_id(fi_id),actualVdd(actualVdd) {}
        
        virtual void process()
        {
			#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<"################ ->>> @"<<curTick()<<" FI"<< fi_id <<" change freq after a voltage scale up "<<r->clockPeriod()<<" -> "<< new_period  <<std::endl;
			#endif
			// if we arrived here we paid a voltage change
	   		Tick new_period = (unsigned long long)(1.0/u_old*SimClock::Float::s);
			std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(fi_id);
			assert(it_pll->second!=NULL);
			(*(it_pll->second)).changePeriod(new_period);

            parent->getQueue()->schedule(new MainDVFSEvent(parent,u_old,fi,fi_id,actualVdd), curTick());
        }
    private:
        AllPnetDVFS *parent;
		Tick new_period;
		double u_old;
		FrequencyIsland *fi;
		int fi_id;
		// for stability analysis DISABLE IF NOT USED

		double actualVdd;

    };
	/*this event is to regulate the voltage*/
    
	class MainDVFSEvent : public Event
    {
    public:
		MainDVFSEvent(AllPnetDVFS *parent, double u_old, FrequencyIsland* fi,int fi_id, double actualVdd): 
									Event(Default_Pri, AutoDelete), parent(parent),
									u_old(u_old),fi(fi),fi_id(fi_id), actualVdd(actualVdd) {}
        
        virtual void process()
        {
			std::vector<PeriodChangeable*>& temp_fi = fi->getObjList();
			int temp_congestion=0;
			assert(temp_fi.size()>0);
			// update power evaluation
			for(int i=0;i<temp_fi.size();i++)
			{
				Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);	
				assert(r!=NULL);
				temp_congestion+=r->getFullCongestion();
			//	if(r->get_id()==5 || r->get_id()==0)
			//	{
			//		std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
			//		r->printRouterPower(*(it_pwr->second),actualVdd);
			//	}
			//	else
				{
					r->computeRouterPower(actualVdd); 
				}
			}


			int actual_congestion=temp_congestion/temp_fi.size();
          
		    // Classical control policy
			double k=parent->k;
		   	//const double k=1; //Obtained from closed loop control with identified model
			const double u_old_factor=0.99; // place a pole in the controller to	smooth its actuation
			const double min_freq=1e8; //min frequency 100MHz
			const double max_freq=1e9; //max frequency 1GHz	
			assert(actual_congestion >= 0);
			const double scaleFactor=1e9; //Identification was done with freq in GHz
		
		   	double u = std::min(max_freq,min_freq+k*actual_congestion*scaleFactor);
			u=u_old_factor*u_old+(1-u_old_factor)*u;

			Tick new_period = (unsigned long long)(1.0/u*SimClock::Float::s);
		// HERE I know the new frequency to be actuated, then I have to decide
		// the dealy to impose due to a possible voltage increase.

			double new_vdd=parent->frequencyToVoltage(new_period /*the required period*/);
			#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<"@"<<curTick()<<" VoltageEvent on R"<< r->get_id() <<" : ["
					<< r->clockPeriod()<<" -> "<< new_period <<"] ["
					<< actualVdd<< " -> "<< new_vdd<< "]"<<std::endl;
			#endif



			if(new_vdd>actualVdd)
			//if(r->clockPeriod()>new_period)
			{
				#if PRINT_FREQ_DVFS_ROUTERS == 1
				std::cout<<"\t waiting for VDD increase"<<std::endl;
				#endif
				parent->getQueue()->schedule(new DVFSEvent(parent,u,fi,fi_id,new_vdd), curTick()+parent->voltageTransientPeriod);
			}
			else
			{// immediately start a frequency change and reschedule the control law event
				#if PRINT_FREQ_DVFS_ROUTERS == 1
				std::cout<<"\t change the frequency now..."<<std::endl;
				#endif
				assert(fi_id>=0 &&fi_id<=3);
				std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(fi_id);
				assert(it_pll->second!=NULL);
				(*(it_pll->second)).changePeriod(new_period);

				const int samplePeriod=100000;//1.0/1e7*SimClock::Float::s; //100000ps=100ns
				parent->getQueue()->schedule(new MainDVFSEvent(parent,u,fi,fi_id,new_vdd), curTick()+samplePeriod);
			}
        }
    private:
        AllPnetDVFS *parent;
		double u_old;
		FrequencyIsland *fi;
		int fi_id;
		double actualVdd;
    };



    static const Tick initialDelay=10000ull;
};



///**
// * Change period of all router, only once (using PllModel)
// */
//class ChangeAllRoutersOnce : public DVFSPolicy
//{
//public:
//    ChangeAllRoutersOnce(EventQueue *eventq)
//            : DVFSPolicy(eventq), first(true), freq(), pll(&freq,eventq) {}
//    
//    virtual void addRouterImpl(Router_d *router)
//    {
//        freq.addObject(router);
//        if(!first) return;
//        first=false;
//        eventq->schedule(new DVFSEvent(&pll),100000);
//    }
//    
//private:
//    class DVFSEvent : public Event
//    {
//    public:
//        DVFSEvent(PllModel *pll) : Event(Default_Pri, AutoDelete), pll(pll) {}
//        virtual void process()
//        {
//            pll->changePeriod(500);
//        }
//    private:
//        PllModel *pll;
//    };
//    
//    bool first;
//    FrequencyIsland freq;
//    
//    PllModel pll;
//};

/**
 * Change period of all router, only once (using AdvancedPllModel)
 */
class ChangeAllRoutersOnce2 : public DVFSPolicy
{
public:
    ChangeAllRoutersOnce2(EventQueue *eventq)
            : DVFSPolicy(eventq), first(true), freq(), pll(&freq,eventq) {}
    
    virtual void addRouterImpl(Router_d *router)
    {
        freq.addObject(router);
        router->setVoltageRegulator(pll.getVoltageRegulator());
        if(!first) return;
        first=false;
        eventq->schedule(new DVFSEvent(&pll),100000);
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(AdvancedPllModel *pll) : Event(Default_Pri, AutoDelete), pll(pll) {}
        virtual void process()
        {
            pll->changePeriod(500);
        }
    private:
        AdvancedPllModel *pll;
    };
    
    bool first;
    FrequencyIsland freq;
    
    AdvancedPllModel pll;
};

/**
 * Change all router periods in a square wave way, but R5 and R0 that are
 * sensed. This policy is useful for the nonlinear dynamic system
 * identification, where x(k+1)= [1-alpha*u(k)]*x(k)+d(k). The model is used for
 * the switched system control law.
 * 
 * NOTE: to use the policy set the fixed_router_freq.txt, i.e. single number
 * reporting the period with respect to ps file routers with static
 * frequencies for which you have to identify the model. E.g. 4000 is 250MHz,
 * 500 is 2GHz 
 */
class DutyCycleSwitchedSystemRouters : public DVFSPolicy
{
public:
    Tick plusduty;
    Tick minusduty;
    Tick plusperiod;
    Tick minusperiod;
	
	Tick fixed_router_freq;

   	std::map<int,std::ofstream*> dump_pwr_f;

    EventQueue *getQueue() { return eventq; }
    
    DutyCycleSwitchedSystemRouters(EventQueue *eventq) : DVFSPolicy(eventq)
    {
         plusduty=10000ull; //In simulation ticks
         minusduty=10000ull;
         plusperiod=2000ull;
         minusperiod=500ull;
		 fixed_router_freq=0;
    }
        
    virtual void addRouterImpl(Router_d *router)
    {
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(routers.empty())
        {
			using namespace std;
			ifstream fixed_router_freq_f("fixed_router_freq.txt");
			assert(fixed_router_freq_f);
			fixed_router_freq_f>>fixed_router_freq;
			assert(fixed_router_freq>0);
            
			ifstream in("freq.txt");
            assert(in);
            string descr;
            getline(in,descr);
            in>>plusduty>>minusduty>>plusperiod>>minusperiod;
            cout<<"Period data loaded from freq.txt ("
                    <<plusduty<<","<<minusduty<<","
                    <<plusperiod<<","<<minusperiod<<")"<<endl;
			if(plusduty!=0 && minusduty!=0)
                eventq->schedule(new DVFSEvent(&routers,this),initialDelay);
        }
		if((router->get_id()==5) || (router->get_id()==0))
		{
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";
			dump_pwr_f[router->get_id()]=new std::ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());
		}
        routers.addObject(router);
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(FrequencyIsland *routers, DutyCycleSwitchedSystemRouters *parent,
                bool toggle=false) : Event(Default_Pri, AutoDelete),
                routers(routers), parent(parent), toggle(toggle) {}
        
        virtual void process()
        {
			Tick p = toggle ? parent->minusperiod : parent->plusperiod;
			std::vector<PeriodChangeable*>& objVect = routers->getObjList();
			#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<"@"<<curTick()<<" Change Freq to "<<p<<" : ";
			#endif
			for(auto it=objVect.begin();it!=objVect.end();it++)
			{
				Router_d* r=dynamic_cast<Router_d*>(*it);
				if(r->get_id()!=5 && r->get_id()!=0)
            	{
					#if PRINT_FREQ_DVFS_ROUTERS == 1
					std::cout<<" R"<<r->get_id();
					#endif
                   	r->changePeriod(p);
          		}
				else
				{			
					std::map<int,std::ofstream*>::iterator it_pwr = 
										parent->dump_pwr_f.find(r->get_id());
					r->changePeriod(parent->fixed_router_freq);
					#if PRINT_FREQ_DVFS_ROUTERS == 1
					std::cout<<" R"<<r->get_id()<<" ( "<<parent->fixed_router_freq <<" ) ";
					#endif
					r->printRouterPower(*(it_pwr->second));	
				}
		   	}
			#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<std::endl;
			#endif
			parent->getQueue()->schedule(
                	new DVFSEvent(routers,parent,!toggle),
                	curTick()+(toggle ? parent->minusduty : parent->plusduty));
        }
    private:
        FrequencyIsland *routers;
        DutyCycleSwitchedSystemRouters *parent;
        bool toggle;
    };
    
    FrequencyIsland routers;
    static const Tick initialDelay=10000ull;
};

/**
 * Changes frequency only of router 5, and senses congestion of that router
 */
class CongestionSensing : public DVFSPolicy
{
public:
    Tick plusduty;
    Tick minusduty;
    Tick plusperiod;
    Tick minusperiod;
    
    std::ofstream dump;
    std::ofstream outfreq;

    EventQueue *getQueue() { return eventq; }
    
    CongestionSensing(EventQueue *eventq)
            : DVFSPolicy(eventq), dump("m5out/dump.txt"), outfreq("m5out/freqout.txt")
    {
        outfreq<<"start_tick\tend_tick\tfreq\ttot_power\tdyn_power"
                 "\tstat_power\tdyn_clock_power\tcong_r5"<<std::endl;
        plusduty=10000ull; //In simulation ticks
        minusduty=10000ull;
        plusperiod=2000ull;
        minusperiod=500ull;
    }

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5)
        {
            ifstream in("freq.txt");
            if(in)
            {
                string descr;
                getline(in,descr);
                in>>plusduty>>minusduty>>plusperiod>>minusperiod;
                cout<<"Period data loaded from freq.txt ("
                    <<plusduty<<","<<minusduty<<","
                    <<plusperiod<<","<<minusperiod<<")"<<endl;
            }
            if(plusduty!=0 && minusduty!=0)
                eventq->schedule(new DVFSEvent(router,this),initialDelay);
        }
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, CongestionSensing *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle), count(count) {}
        
        virtual void process()
        {
            r->printFullCongestionData(parent->dump);
            r->printRouterPower(parent->outfreq);
            
            //Variable duty cycle
            //NOTE: if plusduty or minusduty are not multiples of samplePeriod,
            //the actual plus duty and minus duty are rounded to the next samplePeriod
            const int samplePeriod=100000; //100000ps=100ns
            int limit=toggle ? parent->minusduty : parent->plusduty;
            if(count>=limit)
            {
                count=0;
                toggle=!toggle;
                Tick p = toggle ? parent->minusperiod : parent->plusperiod;
                r->changePeriod(p);
            }
            
            parent->getQueue()->schedule(
                new DVFSEvent(r,parent,toggle,count+samplePeriod),
                curTick()+samplePeriod);
        }
    private:
        Router_d *r;
        CongestionSensing *parent;
        bool toggle;
        int count;
    };
    
    static const Tick initialDelay=10000ull;
};


/**
 * EACH FOUR ROUTER A FREQ ISLAND - TO BE USED WITH 16CORES ARCHS
 *
 * Simple policy that changes frequency only of router 5, and senses congestion
 * of that router. It maintains the router freq at normal level with two
 * congestion thresholds trigger the switch to higher freq or lower freq.
 *
 *
 * NOTE: This policy sits on two ideality:
 *	1) does not account for pll dynamic, i.e. both frequency increase and decrease are instantaneous
 *  2) the frequency can be changed at each sample, i.e. every 100ns too fast.
 *
 */
class AllThresholdPolicyCongestionSensing100_500_1000Grouped : public DVFSPolicy
{
public:
    Tick high_duty;
    Tick low_duty;
    Tick normal_duty;
	int threshold_high;
	int threshold_low;

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

	std::map<int,FrequencyIsland*> freq;

    EventQueue *getQueue() { return eventq; }
    
    AllThresholdPolicyCongestionSensing100_500_1000Grouped(EventQueue *eventq)
            : DVFSPolicy(eventq)    
	{
		
		high_duty=1000; 	 //1000MHz
	    low_duty=10000ull;	 // 100MHz
    	normal_duty=2000ull; // 500MHz

		threshold_high=20;
		threshold_low=10;
		
		for(int i=0;i<4;i++)
		{
			freq[i]=new FrequencyIsland();
		}	
    }

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
        {
			// add an output file per each router managed by this policy
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

        }
		// CHANGE HERE TO GROUP ROUTERS to 1 out of 4 freq island and generate a
		// pll for each freq island
		if(router->get_id()==0 ||router->get_id()==1 
				||router->get_id()==4 || router->get_id()==5)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_0"<<std::endl;
			(*(freq.find(0)->second)).addObject(router);
			if(router->get_id()==5)
			{
				eventq->schedule(new DVFSEvent(this,/*freq island ptr*/(freq.find(0)->second)),initialDelay);
			}
		}

		if(router->get_id()==2 ||router->get_id()==3 
					||router->get_id()==6 || router->get_id()==7)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_1"<<std::endl;
			(*(freq.find(1)->second)).addObject(router);
			if(router->get_id()==7)
			{
				eventq->schedule(new DVFSEvent(this,/*freq island ptr*/(freq.find(1)->second)),initialDelay);
			}
		}

		if(router->get_id()==8 ||router->get_id()==9 
				||router->get_id()==12 || router->get_id()==13)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_2"<<std::endl;
			(*(freq.find(2)->second)).addObject(router);
			if(router->get_id()==13)
			{
				eventq->schedule(new DVFSEvent(this,/*freq island ptr*/(freq.find(2)->second)),initialDelay);
			}
		}

		if(router->get_id()==10 ||router->get_id()==11 
				||router->get_id()==14 || router->get_id()==15)
		{

			std::cout<<"Added R"<<router->get_id()<< " to FI_3"<<std::endl;
			(*(freq.find(3)->second)).addObject(router);
			if(router->get_id()==15)
			{
				eventq->schedule(new DVFSEvent(this,/*freq island ptr*/(freq.find(3)->second)),initialDelay);
			}
		}

    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(AllThresholdPolicyCongestionSensing100_500_1000Grouped *parent,
							FrequencyIsland *fi, bool toggle=false, int count=0)
							: Event(Default_Pri, AutoDelete),
				                parent(parent), fi(fi), toggle(toggle), count(count){}
        
        virtual void process()
        {
			std::vector<PeriodChangeable*>& temp_fi = fi->getObjList();
			int temp_congestion=0;
			assert(temp_fi.size()>0);
			for(int i=0;i<temp_fi.size();i++)
			{
				Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);	
				//std::cout<<"\tFI_"<<fi_id <<" R"<<r->get_id()<<" cong"<<r->getFullCongestion();
				assert(r!=NULL);
				
				temp_congestion+=r->getFullCongestion();

				if(r->get_id()==5||r->get_id()==0)
        		{
        	    	//std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
        	    	//r->printFullCongestionData(*(it->second));
					
					std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
					r->printRouterPower(*(it_pwr->second));
				}
				else
				{
					r->computeRouterPower();
				}
			}
			const int samplePeriod=100000; //100000ps=100ns
			const int limit_to_change=10*samplePeriod;

			int actual_congestion=temp_congestion/temp_fi.size();
			
			//std::cout<<"@"<<curTick()<<" actual_congestion: "<<actual_congestion<<std::endl;
			//Tick p_old=r->clockPeriod();
            if(count>=limit_to_change)
            {
                count=0;
            	Tick p=0;
				if(actual_congestion >= parent->threshold_high)
					p=parent->high_duty;
				else if( parent->threshold_low <= actual_congestion && actual_congestion < parent->threshold_high)
					p=parent->normal_duty;
				else if(parent->threshold_low > actual_congestion)
					p=parent->low_duty;
				else
					assert(false&&"Error assigning congestion to freq mapping");
			
				assert(p!=0);

				// Change frequency to all routers in the frequency island
				for(int i=0;i<temp_fi.size();i++)
				{
					Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);	
					r->changePeriod(p);
				}
			}

            parent->getQueue()->schedule(
                new DVFSEvent(parent, fi,toggle,count+samplePeriod),
                curTick()+samplePeriod);
        }
    private:
        AllThresholdPolicyCongestionSensing100_500_1000Grouped *parent;        
		FrequencyIsland *fi;
        bool toggle;
        int count;
    };
    
    static const Tick initialDelay=10000ull;
};

/**
 * Simple policy that changes frequency only of router 5, and senses congestion
 * of that router. It maintains the router freq at normal level with two
 * congestion thresholds trigger the switch to higher freq or lower freq.
 *
 *
 * NOTE: This policy sits on two ideality:
 *	1) does not account for pll dynamic, i.e. both frequency increase and decrease are instantaneous
 *  2) the frequency can be changed at each sample, i.e. every 100ns too fast.
 *
 */
class AllThresholdPolicyCongestionSensing100_500_1000 : public DVFSPolicy
{
public:
    Tick high_duty;
    Tick low_duty;
    Tick normal_duty;
	int threshold_high;
	int threshold_low;

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

    
    EventQueue *getQueue() { return eventq; }
    
    AllThresholdPolicyCongestionSensing100_500_1000(EventQueue *eventq)
            : DVFSPolicy(eventq)    
	{
		
		high_duty=1000; 	 //1000MHz
	    low_duty=10000ull;	 // 100MHz
    	normal_duty=2000ull; // 500MHz

		threshold_high=20;
		threshold_low=10;
    }

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
        {
			// add an output file per each router managed by this policy
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

        }
        eventq->schedule(new DVFSEvent(router,this),initialDelay);
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, AllThresholdPolicyCongestionSensing100_500_1000 *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle), count(count){}
        
        virtual void process()
        {
			if(r->get_id()==5||r->get_id()==0)
        	{
        	    //std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
        	    //r->printFullCongestionData(*(it->second));
				
				std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
				r->printRouterPower(*(it_pwr->second));
			}
			else
			{
				r->computeRouterPower();
			}
			const int samplePeriod=100000; //100000ps=100ns
			const int limit_to_change=10*samplePeriod;

			int actual_congestion=r->getCongestion();
			
			//std::cout<<"@"<<curTick()<<" actual_congestion: "<<actual_congestion<<std::endl;
			//Tick p_old=r->clockPeriod();
            if(count>=limit_to_change)
            {
                count=0;
            	Tick p=0;
				if(actual_congestion >= parent->threshold_high)
					p=parent->high_duty;
				else if( parent->threshold_low <= actual_congestion && actual_congestion < parent->threshold_high)
					p=parent->normal_duty;
				else if(parent->threshold_low > actual_congestion)
					p=parent->low_duty;
				else
					assert(false&&"Error assigning congestion to freq mapping");
			
				assert(p!=0);
				r->changePeriod(p);
			}

            parent->getQueue()->schedule(
                new DVFSEvent(r,parent,toggle,count+samplePeriod),
                curTick()+samplePeriod);
        }
    private:
        Router_d *r;
        AllThresholdPolicyCongestionSensing100_500_1000 *parent;
        bool toggle;
        int count;
    };
    
    static const Tick initialDelay=10000ull;
};

/**
 * USE VOLTAGE CHANGE - 
 * Simple policy that changes frequency only of router 5, and senses congestion
 * of that router. It maintains the router freq at normal level with two
 * congestion thresholds trigger the switch to higher freq or lower freq.
 *
 *
 * NOTE: This policy sits on two ideality:
 *	1) does not account for pll dynamic, i.e. both frequency increase and decrease are instantaneous
 *  2) the frequency can be changed at each sample, i.e. every 100ns too fast.
 *
 */
class ThresholdPolicyCongestionSensing100_500_1000_DVFS : public DVFSPolicy
{
public:
    Tick high_duty;
    Tick low_duty;
    Tick normal_duty;
	int threshold_high;
	int threshold_low;
	Tick voltageTransientPeriod;

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

    
    EventQueue *getQueue() { return eventq; }
    
    ThresholdPolicyCongestionSensing100_500_1000_DVFS(EventQueue *eventq)
            : DVFSPolicy(eventq)    
	{
		
		high_duty=1000; 	 //1000MHz
	    low_duty=10000ull;	 // 100MHz
    	normal_duty=2000ull; // 500MHz

		threshold_high=20;
		threshold_low=10;
		voltageTransientPeriod=8000000;
    }

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
        {
			// add an output file per each router managed by this policy
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());
	
			// schedule event to check the voltage change
            eventq->schedule(new FirstDVFSEvent(router,this),initialDelay);
        }
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, ThresholdPolicyCongestionSensing100_500_1000_DVFS *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle), count(count){}
        
        virtual void process()
        {
			


			const int samplePeriod=100000; //100000ps=100ns
			//const int limit_to_change=10*samplePeriod;

			int actual_congestion=r->getCongestion();
			
            count=0;
        	Tick p=0;
			if(actual_congestion >= parent->threshold_high)
				p=parent->high_duty;
			else if( parent->threshold_low <= actual_congestion && actual_congestion < parent->threshold_high)
				p=parent->normal_duty;
			else if(parent->threshold_low > actual_congestion)
				p=parent->low_duty;
			else
				assert(false&&"Error assigning congestion to freq mapping");

		
			assert(p!=0);
			r->changePeriod(p);

#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<"@"<<curTick()<<" R"<<r->get_id()<<" changed frequency " <<r->clockPeriod()<<std::endl;
#endif
			std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
			r->printRouterPower(*(it_pwr->second),parent->frequencyToVoltage(p));
   
   			parent->getQueue()->schedule( new FirstDVFSEvent(r,parent,toggle,count+samplePeriod), curTick()+samplePeriod);
        }
    private:
        Router_d *r;
	    ThresholdPolicyCongestionSensing100_500_1000_DVFS *parent;
        bool toggle;
        int count;
    };

    class FirstDVFSEvent : public Event
    {
    public:
        FirstDVFSEvent(Router_d *r, ThresholdPolicyCongestionSensing100_500_1000_DVFS *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle), count(count){}
        
        virtual void process()
        {
			const int samplePeriod=100000; //100000ps=100ns
			//const int limit_to_change=10*samplePeriod;

			int actual_congestion=r->getCongestion();
			
            count=0;
        	Tick p=0;
			if(actual_congestion >= parent->threshold_high)
				p=parent->high_duty;
			else if( parent->threshold_low <= actual_congestion && actual_congestion < parent->threshold_high)
				p=parent->normal_duty;
			else if(parent->threshold_low > actual_congestion)
				p=parent->low_duty;
			else
				assert(false&&"Error assigning congestion to freq mapping");
		
			assert(p!=0);

			if(r->clockPeriod()>p)
			{
#if PRINT_FREQ_DVFS_ROUTERS == 1
				std::cout<<"@"<<curTick()<<" R"<<r->get_id()<<" is waiting for VDD increase: " <<r->clockPeriod()<<" -> "<< p<<std::endl;
#endif
				parent->getQueue()->schedule(new DVFSEvent(r,parent,toggle,count+samplePeriod),curTick()+parent->voltageTransientPeriod);
			}
			else
				parent->getQueue()->schedule(new DVFSEvent(r,parent,toggle,count+samplePeriod),curTick()+1);
        }
    private:
        Router_d *r;
	    ThresholdPolicyCongestionSensing100_500_1000_DVFS *parent;
        bool toggle;
        int count;
    };

    static const Tick initialDelay=10000ull;
};

/**
 * Simple policy that changes frequency only of router 5, and senses congestion
 * of that router. It maintains the router freq at normal level with two
 * congestion thresholds trigger the switch to higher freq or lower freq.
 *
 *
 * NOTE: This policy sits on two ideality:
 *	1) does not account for pll dynamic, i.e. both frequency increase and decrease are instantaneous
 *  2) the frequency can be changed at each sample, i.e. every 100ns too fast.
 *
 */
class ThresholdPolicyCongestionSensing100_500_1000 : public DVFSPolicy
{
public:
    Tick high_duty;
    Tick low_duty;
    Tick normal_duty;
	int threshold_high;
	int threshold_low;

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

    
    EventQueue *getQueue() { return eventq; }
    
    ThresholdPolicyCongestionSensing100_500_1000(EventQueue *eventq)
            : DVFSPolicy(eventq)    
	{
		
		high_duty=1000; 	 //1000MHz
	    low_duty=10000ull;	 // 100MHz
    	normal_duty=2000ull; // 500MHz

		threshold_high=20;
		threshold_low=10;
    }

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
        {
			// add an output file per each router managed by this policy
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

            eventq->schedule(new DVFSEvent(router,this),initialDelay);
        }
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, ThresholdPolicyCongestionSensing100_500_1000 *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle), count(count){}
        
        virtual void process()
        {
            //std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
            //r->printFullCongestionData(*(it->second));
			
			std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
			r->printRouterPower(*(it_pwr->second));

			const int samplePeriod=100000; //100000ps=100ns
			const int limit_to_change=10*samplePeriod;

			int actual_congestion=r->getCongestion();
			
			//std::cout<<"@"<<curTick()<<" actual_congestion: "<<actual_congestion<<std::endl;
			//Tick p_old=r->clockPeriod();
            if(count>=limit_to_change)
            {
                count=0;
            	Tick p=0;
				if(actual_congestion >= parent->threshold_high)
					p=parent->high_duty;
				else if( parent->threshold_low <= actual_congestion && actual_congestion < parent->threshold_high)
					p=parent->normal_duty;
				else if(parent->threshold_low > actual_congestion)
					p=parent->low_duty;
				else
					assert(false&&"Error assigning congestion to freq mapping");
			
				assert(p!=0);
				r->changePeriod(p);
			}

            parent->getQueue()->schedule(
                new DVFSEvent(r,parent,toggle,count+samplePeriod),
                curTick()+samplePeriod);
        }
    private:
        Router_d *r;
	    ThresholdPolicyCongestionSensing100_500_1000 *parent;
        bool toggle;
        int count;
    };
    
    static const Tick initialDelay=10000ull;
};

/**
 * EACH FOUR ROUTER A FREQ ISLAND - TO BE USED WITH 16CORES ARCHS
 *
 * Simple policy that changes frequency only of router 5, and senses congestion
 * of that router. It maintains the router freq at normal level with two
 * congestion thresholds trigger the switch to higher freq or lower freq.
 *
 *
 * NOTE: This policy sits on two ideality:
 *	1) does not account for pll dynamic, i.e. both frequency increase and decrease are instantaneous
 *  2) the frequency can be changed at each sample, i.e. every 100ns too fast.
 *
 */
class AllThresholdPolicyCongestionSensing250_500_800Grouped : public DVFSPolicy
{
public:
    Tick high_duty;
    Tick low_duty;
    Tick normal_duty;
	int threshold_high;
	int threshold_low;

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

	std::map<int,FrequencyIsland*> freq;
    
    EventQueue *getQueue() { return eventq; }
    
    AllThresholdPolicyCongestionSensing250_500_800Grouped(EventQueue *eventq)
            : DVFSPolicy(eventq)    
	{
		
		high_duty=1250; 	 //800MHz
	    low_duty=4000ull;	 // 250MHz
    	normal_duty=2000ull; // 500MHz

		threshold_high=20;
		threshold_low=10;		
		
		for(int i=0;i<4;i++)
		{
			freq[i]=new FrequencyIsland();
		}	
    }

    virtual void addRouterImpl(Router_d *router)
    {
		using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
        {
			// add an output file per each router managed by this policy
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

        }
		// CHANGE HERE TO GROUP ROUTERS to 1 out of 4 freq island and generate a
		// pll for each freq island
		if(router->get_id()==0 ||router->get_id()==1 
				||router->get_id()==4 || router->get_id()==5)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_0"<<std::endl;
			(*(freq.find(0)->second)).addObject(router);
			if(router->get_id()==5)
			{
				eventq->schedule(new DVFSEvent(this,/*freq island ptr*/(freq.find(0)->second)),initialDelay);
			}
		}

		if(router->get_id()==2 ||router->get_id()==3 
					||router->get_id()==6 || router->get_id()==7)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_1"<<std::endl;
			(*(freq.find(1)->second)).addObject(router);
			if(router->get_id()==7)
			{
				eventq->schedule(new DVFSEvent(this,/*freq island ptr*/(freq.find(1)->second)),initialDelay);
			}
		}

		if(router->get_id()==8 ||router->get_id()==9 
				||router->get_id()==12 || router->get_id()==13)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_2"<<std::endl;
			(*(freq.find(2)->second)).addObject(router);
			if(router->get_id()==13)
			{
				eventq->schedule(new DVFSEvent(this,/*freq island ptr*/(freq.find(2)->second)),initialDelay);
			}
		}

		if(router->get_id()==10 ||router->get_id()==11 
				||router->get_id()==14 || router->get_id()==15)
		{

			std::cout<<"Added R"<<router->get_id()<< " to FI_3"<<std::endl;
			(*(freq.find(3)->second)).addObject(router);
			if(router->get_id()==15)
			{
				eventq->schedule(new DVFSEvent(this,/*freq island ptr*/(freq.find(3)->second)),initialDelay);
			}
		}

    }
    
private:
    class DVFSEvent : public Event
    {
    public:
		DVFSEvent(AllThresholdPolicyCongestionSensing250_500_800Grouped *parent,
							FrequencyIsland *fi, bool toggle=false, int count=0)
							: Event(Default_Pri, AutoDelete),
				                parent(parent), fi(fi), toggle(toggle), count(count){}

        virtual void process()
        {
			std::vector<PeriodChangeable*>& temp_fi = fi->getObjList();
			int temp_congestion=0;
			assert(temp_fi.size()>0);
			for(int i=0;i<temp_fi.size();i++)
			{
				Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);	
				//std::cout<<"\tFI_"<<fi_id <<" R"<<r->get_id()<<" cong"<<r->getFullCongestion();
				assert(r!=NULL);
				
				temp_congestion+=r->getFullCongestion();

				if(r->get_id()==5||r->get_id()==0)
        		{
        	    	//std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
        	    	//r->printFullCongestionData(*(it->second));
					
					std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
					r->printRouterPower(*(it_pwr->second));
				}
				else
				{
					r->computeRouterPower();
				}
			}
			const int samplePeriod=100000; //100000ps=100ns
			const int limit_to_change=10*samplePeriod;

			int actual_congestion=temp_congestion/temp_fi.size();
			
			//std::cout<<"@"<<curTick()<<" actual_congestion: "<<actual_congestion<<std::endl;
			//Tick p_old=r->clockPeriod();
            if(count>=limit_to_change)
            {
                count=0;
            	Tick p=0;
				if(actual_congestion >= parent->threshold_high)
					p=parent->high_duty;
				else if( parent->threshold_low <= actual_congestion && actual_congestion < parent->threshold_high)
					p=parent->normal_duty;
				else if(parent->threshold_low > actual_congestion)
					p=parent->low_duty;
				else
					assert(false&&"Error assigning congestion to freq mapping");
			
				assert(p!=0);
				// Change frequency to all routers in the frequency island
				for(int i=0;i<temp_fi.size();i++)
				{
					Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);	
					r->changePeriod(p);
				}

			}
            parent->getQueue()->schedule(
                new DVFSEvent(parent, fi,toggle,count+samplePeriod),
                curTick()+samplePeriod);
        }
    private:
        AllThresholdPolicyCongestionSensing250_500_800Grouped *parent;
		FrequencyIsland *fi;
        bool toggle;
        int count;
    };
    
    static const Tick initialDelay=10000ull;
};

/**
 * Simple policy that changes frequency only of router 5, and senses congestion
 * of that router. It maintains the router freq at normal level with two
 * congestion thresholds trigger the switch to higher freq or lower freq.
 *
 *
 * NOTE: This policy sits on two ideality:
 *	1) does not account for pll dynamic, i.e. both frequency increase and decrease are instantaneous
 *  2) the frequency can be changed at each sample, i.e. every 100ns too fast.
 *
 */
class AllThresholdPolicyCongestionSensing250_500_800 : public DVFSPolicy
{
public:
    Tick high_duty;
    Tick low_duty;
    Tick normal_duty;
	int threshold_high;
	int threshold_low;

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

    
    EventQueue *getQueue() { return eventq; }
    
    AllThresholdPolicyCongestionSensing250_500_800(EventQueue *eventq)
            : DVFSPolicy(eventq)    
	{
		
		high_duty=1250; 	 //800MHz
	    low_duty=4000ull;	 // 250MHz
    	normal_duty=2000ull; // 500MHz

		threshold_high=20;
		threshold_low=10;
    }

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
        {
			// add an output file per each router managed by this policy
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

        }

        eventq->schedule(new DVFSEvent(router,this),initialDelay);
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, AllThresholdPolicyCongestionSensing250_500_800 *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle), count(count){}
        
        virtual void process()
        {
			if(r->get_id()==5||r->get_id()==0)
        	{
        	    //std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
        	    //r->printFullCongestionData(*(it->second));
				
				std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
				r->printRouterPower(*(it_pwr->second));
			}
			else
			{
				r->computeRouterPower();
			}
			const int samplePeriod=100000; //100000ps=100ns
			const int limit_to_change=10*samplePeriod;

			int actual_congestion=r->getCongestion();
			
			//std::cout<<"@"<<curTick()<<" actual_congestion: "<<actual_congestion<<std::endl;
			//Tick p_old=r->clockPeriod();
            if(count>=limit_to_change)
            {
                count=0;
            	Tick p=0;
				if(actual_congestion >= parent->threshold_high)
					p=parent->high_duty;
				else if( parent->threshold_low <= actual_congestion && actual_congestion < parent->threshold_high)
					p=parent->normal_duty;
				else if(parent->threshold_low > actual_congestion)
					p=parent->low_duty;
				else
					assert(false&&"Error assigning congestion to freq mapping");
			
				assert(p!=0);
				r->changePeriod(p);
			}

            parent->getQueue()->schedule(
                new DVFSEvent(r,parent,toggle,count+samplePeriod),
                curTick()+samplePeriod);
        }
    private:
        Router_d *r;
        AllThresholdPolicyCongestionSensing250_500_800 *parent;
        bool toggle;
        int count;
    };
    
    static const Tick initialDelay=10000ull;
};
/**
 * Simple policy that changes frequency only of router 5, and senses congestion
 * of that router. It maintains the router freq at normal level with two
 * congestion thresholds trigger the switch to higher freq or lower freq.
 *
 *
 * NOTE: This policy sits on two ideality:
 *	1) does not account for pll dynamic, i.e. both frequency increase and decrease are instantaneous
 *  2) the frequency can be changed at each sample, i.e. every 100ns too fast.
 *
 */
class ThresholdPolicyCongestionSensing250_500_800 : public DVFSPolicy
{
public:
    Tick high_duty;
    Tick low_duty;
    Tick normal_duty;
	int threshold_high;
	int threshold_low;

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

    
    EventQueue *getQueue() { return eventq; }
    
    ThresholdPolicyCongestionSensing250_500_800(EventQueue *eventq)
            : DVFSPolicy(eventq)    
	{
		
		high_duty=1250; 	 //800MHz
	    low_duty=4000ull;	 // 250MHz
    	normal_duty=2000ull; // 500MHz

		threshold_high=20;
		threshold_low=10;
    }

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
        {
			// add an output file per each router managed by this policy
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

            eventq->schedule(new DVFSEvent(router,this),initialDelay);
        }
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, ThresholdPolicyCongestionSensing250_500_800 *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle), count(count){}
        
        virtual void process()
        {
            std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
            r->printFullCongestionData(*(it->second));
			
			std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
			r->printRouterPower(*(it_pwr->second));

			const int samplePeriod=100000; //100000ps=100ns
			const int limit_to_change=10*samplePeriod;

			int actual_congestion=r->getCongestion();
			
			//std::cout<<"@"<<curTick()<<" actual_congestion: "<<actual_congestion<<std::endl;
			//Tick p_old=r->clockPeriod();
            if(count>=limit_to_change)
            {
                count=0;
            	Tick p=0;
				if(actual_congestion >= parent->threshold_high)
					p=parent->high_duty;
				else if( parent->threshold_low <= actual_congestion && actual_congestion < parent->threshold_high)
					p=parent->normal_duty;
				else if(parent->threshold_low > actual_congestion)
					p=parent->low_duty;
				else
					assert(false&&"Error assigning congestion to freq mapping");
			
				assert(p!=0);
				r->changePeriod(p);
			}

            parent->getQueue()->schedule(
                new DVFSEvent(r,parent,toggle,count+samplePeriod),
                curTick()+samplePeriod);
        }
    private:
        Router_d *r;
        ThresholdPolicyCongestionSensing250_500_800 *parent;
        bool toggle;
        int count;
    };
    
    static const Tick initialDelay=10000ull;
};

/**
 * 
 */
class ProportionalControlPolicy : public DVFSPolicy
{
public:


    std::map<int,FrequencyIsland*> freq;
    std::map<int,AdvancedPllModel*> pll;

	
	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;


    EventQueue *getQueue() { return eventq; }
    
    ProportionalControlPolicy(EventQueue *eventq)
       : DVFSPolicy(eventq){}

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        if(router->get_id()==5 || router->get_id()==0)
        {
			// add an output file per each router managed by this policy
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

			freq[router->get_id()]=new FrequencyIsland();
            (*(freq.find(router->get_id())->second)).addObject(router);

			pll[router->get_id()]=new AdvancedPllModel((freq.find(router->get_id())->second),eventq);
            router->setVoltageRegulator(pll[router->get_id()]->getVoltageRegulator());

            eventq->schedule(new DVFSEvent(this,router),initialDelay);
        } else router->setVoltageRegulator(new DummyVoltageRegulator);
    }
    
private:

    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(ProportionalControlPolicy *parent, Router_d *router) : 
									Event(Default_Pri, AutoDelete),
									parent(parent),r(router){}
        
        virtual void process()
        {
			int actual_congestion=r->getFullCongestion();

			std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
            		r->printFullCongestionData(*(it->second));
			
			std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
			r->printRouterPower(*(it_pwr->second));

           
		    // proportional control policy
		   	const double k=0.035; //Obtained from closed loop control with identified model
			const double min_freq=1e8; //min frequency 100MHz
			const double max_freq=1e9; //max frequency 1GHz	
			assert(actual_congestion >= 0);
			const double scaleFactor=1e9; //Identification was done with freq in GHz
		   	double u = std::min(max_freq,min_freq+k*actual_congestion*scaleFactor);

			//parent->pll.changePeriod(1.0/u*SimClock::Float::s);
			std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(r->get_id());
			(*(it_pll->second)).changePeriod(1.0/u*SimClock::Float::s);

			///////////////////////////////////////
			
			const int samplePeriod=1.0/1e7*SimClock::Float::s; //100000ps=100ns
	
            parent->getQueue()->schedule(new DVFSEvent(parent,r), curTick()+samplePeriod);
        }
    private:
        ProportionalControlPolicy *parent;
		Router_d *r;
    };
    
    static const Tick initialDelay=10000ull;
};

/**
 * EACH FOUR ROUTER A FREQ ISLAND - TO BE USED WITH 16CORES ARCHS
 * This is a proportional contrl policy were the controller has an additional
 * pole to smooth the applied u signal. The pole can be configured to be <1
 * otherwise, we have an integrator in the controller. Such integrator makes the
 * system saturated since all the quantities we are dealing with are positive.
 */


class AllClassicalControlPolicyGrouped : public DVFSPolicy
{
public:
    std::map<int,FrequencyIsland*> freq;
    std::map<int,AdvancedPllModel*> pll;

	
	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

	std::ifstream k_file;
	double k;

// START - this is for stability analysis only (NEVER USED IN SIMULATION)
	std::ofstream u_unconstr_file;
// END - this is for stability analysis only (NEVER USED IN SIMULATION)

    EventQueue *getQueue() { return eventq; }
    
    AllClassicalControlPolicyGrouped(EventQueue *eventq)
				: DVFSPolicy(eventq),k_file("k.txt"), u_unconstr_file("u_unconstr_file.txt")
    {
		k=0;
		assert(k_file.is_open());
		k_file>>k;
		assert(k!=0);

		for(int i=0;i<4;i++)
		{
			freq[i]=new FrequencyIsland();
			pll[i]=new AdvancedPllModel((freq.find(i)->second),eventq);
		}
	}

    virtual void addRouterImpl(Router_d *router)
    {
 		//	GET STATS FOR R5 AND R0
        using namespace std;
        if(router->get_id()==5 || router->get_id()==0)
        {// add an output file per each router managed by this policy

			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());
			
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";
			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

		}

		// CHANGE HERE TO GROUP ROUTERS to 1 out of 4 freq island and generate a
		// pll for each freq island
		if(router->get_id()==0 ||router->get_id()==1 
				||router->get_id()==4 || router->get_id()==5)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_0"<<std::endl;
			(*(freq.find(0)->second)).addObject(router);
			router->setVoltageRegulator(pll[0]->getVoltageRegulator());

			if(router->get_id()==5)
				eventq->schedule(new DVFSEvent(this,1e8 /*initial u*/, /*freq island ptr*/(freq.find(0)->second),0,1e8 /*initial u_unconstr*/),initialDelay);

			//freq[router->get_id()]=new FrequencyIsland();
        	//(*(freq.find(router->get_id())->second)).addObject(router);
			//pll[router->get_id()]=new AdvancedPllModel
			//		((freq.find(router->get_id())->second),eventq);
        	//eventq->schedule(new DVFSEvent
			//		(this,1e8 /*initial u*/,router,1e8 /*initial u_unconstr*/),initialDelay);
	
		}

		if(router->get_id()==2 ||router->get_id()==3 
					||router->get_id()==6 || router->get_id()==7)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_1"<<std::endl;
			(*(freq.find(1)->second)).addObject(router);
			router->setVoltageRegulator(pll[1]->getVoltageRegulator());
			if(router->get_id()==7)
			{
				eventq->schedule(new DVFSEvent(this,1e8 /*initial u*/, /*freq island ptr*/(freq.find(1)->second),1,1e8 /*initial u_unconstr*/),initialDelay);
			}
		}

		if(router->get_id()==8 ||router->get_id()==9 
				||router->get_id()==12 || router->get_id()==13)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_2"<<std::endl;
			(*(freq.find(2)->second)).addObject(router);
			router->setVoltageRegulator(pll[2]->getVoltageRegulator());
			if(router->get_id()==13)
			{
				eventq->schedule(new DVFSEvent(this,1e8 /*initial u*/, /*freq island ptr*/(freq.find(2)->second),2,1e8 /*initial u_unconstr*/),initialDelay);
			}
		}

		if(router->get_id()==10 ||router->get_id()==11 
				||router->get_id()==14 || router->get_id()==15)
		{

			std::cout<<"Added R"<<router->get_id()<< " to FI_3"<<std::endl;
			(*(freq.find(3)->second)).addObject(router);
			router->setVoltageRegulator(pll[3]->getVoltageRegulator());
			if(router->get_id()==15)
			{
				eventq->schedule(new DVFSEvent(this,1e8 /*initial u*/, /*freq island ptr*/(freq.find(3)->second),3,1e8 /*initial u_unconstr*/),initialDelay);
			}
		}


    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(AllClassicalControlPolicyGrouped *parent, double u_old, FrequencyIsland* fi,int fi_id,double u_old_unconstr) : 
					Event(Default_Pri, AutoDelete),
					parent(parent),u_old(u_old),fi(fi),fi_id(fi_id),u_old_unconstr(u_old_unconstr){}
        
        virtual void process()
        {
			// average the congestion on the router of the fi
			std::vector<PeriodChangeable*>& temp_fi = fi->getObjList();
			int temp_congestion=0;
			assert(temp_fi.size()>0);
			for(int i=0;i<temp_fi.size();i++)
			{
				Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);	
				//std::cout<<"\tFI_"<<fi_id <<" R"<<r->get_id()<<" cong"<<r->getFullCongestion();
				assert(r!=NULL);
				
				temp_congestion+=r->getFullCongestion();
				if(r->get_id()==5 || r->get_id()==0)
				{
					std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
					r->printRouterPower(*(it_pwr->second));
				}
				else
				{
					r->computeRouterPower();
				}
			}


		 	int actual_congestion=temp_congestion/temp_fi.size();

		    // Classical control policy
			double k=parent->k;
		   	//const double k=1; //Obtained from closed loop control with identified model
			const double u_old_factor=0.99; // place a pole in the controller to	smooth its actuation
			const double min_freq=1e8; //min frequency 100MHz
			const double max_freq=1e9; //max frequency 1GHz	
			assert(actual_congestion >= 0);
			const double scaleFactor=1e9; //Identification was done with freq in GHz
		
		// START - this is for stability analysis only (NEVER USED IN SIMULATION)
			double u_unconstr1=min_freq+k*actual_congestion*scaleFactor;
			double u_unconstr=u_old_factor*u_old_unconstr+(1-u_old_factor)*u_unconstr1;
			//double u_unconstr=std::min(max_freq,u_unconstr);
			//parent->u_unconstr_file<<u_unconstr1<<" "<<u_unconstr<<std::endl;

		// END - this is for stability analysis only (NEVER USED IN SIMULATION)

		   	double u = std::min(max_freq,min_freq+k*actual_congestion*scaleFactor);
			u=u_old_factor*u_old+(1-u_old_factor)*u;

			//parent->pll.changePeriod(1.0/u*SimClock::Float::s);
			assert(fi_id>=0 &&fi_id<=3);
			std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(fi_id);
			//(*(it_pll->second)).changePeriod(1.0/u*SimClock::Float::s);
			assert(it_pll->second!=NULL);
			(*(it_pll->second)).changePeriod((int)(1.0/u_unconstr*SimClock::Float::s));
			///////////////////////////////////////
			
			const int samplePeriod=1.0/1e7*SimClock::Float::s; //100000ps=100ns
			//std::cout<<" actual_congestion "<<actual_congestion<<" u "<< u<<std::endl;

            parent->getQueue()->schedule(new DVFSEvent(parent,u,fi,fi_id,u_unconstr), curTick()+samplePeriod);
        }
    private:
        AllClassicalControlPolicyGrouped *parent;
		double u_old;
		FrequencyIsland *fi;
		int fi_id;
		// for stability analysis DISABLE IF NOT USED
		double u_old_unconstr;

    };
    
    static const Tick initialDelay=10000ull;
};


/**
 * EACH ROUTER ITS OWN FREQUENCY ISLAND
 * This is a proportional contrl policy were the controller has an additional
 * pole to smooth the applied u signal. The pole can be configured to be <1
 * otherwise, we have an integrator in the controller. Such integrator makes the
 * system saturated since all the quantities we are dealing with are positive.
 */


class AllClassicalControlPolicy : public DVFSPolicy
{
public:
    std::map<int,FrequencyIsland*> freq;
    std::map<int,AdvancedPllModel*> pll;

	
	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

	std::ifstream k_file;
	double k;

// START - this is for stability analysis only (NEVER USED IN SIMULATION)
	std::ofstream u_unconstr_file;
// END - this is for stability analysis only (NEVER USED IN SIMULATION)

    EventQueue *getQueue() { return eventq; }
    
    AllClassicalControlPolicy(EventQueue *eventq)
				: DVFSPolicy(eventq),k_file("k.txt"), u_unconstr_file("u_unconstr_file.txt")
    {
		k=0;
		assert(k_file.is_open());
		k_file>>k;
		assert(k!=0);
	}

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        if(router->get_id()==5 || router->get_id()==0)
        {// add an output file per each router managed by this policy

			//std::stringstream str;
			//str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			//dump_f[router->get_id()]=new ofstream(str.str());
			//assert((*dump_f[router->get_id()]).is_open());
			
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";
			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());
		}
		freq[router->get_id()]=new FrequencyIsland();
        (*(freq.find(router->get_id())->second)).addObject(router);

		pll[router->get_id()]=new AdvancedPllModel((freq.find(router->get_id())->second),eventq);
        router->setVoltageRegulator(pll[router->get_id()]->getVoltageRegulator());
        
        eventq->schedule(new DVFSEvent(this,1e8 /*initial u*/, router,1e8 /*initial u_unconstr*/),initialDelay);
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(AllClassicalControlPolicy *parent, double u_old, Router_d* router,double u_old_unconstr) : 
									Event(Default_Pri, AutoDelete),
									parent(parent),u_old(u_old),r(router),u_old_unconstr(u_old_unconstr){}
        
        virtual void process()
        {
		 	int actual_congestion=r->getFullCongestion();

			//std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
            //r->printFullCongestionData(*(it->second));
			if(r->get_id()==5 || r->get_id()==0)
			{
				std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
				r->printRouterPower(*(it_pwr->second));
			}
			else
			{
				r->computeRouterPower();
			}
		    // Classical control policy
			double k=parent->k;
		   	//const double k=1; //Obtained from closed loop control with identified model
			const double u_old_factor=0.99; // place a pole in the controller to	smooth its actuation
			const double min_freq=1e8; //min frequency 100MHz
			const double max_freq=1e9; //max frequency 1GHz	
			assert(actual_congestion >= 0);
			const double scaleFactor=1e9; //Identification was done with freq in GHz
		
		// START - this is for stability analysis only (NEVER USED IN SIMULATION)
			double u_unconstr1=min_freq+k*actual_congestion*scaleFactor;
			double u_unconstr=u_old_factor*u_old_unconstr+(1-u_old_factor)*u_unconstr1;
			//double u_unconstr=std::min(max_freq,u_unconstr);
			//parent->u_unconstr_file<<u_unconstr1<<" "<<u_unconstr<<std::endl;

		// END - this is for stability analysis only (NEVER USED IN SIMULATION)

		   	double u = std::min(max_freq,min_freq+k*actual_congestion*scaleFactor);
			u=u_old_factor*u_old+(1-u_old_factor)*u;

			//parent->pll.changePeriod(1.0/u*SimClock::Float::s);
			std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(r->get_id());
			//(*(it_pll->second)).changePeriod(1.0/u*SimClock::Float::s);
			(*(it_pll->second)).changePeriod((int)(1.0/u_unconstr*SimClock::Float::s));

			///////////////////////////////////////
			
			const int samplePeriod=1.0/1e7*SimClock::Float::s; //100000ps=100ns
	
            parent->getQueue()->schedule(new DVFSEvent(parent,u,r,u_unconstr), curTick()+samplePeriod);
        }
    private:
        AllClassicalControlPolicy *parent;
		double u_old;
		Router_d *r;

		// for stability analysis DISABLE IF NOT USED
		double u_old_unconstr;

    };
    
    static const Tick initialDelay=10000ull;
};


/**
 * This is a proportional contrl policy were the controller has an additional
 * pole to smooth the applied u signal. The pole can be configured to be <1
 * otherwise, we have an integrator in the controller. Such integrator makes the
 * system saturated since all the quantities we are dealing with are positive.
 * NOTE: 
 *	1) works on all routers gruping them in 4 2x2 macrotiles for 16cores only
 *  2) DVFS implementation through the simple policy to regulate voltage
 *  depending on the required frequency 
 */


class AllClassicalControlPolicyDVFS_Grouped : public DVFSPolicy
{
public:
    std::map<int,FrequencyIsland*> freq;
    std::map<int,AdvancedPllModel*> pll;

	
	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;	

	std::ifstream k_file;
	double k;

	Tick voltageTransientPeriod;

// START - this is for stability analysis only (NEVER USED IN SIMULATION)
	std::ofstream u_unconstr_file;
// END - this is for stability analysis only (NEVER USED IN SIMULATION)

    EventQueue *getQueue() { return eventq; }
    
    AllClassicalControlPolicyDVFS_Grouped(EventQueue *eventq)
				: DVFSPolicy(eventq),k_file("k.txt"), u_unconstr_file("u_unconstr_file.txt")
    {
		k=0;
		assert(k_file.is_open());		
		k_file>>k;
		assert(k!=0);
		voltageTransientPeriod=5000000; // 5us

		for(int i=0;i<4;i++)
		{
			freq[i]=new FrequencyIsland();
			pll[i]=new AdvancedPllModel((freq.find(i)->second),eventq);

		}	
	}

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        if(router->get_id()==5 || router->get_id()==0)
        {// add an output file per each router managed by this policy

			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";
			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());
		}
			// CHANGE HERE TO GROUP ROUTERS to 1 out of 4 freq island and generate a
		// pll for each freq island
		if(router->get_id()==0 ||router->get_id()==1 
				||router->get_id()==4 || router->get_id()==5)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_0"<<std::endl;
			(*(freq.find(0)->second)).addObject(router);
			router->setVoltageRegulator(pll[0]->getVoltageRegulator());
			if(router->get_id()==5)
				eventq->schedule(new FirstDVFSEvent(this,1e8 /*initial u*/, /*freq island ptr*/(freq.find(0)->second),0,1.0 /*initial u_unconstr*/),initialDelay);

		}
		if(router->get_id()==2 ||router->get_id()==3 ||router->get_id()==6 || router->get_id()==7)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_1"<<std::endl;
			(*(freq.find(1)->second)).addObject(router);
			router->setVoltageRegulator(pll[1]->getVoltageRegulator());
			if(router->get_id()==7)
			{
				eventq->schedule(new FirstDVFSEvent(this,1e8 /*initial u*/, /*freq island ptr*/(freq.find(1)->second),1,1.0 /*initial u_unconstr*/),initialDelay);
			}
		}
		
		if(router->get_id()==8 ||router->get_id()==9 ||router->get_id()==12 || router->get_id()==13)
		{
			std::cout<<"Added R"<<router->get_id()<< " to FI_2"<<std::endl;
			(*(freq.find(2)->second)).addObject(router);
			router->setVoltageRegulator(pll[2]->getVoltageRegulator());
			if(router->get_id()==13)
			{
				eventq->schedule(new FirstDVFSEvent(this,1e8 /*initial u*/, /*freq island ptr*/(freq.find(2)->second),2,1.0 /*initial u_unconstr*/),initialDelay);
			}
		}

		if(router->get_id()==10 ||router->get_id()==11 ||router->get_id()==14 || router->get_id()==15)
		{

			std::cout<<"Added R"<<router->get_id()<< " to FI_3"<<std::endl;
			(*(freq.find(3)->second)).addObject(router);
			router->setVoltageRegulator(pll[3]->getVoltageRegulator());
			if(router->get_id()==15)
			{
				eventq->schedule(new FirstDVFSEvent(this,1e8 /*initial u*/, /*freq island ptr*/(freq.find(3)->second),3,1.0 /*initial u_unconstr*/),initialDelay);
			}
		}

    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(AllClassicalControlPolicyDVFS_Grouped *parent, double u_old,
FrequencyIsland* fi,int fi_id,double actualVdd) : 
									Event(Default_Pri, AutoDelete),
parent(parent),u_old(u_old),fi(fi),fi_id(fi_id),actualVdd(actualVdd) {}
        
        virtual void process()
        {

			#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<"################ ->>> @"<<curTick()<<" FI"<< fi_id <<" change freq after a voltage scale up "<<r->clockPeriod()<<" -> "<< new_period  <<std::endl;
			#endif
			// if we arrived here we paid a voltage change
	   		Tick new_period = (unsigned long long)(1.0/u_old*SimClock::Float::s);
			// HERE I must updated the previously computed frequency, only and
			assert(fi_id>=0 &&fi_id<=3);
			std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(fi_id);
			assert(it_pll->second!=NULL);
			(*(it_pll->second)).changePeriod(new_period);

//			std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(r->get_id());
//			(*(it_pll->second)).changePeriod(new_period);
			
            parent->getQueue()->schedule(new FirstDVFSEvent(parent,u_old,fi,fi_id,actualVdd), curTick());
        }
    private:
        AllClassicalControlPolicyDVFS_Grouped *parent;
		Tick new_period;
		double u_old;
		FrequencyIsland *fi;
		int fi_id;
		// for stability analysis DISABLE IF NOT USED

		double actualVdd;

    };
	/*this event is to regulate the voltage*/
    class FirstDVFSEvent : public Event
    {
    public:
		FirstDVFSEvent(AllClassicalControlPolicyDVFS_Grouped *parent, double
u_old, FrequencyIsland* fi,int fi_id, double actualVdd): 
									Event(Default_Pri, AutoDelete), parent(parent),
									u_old(u_old),fi(fi),fi_id(fi_id), actualVdd(actualVdd) {}
        
        virtual void process()
        {
			std::vector<PeriodChangeable*>& temp_fi = fi->getObjList();
			int temp_congestion=0;
			assert(temp_fi.size()>0);
			// update power evaluation
			for(int i=0;i<temp_fi.size();i++)
			{
				Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);	
				assert(r!=NULL);
				temp_congestion+=r->getFullCongestion();
				if(r->get_id()==5 || r->get_id()==0)
				{
					std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
					r->printRouterPower(*(it_pwr->second),actualVdd);
				}
				else
				{
					r->computeRouterPower(actualVdd); 
				}
			}


			int actual_congestion=temp_congestion/temp_fi.size();
          
		    // Classical control policy
			double k=parent->k;
		   	//const double k=1; //Obtained from closed loop control with identified model
			const double u_old_factor=0.99; // place a pole in the controller to	smooth its actuation
			const double min_freq=1e8; //min frequency 100MHz
			const double max_freq=1e9; //max frequency 1GHz	
			assert(actual_congestion >= 0);
			const double scaleFactor=1e9; //Identification was done with freq in GHz
		
		   	double u = std::min(max_freq,min_freq+k*actual_congestion*scaleFactor);
			u=u_old_factor*u_old+(1-u_old_factor)*u;

			Tick new_period = (unsigned long long)(1.0/u*SimClock::Float::s);
		// HERE I know the new frequency to be actuated, then I have to decide
		// the dealy to impose due to a possible voltage increase.

			double new_vdd=parent->frequencyToVoltage(new_period /*the required period*/);
			#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<"@"<<curTick()<<" VoltageEvent on R"<< r->get_id() <<" : ["
					<< r->clockPeriod()<<" -> "<< new_period <<"] ["
					<< actualVdd<< " -> "<< new_vdd<< "]"<<std::endl;
			#endif



			if(new_vdd>actualVdd)
			//if(r->clockPeriod()>new_period)
			{
				#if PRINT_FREQ_DVFS_ROUTERS == 1
				std::cout<<"\t waiting for VDD increase"<<std::endl;
				#endif
				parent->getQueue()->schedule(new DVFSEvent(parent,u,fi,fi_id,new_vdd), curTick()+parent->voltageTransientPeriod);
			}
			else
			{// immediately start a frequency change and reschedule the control law event
				#if PRINT_FREQ_DVFS_ROUTERS == 1
				std::cout<<"\t change the frequency now..."<<std::endl;
				#endif
				assert(fi_id>=0 &&fi_id<=3);
				std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(fi_id);
				assert(it_pll->second!=NULL);
				(*(it_pll->second)).changePeriod(new_period);

				const int samplePeriod=100000;//1.0/1e7*SimClock::Float::s; //100000ps=100ns
				parent->getQueue()->schedule(new FirstDVFSEvent(parent,u,fi,fi_id,new_vdd), curTick()+samplePeriod);
			}
        }
    private:
        AllClassicalControlPolicyDVFS_Grouped *parent;
		double u_old;
		FrequencyIsland *fi;
		int fi_id;
		double actualVdd;
    };

    static const Tick initialDelay=10000ull;
};

/**
 * This is a proportional contrl policy were the controller has an additional
 * pole to smooth the applied u signal. The pole can be configured to be <1
 * otherwise, we have an integrator in the controller. Such integrator makes the
 * system saturated since all the quantities we are dealing with are positive.
 * NOTE: 
 *	1) works on all routers
 *  2) DVFS implementation through the simple policy to regulate voltage
 *  depending on the required frequency 
 */


class AllClassicalControlPolicyDVFS : public DVFSPolicy
{
public:
    std::map<int,FrequencyIsland*> freq;
    std::map<int,AdvancedPllModel*> pll;

	
	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

	std::ifstream k_file;
	double k;

	Tick voltageTransientPeriod;

// START - this is for stability analysis only (NEVER USED IN SIMULATION)
	std::ofstream u_unconstr_file;
// END - this is for stability analysis only (NEVER USED IN SIMULATION)

    EventQueue *getQueue() { return eventq; }
    
    AllClassicalControlPolicyDVFS(EventQueue *eventq)
				: DVFSPolicy(eventq),k_file("k.txt"), u_unconstr_file("u_unconstr_file.txt")
    {
		k=0;
		assert(k_file.is_open());		
		k_file>>k;
		assert(k!=0);
		voltageTransientPeriod=5000000; // 5us
	}

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        if(router->get_id()==5 || router->get_id()==0)
        {// add an output file per each router managed by this policy

			//std::stringstream str;
			//str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			//dump_f[router->get_id()]=new ofstream(str.str());
			//assert((*dump_f[router->get_id()]).is_open());

			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";
			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());
		}
		freq[router->get_id()]=new FrequencyIsland();
        (*(freq.find(router->get_id())->second)).addObject(router);

		pll[router->get_id()]=new AdvancedPllModel((freq.find(router->get_id())->second),eventq);
        router->setVoltageRegulator(pll[router->get_id()]->getVoltageRegulator());

        eventq->schedule(new FirstDVFSEvent(this,1e8 /*initial u*/, router,1.0/*initial Vdd*/),initialDelay);
        
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(AllClassicalControlPolicyDVFS *parent, double u_old, Router_d* router,double actualVdd) : 
									Event(Default_Pri, AutoDelete), parent(parent),u_old(u_old),r(router),actualVdd(actualVdd) {}
        
        virtual void process()
        {
			#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<"################ ->>> @"<<curTick()<<" R"<< r->get_id() <<" change freq after a voltage scale up "<<r->clockPeriod()<<" -> "<< new_period  <<std::endl;
			#endif
			// if we arrived here we paid a voltage change
	   		Tick new_period = (int)(1.0/u_old*SimClock::Float::s);
			// HERE I must updated the previously computed frequency, only and

			std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(r->get_id());
			(*(it_pll->second)).changePeriod(new_period);
			
            parent->getQueue()->schedule(new FirstDVFSEvent(parent,u_old,r,actualVdd), curTick());
        }
    private:
        AllClassicalControlPolicyDVFS *parent;
		Tick new_period;
		double u_old;
		Router_d *r;

		// for stability analysis DISABLE IF NOT USED

		double actualVdd;

    };
	/*this event is to regulate the voltage*/
    class FirstDVFSEvent : public Event
    {
    public:
		FirstDVFSEvent(AllClassicalControlPolicyDVFS *parent, double u_old, Router_d* router, double actualVdd): 
									Event(Default_Pri, AutoDelete), parent(parent),
									u_old(u_old),r(router), actualVdd(actualVdd) {}
        
        virtual void process()
        {
		//	#if PRINT_FREQ_DVFS_ROUTERS == 1
		//	std::cout<<"@"<<curTick()<<" VoltageEvent on R"<< r->get_id()<<std::endl;
		//	#endif

			int actual_congestion=r->getFullCongestion();

           
		    // Classical control policy
			double k=parent->k;
		   	//const double k=1; //Obtained from closed loop control with identified model
			const double u_old_factor=0.99; // place a pole in the controller to	smooth its actuation
			const double min_freq=1e8; //min frequency 100MHz
			const double max_freq=1e9; //max frequency 1GHz	
			assert(actual_congestion >= 0);
			const double scaleFactor=1e9; //Identification was done with freq in GHz
		
		   	double u = std::min(max_freq,min_freq+k*actual_congestion*scaleFactor);
			u=u_old_factor*u_old+(1-u_old_factor)*u;
			Tick new_period = (int)(1.0/u*SimClock::Float::s);
		// HERE I know the new frequency to be actuated, then I have to decide
		// the dealy to impose due to a possible voltage increase.

			double new_vdd=parent->frequencyToVoltage(new_period /*the required period*/);
			#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<"@"<<curTick()<<" VoltageEvent on R"<< r->get_id() <<" : ["
					<< r->clockPeriod()<<" -> "<< new_period <<"] ["
					<< actualVdd<< " -> "<< new_vdd<< "]"<<std::endl;
			#endif
			if(r->get_id()==5 || r->get_id()==0)
			{
				std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
				r->printRouterPower(*(it_pwr->second),actualVdd); //use the old vdd to compute power and update it 
			}
			else
			{
				r->computeRouterPower(actualVdd);
			}

			if(new_vdd>actualVdd)
			//if(r->clockPeriod()>new_period)
			{
				#if PRINT_FREQ_DVFS_ROUTERS == 1
				std::cout<<"\t waiting for VDD increase"<<std::endl;
				#endif
				parent->getQueue()->schedule(new DVFSEvent(parent,u,r,new_vdd), curTick()+parent->voltageTransientPeriod);
			}
			else
			{// immediately start a frequency change and reschedule the control law event
				#if PRINT_FREQ_DVFS_ROUTERS == 1
				std::cout<<"\t change the frequency now..."<<std::endl;
				#endif
				std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(r->get_id());
				(*(it_pll->second)).changePeriod(new_period);
				const int samplePeriod=100000;//1.0/1e7*SimClock::Float::s; //100000ps=100ns
				parent->getQueue()->schedule(new FirstDVFSEvent(parent,u,r,new_vdd), curTick()+samplePeriod);
			}
        }
    private:
        AllClassicalControlPolicyDVFS *parent;
		double u_old;
		Router_d *r;
		double actualVdd;
    };

    static const Tick initialDelay=10000ull;
};


/**
 * This is a proportional contrl policy were the controller has an additional
 * pole to smooth the applied u signal. The pole can be configured to be <1
 * otherwise, we have an integrator in the controller. Such integrator makes the
 * system saturated since all the quantities we are dealing with are positive.
 * NOTE: 
 *	1) works now tiled on a single router only, i.e. R5 in the current
 * implementation
 *  2) DVFS implementation through the simple policy to regulate voltage
 *  depending on the required frequency 
 */


class ClassicalControlPolicyDVFS : public DVFSPolicy
{
public:
    std::map<int,FrequencyIsland*> freq;
    std::map<int,AdvancedPllModel*> pll;

	
	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

	std::ifstream k_file;
	double k;

	Tick voltageTransientPeriod;

// START - this is for stability analysis only (NEVER USED IN SIMULATION)
	std::ofstream u_unconstr_file;
// END - this is for stability analysis only (NEVER USED IN SIMULATION)

    EventQueue *getQueue() { return eventq; }
    
    ClassicalControlPolicyDVFS(EventQueue *eventq)
				: DVFSPolicy(eventq),k_file("k.txt"), u_unconstr_file("u_unconstr_file.txt")
    {
		k=0;
		assert(k_file.is_open());		
		k_file>>k;
		assert(k!=0);
		voltageTransientPeriod=5000000; // 5us
	}

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        if(router->get_id()==5 )//|| router->get_id()==0)
        {// add an output file per each router managed by this policy

			//std::stringstream str;
			//str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			//dump_f[router->get_id()]=new ofstream(str.str());
			//assert((*dump_f[router->get_id()]).is_open());

			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";
			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

			freq[router->get_id()]=new FrequencyIsland();
            (*(freq.find(router->get_id())->second)).addObject(router);

			pll[router->get_id()]=new AdvancedPllModel((freq.find(router->get_id())->second),eventq);
            router->setVoltageRegulator(pll[router->get_id()]->getVoltageRegulator());

            eventq->schedule(new FirstDVFSEvent(this,1e8 /*initial u*/, router,1.0/*initial Vdd*/),initialDelay);
        } else router->setVoltageRegulator(new DummyVoltageRegulator);
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(ClassicalControlPolicyDVFS *parent, double u_old, Router_d* router,double actualVdd) : 
									Event(Default_Pri, AutoDelete), parent(parent),u_old(u_old),r(router),actualVdd(actualVdd) {}
        
        virtual void process()
        {
			#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<"################ ->>> @"<<curTick()<<" R"<< r->get_id() <<" change freq after a voltage scale up "<<r->clockPeriod()<<" -> "<< new_period  <<std::endl;
			#endif
			// if we arrived here we paid a voltage change
	   		Tick new_period = (int)(1.0/u_old*SimClock::Float::s);
			// HERE I must updated the previously computed frequency, only and

			std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(r->get_id());
			(*(it_pll->second)).changePeriod(new_period);
			
            parent->getQueue()->schedule(new FirstDVFSEvent(parent,u_old,r,actualVdd), curTick());
        }
    private:
        ClassicalControlPolicyDVFS *parent;
		Tick new_period;
		double u_old;
		Router_d *r;

		// for stability analysis DISABLE IF NOT USED

		double actualVdd;

    };
	/*this event is to regulate the voltage*/
    class FirstDVFSEvent : public Event
    {
    public:
		FirstDVFSEvent(ClassicalControlPolicyDVFS *parent, double u_old, Router_d* router, double actualVdd): 
									Event(Default_Pri, AutoDelete), parent(parent),
									u_old(u_old),r(router), actualVdd(actualVdd) {}
        
        virtual void process()
        {
		//	#if PRINT_FREQ_DVFS_ROUTERS == 1
		//	std::cout<<"@"<<curTick()<<" VoltageEvent on R"<< r->get_id()<<std::endl;
		//	#endif

			int actual_congestion=r->getFullCongestion();

           
		    // Classical control policy
			double k=parent->k;
		   	//const double k=1; //Obtained from closed loop control with identified model
			const double u_old_factor=0.99; // place a pole in the controller to	smooth its actuation
			const double min_freq=1e8; //min frequency 100MHz
			const double max_freq=1e9; //max frequency 1GHz	
			assert(actual_congestion >= 0);
			const double scaleFactor=1e9; //Identification was done with freq in GHz
		
		   	double u = std::min(max_freq,min_freq+k*actual_congestion*scaleFactor);
			u=u_old_factor*u_old+(1-u_old_factor)*u;
			Tick new_period = (int)(1.0/u*SimClock::Float::s);
		// HERE I know the new frequency to be actuated, then I have to decide
		// the dealy to impose due to a possible voltage increase.

			double new_vdd=parent->frequencyToVoltage(new_period /*the required period*/);
			#if PRINT_FREQ_DVFS_ROUTERS == 1
			std::cout<<"@"<<curTick()<<" VoltageEvent on R"<< r->get_id() <<" : ["
					<< r->clockPeriod()<<" -> "<< new_period <<"] ["
					<< actualVdd<< " -> "<< new_vdd<< "]"<<std::endl;
			#endif
			std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());

			r->printRouterPower(*(it_pwr->second),actualVdd); //use the old vdd to compute power and update it 

			if(new_vdd>actualVdd)
			//if(r->clockPeriod()>new_period)
			{
				#if PRINT_FREQ_DVFS_ROUTERS == 1
				std::cout<<"\t waiting for VDD increase"<<std::endl;
				#endif
				parent->getQueue()->schedule(new DVFSEvent(parent,u,r,new_vdd), curTick()+parent->voltageTransientPeriod);
			}
			else
			{// immediately start a frequency change and reschedule the control law event
				#if PRINT_FREQ_DVFS_ROUTERS == 1
				std::cout<<"\t change the frequency now..."<<std::endl;
				#endif
				std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(r->get_id());
				(*(it_pll->second)).changePeriod(new_period);
				const int samplePeriod=100000;//1.0/1e7*SimClock::Float::s; //100000ps=100ns
				parent->getQueue()->schedule(new FirstDVFSEvent(parent,u,r,new_vdd), curTick()+samplePeriod);
			}
        }
    private:
        ClassicalControlPolicyDVFS *parent;
		double u_old;
		Router_d *r;
		double actualVdd;
    };

    static const Tick initialDelay=10000ull;
};

/**
 * This is a proportional contrl policy were the controller has an additional
 * pole to smooth the applied u signal. The pole can be configured to be <1
 * otherwise, we have an integrator in the controller. Such integrator makes the
 * system saturated since all the quantities we are dealing with are positive.
 * It is now working on a single router only, i.e. R5 in the implementation
 */


class ClassicalControlPolicy : public DVFSPolicy
{
public:
    std::map<int,FrequencyIsland*> freq;
    std::map<int,AdvancedPllModel*> pll;

	
	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

	std::ifstream k_file;
	double k;

// START - this is for stability analysis only (NEVER USED IN SIMULATION)
	std::ofstream u_unconstr_file;
// END - this is for stability analysis only (NEVER USED IN SIMULATION)

    EventQueue *getQueue() { return eventq; }
    
    ClassicalControlPolicy(EventQueue *eventq)
				: DVFSPolicy(eventq),k_file("k.txt"), u_unconstr_file("u_unconstr_file.txt")
    {
		k=0;
		assert(k_file.is_open());
		k_file>>k;
		assert(k!=0);
	}

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        if(router->get_id()==5 )//|| router->get_id()==0)
        {// add an output file per each router managed by this policy

			//std::stringstream str;
			//str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			//dump_f[router->get_id()]=new ofstream(str.str());
			//assert((*dump_f[router->get_id()]).is_open());
			
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";
			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

			freq[router->get_id()]=new FrequencyIsland();
            (*(freq.find(router->get_id())->second)).addObject(router);

			pll[router->get_id()]=new AdvancedPllModel((freq.find(router->get_id())->second),eventq);
            router->setVoltageRegulator(pll[router->get_id()]->getVoltageRegulator());

            eventq->schedule(new DVFSEvent(this,1e8 /*initial u*/, router),initialDelay);
        } else router->setVoltageRegulator(new DummyVoltageRegulator);
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(ClassicalControlPolicy *parent, double u_old, Router_d* router) : 
									Event(Default_Pri, AutoDelete),
									parent(parent),u_old(u_old),r(router){}
        
        virtual void process()
        {
		 	int actual_congestion=r->getFullCongestion();

			//std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
            //r->printFullCongestionData(*(it->second));
			
			std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
			r->printRouterPower(*(it_pwr->second));

           
		    // Classical control policy
			double k=parent->k;
		   	//const double k=1; //Obtained from closed loop control with identified model
			const double u_old_factor=0.99; // place a pole in the controller to	smooth its actuation
			const double min_freq=1e8; //min frequency 100MHz
			const double max_freq=1e9; //max frequency 1GHz	
			assert(actual_congestion >= 0);
			const double scaleFactor=1e9; //Identification was done with freq in GHz
		
		// START - this is for stability analysis only (NEVER USED IN SIMULATION)
			//double u_unconstr1=min_freq+k*actual_congestion*scaleFactor;
			//double u_unconstr=u_old_factor*u_old_unconstr+(1-u_old_factor)*u_unconstr1;
			//double u_unconstr=std::min(max_freq,u_unconstr);
			//parent->u_unconstr_file<<u_unconstr1<<" "<<u_unconstr<<std::endl;

		// END - this is for stability analysis only (NEVER USED IN SIMULATION)

		   	double u = std::min(max_freq,min_freq+k*actual_congestion*scaleFactor);
			u=u_old_factor*u_old+(1-u_old_factor)*u;

			//parent->pll.changePeriod(1.0/u*SimClock::Float::s);
			std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(r->get_id());

			//(*(it_pll->second)).changePeriod(1.0/u*SimClock::Float::s);
			(*(it_pll->second)).changePeriod((int)(1.0/u*SimClock::Float::s));

			///////////////////////////////////////
			const int samplePeriod=100000;//1.0/1e7*SimClock::Float::s;
//			const int samplePeriod=1.0/1e7*SimClock::Float::s; //100000ps=100ns
	
            parent->getQueue()->schedule(new DVFSEvent(parent,u,r), curTick()+samplePeriod);
        }
    private:
        ClassicalControlPolicy *parent;
		double u_old;
		Router_d *r;
    };
    
    static const Tick initialDelay=10000ull;
};
/**
 * This is used to collect data without change frequency
 */
class AllCongestionSensingNoFreqChange : public DVFSPolicy
{
public:
    Tick plusduty;
    Tick minusduty;
    Tick plusperiod;
    Tick minusperiod;
    
    std::ofstream dump;
    std::ofstream outfreq;

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

    EventQueue *getQueue() { return eventq; }
    
    AllCongestionSensingNoFreqChange(EventQueue *eventq)
            : DVFSPolicy(eventq), dump("m5out/dump.txt"), outfreq("m5out/freqout.txt")
    {
        outfreq<<"start_tick\tend_tick\tfreq\ttot_power\tdyn_power"
                 "\tstat_power\tdyn_clock_power\tcong_r5"<<std::endl;
        plusduty=10000ull; //In simulation ticks
        minusduty=10000ull;
        plusperiod=2000ull;
        minusperiod=500ull;
    }
	~AllCongestionSensingNoFreqChange()
	{
		for(std::map<int,std::ofstream*>::iterator it=dump_f.begin(); it!=dump_f.end(); it++)
			it->second->close();	

		dump_f.clear();
	}
    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
		// add the router to be managed here
        {
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

            ifstream in("freq.txt");
            if(in)
            {
                string descr;
                getline(in,descr);
                in>>plusduty>>minusduty>>plusperiod>>minusperiod;
                cout<<"Period data loaded from freq.txt ("
                    <<plusduty<<","<<minusduty<<","
                    <<plusperiod<<","<<minusperiod<<")"<<endl;
            }
        }
        eventq->schedule(new DVFSEvent(router,this),initialDelay);
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, AllCongestionSensingNoFreqChange *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle), count(count) {}
        
        virtual void process()
        {
			const int samplePeriod=100000; //100000ps=100ns

			//std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
            //r->printFullCongestionData(*(it->second));
			
			/**(it->second)<<curTick()<<" "<<r->getCongestion()
									<<" "<<r->getFullCongestion()
									<<" "<<(r->get_net_ptr())->getNetworkLatency()
									<<" "<<(r->get_net_ptr())->getQueueLatency()
									<<" "<<(r->get_net_ptr())->getAverageLatency()
									<<std::endl;
			*/
			if(r->get_id()==5||r->get_id()==0)
			// add the router to be managed here
        	{

				std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
				r->printRouterPower(*(it_pwr->second));
			}
			else
			{	
				r->computeRouterPower();
			}


            parent->getQueue()->schedule(new DVFSEvent(r,parent,toggle,count), curTick()+samplePeriod);
        }
    private:
        Router_d *r;
        AllCongestionSensingNoFreqChange *parent;
        bool toggle;
        int count;
    };
    
    static const Tick initialDelay=10000ull;
};


/**
 * This is used to collect data without change frequency
 */
class CongestionSensingNoFreqChange : public DVFSPolicy
{
public:
    Tick plusduty;
    Tick minusduty;
    Tick plusperiod;
    Tick minusperiod;
    
    std::ofstream dump;
    std::ofstream outfreq;

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

    EventQueue *getQueue() { return eventq; }
    
    CongestionSensingNoFreqChange(EventQueue *eventq)
            : DVFSPolicy(eventq), dump("m5out/dump.txt"), outfreq("m5out/freqout.txt")
    {
        outfreq<<"start_tick\tend_tick\tfreq\ttot_power\tdyn_power"
                 "\tstat_power\tdyn_clock_power\tcong_r5"<<std::endl;
        plusduty=10000ull; //In simulation ticks
        minusduty=10000ull;
        plusperiod=2000ull;
        minusperiod=500ull;
    }
	~CongestionSensingNoFreqChange()
	{
		for(std::map<int,std::ofstream*>::iterator it=dump_f.begin(); it!=dump_f.end(); it++)
			it->second->close();	

		dump_f.clear();
	}
    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
		// add the router to be managed here
        {
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

            ifstream in("freq.txt");
            if(in)
            {
                string descr;
                getline(in,descr);
                in>>plusduty>>minusduty>>plusperiod>>minusperiod;
                cout<<"Period data loaded from freq.txt ("
                    <<plusduty<<","<<minusduty<<","
                    <<plusperiod<<","<<minusperiod<<")"<<endl;
            }
            eventq->schedule(new DVFSEvent(router,this),initialDelay);
        }
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, CongestionSensingNoFreqChange *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle), count(count) {}
        
        virtual void process()
        {
			const int samplePeriod=100000; //100000ps=100ns

			std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
			//std::cout<<r->getObjectId()<<std::endl;
			r->printRouterPower(*(it_pwr->second));


            parent->getQueue()->schedule(new DVFSEvent(r,parent,toggle,count), curTick()+samplePeriod);
        }
    private:
        Router_d *r;
        CongestionSensingNoFreqChange *parent;
        bool toggle;
        int count;
    };
    
    static const Tick initialDelay=10000ull;
};
/**
 * This is used to collect data with a specific frequency set for all the
 * routers inside the frequency island
 */
class AllSensingStaticFreqSelectable : public DVFSPolicy
{
public:

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

    EventQueue *getQueue() { return eventq; }
    
    AllSensingStaticFreqSelectable(EventQueue *eventq)
            : DVFSPolicy(eventq){}

	~AllSensingStaticFreqSelectable()
	{
		for(std::map<int,std::ofstream*>::iterator it=dump_f.begin(); it!=dump_f.end(); it++)
			it->second->close();	
		dump_f.clear();
	}
    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
		// add the router to be managed here
        {
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

        }

        eventq->schedule(new DVFSEvent(router,this,true /*to change frequency*/),initialDelay);
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, AllSensingStaticFreqSelectable *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle) {}
        
        virtual void process()
        {
			const int samplePeriod=100000; //100000ps=100ns
			if(r->get_id()==5||r->get_id()==0)
			// add the router to be managed here
        	{
				std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
            	r->printFullCongestionData(*(it->second));
			
				std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
				r->printRouterPower(*(it_pwr->second));
			}
			else
			{
				r->computeRouterPower();
			}

			if(toggle)
			{
				Tick p = 10000; // set router frequency
				assert(p!=0);
				r->changePeriod(p);
			}

            parent->getQueue()->schedule(new DVFSEvent(r,parent,false/*toggle always false*/), curTick()+samplePeriod);
        }
    private:
        Router_d *r;
        AllSensingStaticFreqSelectable *parent;
        bool toggle;
    };
    
    static const Tick initialDelay=10000ull;
};

/**
 * This is used to collect data with a specific frequency set for all the
 * routers inside the frequency island
 */
class SensingStaticFreqSelectable : public DVFSPolicy
{
public:

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

    EventQueue *getQueue() { return eventq; }
    
    SensingStaticFreqSelectable(EventQueue *eventq)
            : DVFSPolicy(eventq){}

	~SensingStaticFreqSelectable()
	{
		for(std::map<int,std::ofstream*>::iterator it=dump_f.begin(); it!=dump_f.end(); it++)
			it->second->close();	
		dump_f.clear();
	}
    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0)
		// add the router to be managed here
        {
			std::stringstream str;
			str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";

			dump_f[router->get_id()]=new ofstream(str.str());
			assert((*dump_f[router->get_id()]).is_open());

			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());

            eventq->schedule(new DVFSEvent(router,this,true /*to change frequency*/),initialDelay);
        }
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, SensingStaticFreqSelectable *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle) {}
        
        virtual void process()
        {
			const int samplePeriod=100000; //100000ps=100ns

			std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
            r->printFullCongestionData(*(it->second));
			
			std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
			r->printRouterPower(*(it_pwr->second));

			if(toggle)
			{
				Tick p = 10000; // set router frequency
				assert(p!=0);
				r->changePeriod(p);
			}

            parent->getQueue()->schedule(new DVFSEvent(r,parent,false/*toggle always false*/), curTick()+samplePeriod);
        }
    private:
        Router_d *r;
        SensingStaticFreqSelectable *parent;
        bool toggle;
    };
    
    static const Tick initialDelay=10000ull;
};

/**
 * This is used to collect data with a specific frequency set for all the
 * routers inside the frequency island
 */
class SensingStaticFreqIslandSelectable : public DVFSPolicy
{
public:

	std::map<int,std::ofstream*> dump_f;
	std::map<int,std::ofstream*> dump_pwr_f;

	std::ifstream freqIslandFile;
	std::vector<Tick> freqIsland_v;

    EventQueue *getQueue() { return eventq; }
    
    SensingStaticFreqIslandSelectable(EventQueue *eventq)
            : DVFSPolicy(eventq),freqIslandFile("freqIsland.txt")
			{
				assert(freqIslandFile.is_open());
				std::string s;
				while(std::getline(freqIslandFile,s))
				{
					std::stringstream ss(s);
					while(ss.good())
					{
						Tick val=0;
						ss>>val;
						assert(val!=0);
						freqIsland_v.push_back(val);
					}
				}
				// print file content
				for(int i=0;i<freqIsland_v.size();i++)
					std::cout<<i<<" "<<freqIsland_v.at(i)<<std::endl;
			}

	~SensingStaticFreqIslandSelectable()
	{
		for(std::map<int,std::ofstream*>::iterator it=dump_f.begin(); it!=dump_f.end(); it++)
			it->second->close();	
		dump_f.clear();
	}
    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        router->setVoltageRegulator(new DummyVoltageRegulator);
        if(router->get_id()==5||router->get_id()==0||router->get_id()==10)
		{// add here the routers for which stats must be recorded
        
			//std::stringstream str;
			//str<<"m5out/dump_cong_router"<<router->getObjectId()<<".txt";	
			//dump_f[router->get_id()]=new ofstream(str.str());
			//assert((*dump_f[router->get_id()]).is_open());
			
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_pwr_router"<<router->getObjectId()<<".txt";
			dump_pwr_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_pwr_f[router->get_id()]).is_open());
		}
		//all the routers will be scheduled to change statically the frequency once
        eventq->schedule(new DVFSEvent(router,this,true /*to change frequency*/),initialDelay);
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(Router_d *r, SensingStaticFreqIslandSelectable *parent,
                bool toggle=false, int count=0) : Event(Default_Pri, AutoDelete),
                r(r), parent(parent), toggle(toggle) {}
        
        virtual void process()
        {
			const int samplePeriod=100000; //100000ps=100ns
			if(r->get_id()==5||r->get_id()==0 ||r->get_id()==10 )
			{// add here the routers for which stats must be recorded
				//std::map<int,std::ofstream*>::iterator it = parent->dump_f.find(r->get_id());
        	    //r->printFullCongestionData(*(it->second));
				
				std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_pwr_f.find(r->get_id());
				r->printRouterPower(*(it_pwr->second));
			}
			if(toggle)
			{// set router frequency according to the file freqIsland.txt
				Tick p = (parent->freqIsland_v).at(r->get_id()); 
				assert(p>=500 && p<=10000 && "Allowed frequencies between 2GHz and 100MHz"); // allowed between 2GHz and 100MHz
				r->changePeriod(p);
			}

            parent->getQueue()->schedule(new DVFSEvent(r,parent,false/*toggle always false*/), curTick()+samplePeriod);
        }
    private:
        Router_d *r;
        SensingStaticFreqIslandSelectable *parent;
        bool toggle;
    };
    
    static const Tick initialDelay=10000ull;
};
#endif
