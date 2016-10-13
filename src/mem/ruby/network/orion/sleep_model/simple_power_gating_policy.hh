#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <fstream>

#include "mem/ruby/common/Consumer.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"

#include "sim/core.hh"

#ifndef __ROUTER_D_POWER_POLICY__
#define __ROUTER_D_POWER_POLICY__

class SimplePowerGatingPolicy: public Consumer
{
	public:
	SimplePowerGatingPolicy(std::string, Router_d *);

	/* setup the logger model to collect stats */
	void init_logger();

	/* setup and init the policy structures if an y*/
	void init_policy();

	/* this is the actual logic of the policy */
	void policy_logic();

	/* virtual functions from Consumer */
	void wakeup();
	void print(std::ostream& out)const{out<<m_name<<std::endl;};

	private:
		std::string m_name;
		Router_d* m_router_ptr;
		std::ofstream f; //TODO shared for all router remove using power gating	islands
		int count;
};


#endif
