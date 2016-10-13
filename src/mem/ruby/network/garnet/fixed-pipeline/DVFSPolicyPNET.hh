/*
 * Copyright (c) 2015 Politecnico di Milano
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
 * Authors: Davide Zoni
 */

#ifndef DVFSPOLICY_PNET_H
#define	DVFSPOLICY_PNET_H
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


#define PRINT_PNET_DVFS_DEBUG 0

#define PRINT_FREQ_DVFS_ROUTERS 0
/**
 * Base class for DVFS policies. Also, this calls defines the defualt policy
 * which does nothing
 */


class DVFSPolicyPNET
{
public:
    /**
     * Called by router_d constructor to add a router
     * \param router new router instantiated
     */
    static void addRouter(Router_PNET_Container_d *router, EventQueue *eventq, int num_pnet);
    
    /**
     * Needed by EventWrapper for debugging purposes
     */
    std::string name() const { return typeid(*this).name(); }
    
    virtual ~DVFSPolicyPNET();

    
protected:
    explicit DVFSPolicyPNET(EventQueue *eventq, int num_pnet);
    
    virtual void addRouterImpl(Router_PNET_Container_d *router);
   
   	double frequencyToVoltage(Tick p /*the required period*/)
	{
		assert(p>0);
			
		double RequiredVdd=0.8;

		if(p<=625)/*>=1600MHz*/
			RequiredVdd = 1.1;
		else if(p<=800)/*>=1250MHz*/
			RequiredVdd = 1.0;
		else if(p<=1250 /*>=800MHz*/)
			RequiredVdd = 0.9;
		else if(p<=2000	/*>=500*/)
			RequiredVdd = 0.8;
		//else /*lower than 250MHz*/
		//	RequiredVdd = 0.7; 
		return RequiredVdd;
	}
	//functionaal variables coommon to all the policies
    EventQueue *eventq;
	int m_num_pnet;
	std::vector<int> enPnetDVFS; // from file check if dvfs is enable per each pnet
	std::vector<Tick> freqIfNoPnetDVFS; //from file fixed freq in Tick to use if no DVFS is used

	Tick	initialDelay;
	Tick	samplePeriod;
	// additional structures to write out stats to file
	std::map<int,std::ofstream*> f_dumpPnetStats;// 1 ofstream per pnet
	std::map<std::string,std::ofstream*> f_dumpPnetPerRouterStats; // 1 ofstream per router. Map using objectId

};

class AllPnetNoChange : public DVFSPolicyPNET
{
public:
	bool is_first_time;
	std::map<int,FrequencyIsland*> freq;

    EventQueue *getQueue() { return eventq; }
    
    AllPnetNoChange(EventQueue *eventq,int num_pnet)
				: DVFSPolicyPNET(eventq,num_pnet) 
    {	
		//initialDelay=10000ull;
		//samplePeriod=100000; //0.1 us
		is_first_time=true;

		for(int i=0;i<m_num_pnet;i++)
		{			
			freq[i]=new FrequencyIsland();
		}	
	}

    virtual void addRouterImpl(Router_PNET_Container_d *router)
	{
		std::cout<<"THIS POLICY DOES NOT DO ANYTHING, but computing the power"<<std::endl;
		std::vector<Router_d*> r_pnet = router->getContainedRoutersRef();
		//assign each router to the correct pnet vfi
		for(int i=0;i<r_pnet.size();i++)
			(*(freq.find(i)->second)).addObject(r_pnet.at(i));
		
		// schedule 1 event for each PNET to pass the correct old u values
		if(is_first_time)
		{
			is_first_time=false;
		  //generate 1 MainDVFS_Event per PNET
			for(int i=0;i<r_pnet.size();i++)
				eventq->schedule(new MainDVFSEvent(this, (freq.find(i)->second)/*VFI ptr*/,i) ,initialDelay );
		}
	}
	/*main event control loop*/
	class MainDVFSEvent : public Event
    {
    public:
		MainDVFSEvent(AllPnetNoChange *parent,FrequencyIsland* fi,int fi_id) : 
									Event(Default_Pri, AutoDelete), parent(parent),fi(fi),m_fi_id(fi_id){}
        
        virtual void process()
        {
		#if PRINT_PNET_DVFS_DEBUG == 1
			std::cout<<"@"<<curTick()<<" MainDVFSEvent::process()"<<std::endl;
		#endif
			std::vector<PeriodChangeable*>& temp_fi = fi->getObjList();
			int temp_congestion=0;
			assert(temp_fi.size()>0);
			for(int i=0;i<temp_fi.size();i++)
			{// update power evaluation
				Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);	
				assert(r!=NULL);
				temp_congestion+=r->getCongestion();
				Tick p = parent->freqIfNoPnetDVFS[m_fi_id];
				assert(p!=0);
				double new_vdd=parent->frequencyToVoltage(p /*the required period*/);
				r->computeRouterPower(new_vdd);
				r->changePeriod(p);
			}
			//write to PNET ofstream
			Router_d* r=dynamic_cast<Router_d*>(temp_fi[0]);
			std::map<int,std::ofstream*>::iterator it_pwr = parent->f_dumpPnetStats.find(m_fi_id);
			(*(it_pwr->second))<<curTick()<<","<<r->get_frequency()<<","<<temp_congestion<<std::endl;
			//schedule next MainDVFS event
			parent->getQueue()->schedule(new MainDVFSEvent(parent,fi,m_fi_id), curTick() + parent->samplePeriod);
		}
    private:
        AllPnetNoChange *parent;
		FrequencyIsland *fi;
		int m_fi_id;
   };

	

};

/*
 threshold-based differential PNET policy. 
 The changes are controlled on the difference between the actual and the
 previous congestion. Namely, if the congestion increaes or decreases too much
 within two samples than a control action to correct the performance is
 provided. This way it is similar to an event-based solution. Moreover it avoids
 oscillations in the actuation!!!! 
*/
class AllPnetThDiffDVFS : public DVFSPolicyPNET
{
public:

	std::ifstream k_file;
// START - this is for stability analysis only (NEVER USED IN SIMULATION)
	std::ofstream u_unconstr_file;
// END - this is for stability analysis only (NEVER USED IN SIMULATION)



	std::map<int,FrequencyIsland*> freq;
	std::vector<Tick> freq_vector;
	
	//compared with (cong_now-cong_old)/cong_old to eventually trigger a
	//reconfiguration event
	double threshold_diff_high;
	double threshold_diff_low;

	bool is_first_time;
	Tick voltageTransientPeriod;

    EventQueue *getQueue() { return eventq; }
    ~AllPnetThDiffDVFS()
	{
		//close all files
		//delete all structures

	}
    AllPnetThDiffDVFS(EventQueue *eventq,int num_pnet)
				: DVFSPolicyPNET(eventq,num_pnet),k_file("k.txt"), u_unconstr_file("u_unconstr_file.txt")
    {

//////// vector for the available frequencies////////
		freq_vector.resize(7); //FIXME make it dynamic from file
		freq_vector.at(0)=10000;/*100MHz*/	
		freq_vector.at(1)=5000;/*250MHz*/
		freq_vector.at(2)=2000;/*500MHz*/
		freq_vector.at(3)=1125;/*800MHz*/
		freq_vector.at(4)=1000;/*1000MHz*/		
		freq_vector.at(5)=800;/*1250MHz*/		
		freq_vector.at(6)=625;/*1600MHz*/		
////////////////////////////////////////////////////
		threshold_diff_high=0.1; // 10% variation in the congestion to trigger a reconfiguration
		threshold_diff_low=0.1; //

		is_first_time=true;
		voltageTransientPeriod=1000000; // 5us
//		initialDelay=10000ull;
//		samplePeriod=100000; //0.1 us
		
		//////// init the rest of the structures
		for(int i=0;i<m_num_pnet;i++)
		{			
			freq[i]=new FrequencyIsland();
		}
	}

    virtual void addRouterImpl(Router_PNET_Container_d *router)
    {
        using namespace std;
		std::vector<Router_d*> r_pnet = router->getContainedRoutersRef();
		//assign each router to the correct pnet vfi
		for(int i=0;i<r_pnet.size();i++)
			(*(freq.find(i)->second)).addObject(r_pnet.at(i));
	
		// schedule 1 event for each PNET to pass the correct old u values
		if(is_first_time)
		{
			is_first_time=false;
			for(int i=0;i<r_pnet.size();i++)
				eventq->schedule(new MainDVFSEvent
										( this, 
										(freq.find(i)->second)/*VFI ptr*/,
										i,// pnet_id
										0/*freq vector id (lowest freq)*/,
										0.7/*initial voltage*/,
										0/*initial congestion*/ )
									,initialDelay );
		}
    }
    
private:
    class VregEvent : public Event
    {
    public:
        VregEvent(AllPnetThDiffDVFS *parent,FrequencyIsland* fi,int fi_id,int freq_vect_id, double actualVdd, int  old_cong) : 
									Event(Default_Pri, AutoDelete), parent(parent),fi(fi),m_fi_id(fi_id),freq_vect_id(freq_vect_id),actualVdd(actualVdd),
									old_cong(old_cong){}
        
        virtual void process()
        {
		#if PRINT_PNET_DVFS_DEBUG == 1
			std::cout<<"@"<<curTick()<<" \t\tVregEvent::process()"<<std::endl;
		#endif

			// if we arrived here we paid a voltage change, thus we can change
			// the frequency now [TODO: NOT accurate, should compute pwr with old VDD
			// here!!!].
			std::vector<PeriodChangeable*>& temp_fi = fi->getObjList();
			Tick p = parent->freq_vector.at(freq_vect_id);
			for(int i=0;i<temp_fi.size();i++)
			{
				Router_d* r = dynamic_cast<Router_d*>(temp_fi[i]);	
				r->changePeriod(p);
			}
            parent->getQueue()->schedule(
											new MainDVFSEvent(parent,fi,m_fi_id,freq_vect_id,actualVdd,old_cong),
											curTick()+parent->samplePeriod
										);
        }
    private:
        AllPnetThDiffDVFS *parent;
		FrequencyIsland *fi;
		int m_fi_id;
		int freq_vect_id;	
		double actualVdd;
		int old_cong;
    };
	/*this event is to regulate the voltage*/
    
	class MainDVFSEvent : public Event
    {
    public:
		MainDVFSEvent(AllPnetThDiffDVFS *parent,FrequencyIsland* fi,int fi_id,int freq_vect_id, double actualVdd,int old_cong):
							Event(Default_Pri, AutoDelete), parent(parent),fi(fi),m_fi_id(fi_id),
													freq_vect_id(freq_vect_id),actualVdd(actualVdd) , old_cong(old_cong){}
        
        virtual void process()
        {

		#if PRINT_PNET_DVFS_DEBUG == 1
			std::cout<<"@"<<curTick()<<" MainDVFSEvent::process()"<<std::endl;
		#endif
			std::vector<PeriodChangeable*>& temp_fi = fi->getObjList();
			int temp_congestion=0;
			assert(temp_fi.size()>0);
			for(int i=0;i<temp_fi.size();i++)
			{// update power evaluation
				Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);	
				assert(r!=NULL);
				temp_congestion+=r->getCongestion();
				r->computeRouterPower(actualVdd); 
			}
			if(parent->enPnetDVFS[m_fi_id]==0)
			{// do not DVFS this PNET; keep at max freq
				int f_id=parent->freq_vector.size()-1;
				Tick p=parent->freqIfNoPnetDVFS[m_fi_id];//use fixed freq from configPnetDVFS file //parent->freq_vector[f_id];//max freq
				double new_vdd=parent->frequencyToVoltage(p /*the required period*/);
				for(int i=0;i<temp_fi.size();i++)
				{
					Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);
					r->changePeriod(p);
				}
				//write to PNET ofstream
				Router_d* r=dynamic_cast<Router_d*>(temp_fi[0]);
				std::map<int,std::ofstream*>::iterator it_pwr = parent->f_dumpPnetStats.find(m_fi_id);
				(*(it_pwr->second))<<curTick()<<","<<r->get_frequency()<<","<<temp_congestion<<std::endl;
	
				parent->getQueue()->schedule(
								new MainDVFSEvent(parent,fi,m_fi_id,f_id,new_vdd,temp_congestion), 
								curTick() + parent->samplePeriod
							);
			}
			else
			{
            	Tick p=0;
				if (old_cong==0) old_cong=1;// hack to have the policy working

				double congestion_diff = (double)(temp_congestion - old_cong) /
				(double) old_cong;
				int new_freq_vect_id=freq_vect_id;
				if( congestion_diff >= parent->threshold_diff_high)
					new_freq_vect_id++;
				else if( parent->threshold_diff_low <= (-congestion_diff))
					new_freq_vect_id--;

				if(new_freq_vect_id >= parent->freq_vector.size() || new_freq_vect_id < 0 )
					new_freq_vect_id=freq_vect_id;

				if(!(new_freq_vect_id>=0 && new_freq_vect_id<=6))
					std::cout<<new_freq_vect_id<<std::endl;
				assert(new_freq_vect_id>=0 && new_freq_vect_id<=6);

				p = parent->freq_vector[new_freq_vect_id];
				assert(p!=0);
				double new_vdd=parent->frequencyToVoltage(p /*the required period*/);

				int actualCong=0;
				if(new_freq_vect_id!=freq_vect_id)	
				// freq is going to be changed
					actualCong=temp_congestion; // the new one
				else
					actualCong= old_cong;

			  //write to PNET ofstream
			  	Router_d* r=dynamic_cast<Router_d*>(temp_fi[0]);
				std::map<int,std::ofstream*>::iterator it_pwr = parent->f_dumpPnetStats.find(m_fi_id);
				(*(it_pwr->second))<<curTick()<<","<<r->get_frequency()<<","<<temp_congestion<<std::endl;
	
				if(new_vdd>actualVdd)
				{
					#if PRINT_FREQ_DVFS_ROUTERS == 1
					std::cout<<"\t waiting for VDD increase"<<std::endl;
					#endif
					parent->getQueue()->schedule(
													new VregEvent(parent,fi,m_fi_id,new_freq_vect_id,new_vdd,actualCong), 
															curTick()+parent->voltageTransientPeriod
												);
				}
				else
				{
					for(int i=0;i<temp_fi.size();i++)
					{
						Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);
						r->changePeriod(p);
					}
					parent->getQueue()->schedule(
												new MainDVFSEvent(parent,fi,m_fi_id,new_freq_vect_id,new_vdd,actualCong), 
												curTick() + parent->samplePeriod
											);
				}
			}//if enPnetDVFS	
        }
    private:
        AllPnetThDiffDVFS *parent;
		FrequencyIsland *fi;
		int m_fi_id;
		int freq_vect_id;	
		double actualVdd;
		int old_cong;
    };




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

class AllPnetControlDVFS : public DVFSPolicyPNET
{
public:
    std::map<int,FrequencyIsland*> freq;
    std::map<int,AdvancedPllModel*> pll;

	


	std::ifstream k_file;
	double k;
	bool is_first_time;
	Tick voltageTransientPeriod;

// START - this is for stability analysis only (NEVER USED IN SIMULATION)
	std::ofstream u_unconstr_file;
// END - this is for stability analysis only (NEVER USED IN SIMULATION)

    EventQueue *getQueue() { return eventq; }
    
    AllPnetControlDVFS(EventQueue *eventq, int num_pnet)
				: DVFSPolicyPNET(eventq,num_pnet),k_file("k.txt"), u_unconstr_file("u_unconstr_file.txt")
    {
		k=0;
		assert(k_file.is_open());		
		k_file>>k;
		assert(k!=0);
		is_first_time=true;
		voltageTransientPeriod=1000000;//5000000; // 5us
		
		//generate PLLs and VFI in a number equal to the protocol's vnets!!
		for(int i=0;i<m_num_pnet;i++)
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
				eventq->schedule(new MainDVFSEvent(this,1e8 /*initial u*/, /*VFI ptr*/(freq.find(i)->second),i,1.0 /*initial u_unconstr*/),initialDelay);
		}
        
    }
    
private:
    class DVFSEvent : public Event
    {
    public:
        DVFSEvent(AllPnetControlDVFS *parent, double u_old, FrequencyIsland* fi,int fi_id,double actualVdd) : 
									Event(Default_Pri, AutoDelete), parent(parent),u_old(u_old),fi(fi),m_fi_id(fi_id),actualVdd(actualVdd) {}
        
        virtual void process()
        {
			// if we arrived here we paid a voltage change
	   		Tick new_period = (unsigned long long)(1.0/u_old*SimClock::Float::s);
			std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(m_fi_id);
			assert(it_pll->second!=NULL);
			(*(it_pll->second)).changePeriod(new_period);

            parent->getQueue()->schedule(new MainDVFSEvent(parent,u_old,fi,m_fi_id,actualVdd), curTick()+parent->samplePeriod);
        }
    private:
        AllPnetControlDVFS *parent;
		Tick new_period;
		double u_old;
		FrequencyIsland *fi;
		int m_fi_id;
		// for stability analysis DISABLE IF NOT USED

		double actualVdd;

    };
	/*this event is to regulate the voltage*/
    
	class MainDVFSEvent : public Event
    {
    public:
		MainDVFSEvent(AllPnetControlDVFS *parent, double u_old, FrequencyIsland* fi,int fi_id, double actualVdd): 
									Event(Default_Pri, AutoDelete), parent(parent),
									u_old(u_old),fi(fi),m_fi_id(fi_id), actualVdd(actualVdd) {}
        
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
				temp_congestion+=r->getCongestion();
				r->computeRouterPower(actualVdd); 
			}
			
			if(parent->enPnetDVFS[m_fi_id]==0)
			{// do not DVFS this PNET; keep at max freq
				Tick p=parent->freqIfNoPnetDVFS[m_fi_id];//use fixed freq from configPnetDVFS file //parent->freq_vector[f_id];//max freq
				double new_vdd=parent->frequencyToVoltage(p /*the required period*/);
				for(int i=0;i<temp_fi.size();i++)
				{
					Router_d* r=dynamic_cast<Router_d*>(temp_fi[i]);
					r->changePeriod(p);
				}
				//write to PNET ofstream
				Router_d* r=dynamic_cast<Router_d*>(temp_fi[0]);
				std::map<int,std::ofstream*>::iterator it_pwr = parent->f_dumpPnetStats.find(m_fi_id);
				(*(it_pwr->second))<<curTick()<<","<<r->get_frequency()<<","<<temp_congestion<<std::endl;
	
				parent->getQueue()->schedule(
								new MainDVFSEvent(parent,1.0/(p*SimClock::Float::s),fi,m_fi_id,new_vdd),
									curTick() + parent->samplePeriod
							);
			}
			else
			{
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
					parent->getQueue()->schedule(new DVFSEvent(parent,u,fi,m_fi_id,new_vdd), curTick()+parent->voltageTransientPeriod);
				}
				else
				{// immediately start a frequency change and reschedule the control law event
					#if PRINT_FREQ_DVFS_ROUTERS == 1
					std::cout<<"\t change the frequency now..."<<std::endl;
					#endif
					std::map<int,AdvancedPllModel*>::iterator it_pll = parent->pll.find(m_fi_id);
					assert(it_pll->second!=NULL);
					(*(it_pll->second)).changePeriod(new_period);

					//write to PNET ofstream
				  	Router_d* r=dynamic_cast<Router_d*>(temp_fi[0]);
					std::map<int,std::ofstream*>::iterator it_pwr = parent->f_dumpPnetStats.find(m_fi_id);
					(*(it_pwr->second))<<curTick()<<","<<r->get_frequency()<<","<<temp_congestion<<std::endl;

					parent->getQueue()->schedule(new MainDVFSEvent(parent,u,fi,m_fi_id,new_vdd), curTick()+parent->samplePeriod);
				}
			}// if enPnetDVFS==1 this pnet is dvfs controlled !!!
        }
    private:
        AllPnetControlDVFS *parent;
		double u_old;
		FrequencyIsland *fi;
		int m_fi_id;
		double actualVdd;
    };



    static const Tick initialDelay=10000ull;
};


#endif
