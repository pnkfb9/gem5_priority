
#include "DVFSPolicy.h"
#include "config/actual_dvfs_policy.hh"
#include "config/use_pnet.hh"

using namespace std;

//#define PLL //Change frequency using PLL

/**
 * Change period in a square wave way
 */
class DutyCycleRouters : public DVFSPolicy
{
public:
    DutyCycleRouters(EventQueue *eventq) : DVFSPolicy(eventq), event(this),
#ifdef PLL
            pll(&routers,eventq),
#endif //PLL
            toggle(0),plusduty(0),minusduty(0),plusperiod(0),minusperiod(0) {}
        
    virtual void addRouterImpl(Router_d *router)
    {
        if(routers.empty())
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
            if(plusduty!=0 && minusduty!=0 && plusperiod!=0 && minusperiod!=0)
                eventq->schedule(&event,initialDelay);
            else cout<<"DFS policy on routers disabled in freq.txt"<<endl;
        }
        routers.addObject(router);
        router->setVoltageRegulator(new DummyVoltageRegulator);
    }
    
    void dvfsEvent()
    {
        Tick p = toggle ? minusperiod : plusperiod;
            
//        cout<<"--> @"<<curTick()<<" Changing period to "<<p
//            <<" ("<<routers.getObjList().size()<<" items)"<<endl;

        std::vector<PeriodChangeable*>& pc=routers.getObjList();
        for(int i=0;i<pc.size();i++)
        {
            Router_d* r=dynamic_cast<Router_d*>(pc[i]);
            assert(r!=NULL);
            r->computeRouterPower();
        }
        
#ifdef PLL
        pll.changePeriod(p);
#else //PLL
        routers.changePeriod(p);
#endif //PLL
        
        eventq->schedule(&event,curTick()+(toggle ? minusduty : plusduty));
        toggle^=1;
    }
    
    virtual ~DutyCycleRouters()
    {
        //To prevent an assertion at gem5 exit
        if(event.scheduled()) eventq->deschedule(&event);
    }
    
private:
    EventWrapper<DutyCycleRouters, &DutyCycleRouters::dvfsEvent> event;
    FrequencyIsland routers;
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

//
// class DVFSPolicy
//

void DVFSPolicy::addRouter(Router_d* router, EventQueue* eventq)
{
    //std::cout<<std::endl<<std::endl<<"## DFS on routers disabled in DVFSPolicy.cpp"<<std::endl<<std::endl;
    //return;
    
    // Policy selection: uncomment the one you want and rebuild gem5
    std::string actual_dvfs_policy = ACTUAL_DVFS_POLICY;

    //static DVFSPolicy policy(eventq);
    //static ChangeAllRoutersOnce policy(eventq);
    //static ChangeAllRoutersOnce2 policy(eventq);
    //static DutyCycleRouters policy(eventq);
    //static CongestionSensing policy(eventq);
	if(actual_dvfs_policy=="ALL_THRESHOLD_100_500_1000_GROUPED")
	{
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static AllThresholdPolicyCongestionSensing100_500_1000Grouped policy(eventq);
		policy.addRouterImpl(router);
	}
    else if(actual_dvfs_policy=="DUTY_CYCLE")
    {
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
        static DutyCycleRouters policy(eventq);
		policy.addRouterImpl(router);
    }
    else if(actual_dvfs_policy=="DUTY_CYCLE_SWITCHED_IDENTIFICATION")
    {
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
        static DutyCycleSwitchedSystemRouters policy(eventq);
		policy.addRouterImpl(router);
    }	
	else if(actual_dvfs_policy=="THRESHOLD_100_500_1000_DVFS")
	{
	
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static ThresholdPolicyCongestionSensing100_500_1000_DVFS policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="ALL_THRESHOLD_100_500_1000")
	{	
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif	
		static AllThresholdPolicyCongestionSensing100_500_1000 policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="THRESHOLD_100_500_1000")
	{		
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static ThresholdPolicyCongestionSensing100_500_1000 policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="ALL_THRESHOLD_250_500_800_GROUPED")
	{		
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static AllThresholdPolicyCongestionSensing250_500_800Grouped policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="ALL_THRESHOLD_250_500_800")
	{		
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static AllThresholdPolicyCongestionSensing250_500_800 policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="THRESHOLD_250_500_800")
	{		
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static ThresholdPolicyCongestionSensing250_500_800 policy(eventq);
		policy.addRouterImpl(router);
	}

    //static ProportionalControlPolicy policy(eventq);
	else if(actual_dvfs_policy=="ALL_CLASSIC_CONTROL_GROUPED")
	{		
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static AllClassicalControlPolicyGrouped policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="ALL_CLASSIC_CONTROL")
	{		
		//#if USE_PNET==1
		//	assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		//#endif
		static AllClassicalControlPolicy policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="CLASSIC_CONTROL_DVFS")
	{		
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static ClassicalControlPolicyDVFS policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="ALL_CLASSIC_CONTROL_DVFS")
	{		
		//#if USE_PNET==1
		//	assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		//#endif
		static AllClassicalControlPolicyDVFS policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="ALL_CLASSIC_CONTROL_DVFS_GROUPED")
	{		
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static AllClassicalControlPolicyDVFS_Grouped policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="CLASSIC_CONTROL")
	{		
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static ClassicalControlPolicy policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="ALL_NOCHANGE_SENSING")
	{	
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static AllCongestionSensingNoFreqChange policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="NOCHANGE_SENSING")
	{	
		//#if USE_PNET==1
		//	assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		//#endif
		static CongestionSensingNoFreqChange policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="ALL_SINGLE_FREQ_SELECT")
	{	
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static AllSensingStaticFreqSelectable policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="SINGLE_FREQ_SELECT")
	{	
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static SensingStaticFreqSelectable policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_dvfs_policy=="FREQ_ISLANDS")
	{	
		#if USE_PNET==1
			assert(false&&"\n\nThis DVFS policy has not  been deisgned to be used with PNET impl\n\n");
		#endif
		static SensingStaticFreqIslandSelectable policy(eventq);
		policy.addRouterImpl(router);
	}
	else
		assert(false && "NO DVFS POLICY SELECTED");

}

DVFSPolicy::DVFSPolicy(EventQueue* eventq) : eventq(eventq) {}

void DVFSPolicy::addRouterImpl(Router_d* router) {}

DVFSPolicy::~DVFSPolicy() {}

