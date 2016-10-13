
#include "DVFSPolicyPNET.hh"
#include "config/actual_dvfs_policy.hh"
#include "config/use_pnet.hh"

using namespace std;

//
// class DVFSPolicy
//

void DVFSPolicyPNET::addRouter(Router_PNET_Container_d* router, EventQueue* eventq, int num_pnet)
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
 	#if USE_PNET==0
		assert(false&&"\n\nThis DVFS policy requires PNET impl\n\n");
	#endif
	if(actual_dvfs_policy=="ALL_PNET_NOCHANGE")
    {
        static AllPnetNoChange policy(eventq,num_pnet);
		policy.addRouterImpl(router);
	}
    else if(actual_dvfs_policy=="ALL_PNET_CONTROL_DVFS")
    {
        static AllPnetControlDVFS policy(eventq,num_pnet);
		policy.addRouterImpl(router);
	}
    else if(actual_dvfs_policy=="ALL_PNET_THRESHOLD_DIFF_DVFS")
    {
        static AllPnetThDiffDVFS policy(eventq,num_pnet);
		policy.addRouterImpl(router);
	}
	else
		assert(false && "NO DVFS_PNET POLICY SELECTED");

}

DVFSPolicyPNET::DVFSPolicyPNET(EventQueue* eventq, int num_pnet) : eventq(eventq),m_num_pnet(num_pnet) 
{
	initialDelay=10000ull;
	samplePeriod=100000; //0.1 us
	//enPnetDVFS from file
	std::ifstream f_enPnetDVFS("./0_pnetConfig/enPnetDVFS.txt");
	assert(f_enPnetDVFS.is_open());
	std::string strFirst;
	std::getline(f_enPnetDVFS,strFirst);
	std::ifstream f_freqIfNoPnetDVFS("./0_pnetConfig/freqIfNoPnetDVFS.txt");
	assert(f_freqIfNoPnetDVFS.is_open());
	std::getline( f_freqIfNoPnetDVFS,strFirst);

	enPnetDVFS.resize(m_num_pnet);
	freqIfNoPnetDVFS.resize(m_num_pnet);
	
	for(int i=0;i<m_num_pnet;i++)
	{
		std::string str;
		assert(std::getline(f_enPnetDVFS,str));
		enPnetDVFS[i]=std::atoi(str.c_str());
		
		std::string str1;
		assert(std::getline(f_freqIfNoPnetDVFS,str1));
		freqIfNoPnetDVFS[i]=std::atol(str1.c_str());
		
		assert(freqIfNoPnetDVFS[i]>0);

	  //init dump files
		std::stringstream str_pwr;
		str_pwr<<"m5out/dump_pnet"<<i<<".txt";
		f_dumpPnetStats[i]=new std::ofstream(str_pwr.str());
	}

}

void DVFSPolicyPNET::addRouterImpl(Router_PNET_Container_d* router) {}

DVFSPolicyPNET::~DVFSPolicyPNET() 
{
	for(int i=0;i<m_num_pnet;i++)
	{
		f_dumpPnetStats[i]->close();
	}	
};


