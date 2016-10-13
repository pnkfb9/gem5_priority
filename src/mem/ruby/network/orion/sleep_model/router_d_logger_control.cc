#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <iostream>
#include <string>
#include <sstream>

#include <numeric>
#include <algorithm>
#include <functional>

//#include "mem/ruby/network/orion/NetworkPower.hh"
//#include "mem/ruby/network/orion/OrionConfig.hh"
//#include "mem/ruby/network/orion/OrionLink.hh"
//#include "mem/ruby/network/orion/OrionRouter.hh"

#include "base/types.hh"
#include "base/stl_helpers.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/CreditLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/SWallocator_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Switch_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/VCallocator_d.hh"


void
Router_d::setLoggerEpochInterval(int block, int subblock,uint32_t interval)
{
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());
	assert(interval>0);

	logger[block][subblock].setEpochInterval(interval);
}

void 
Router_d::setLoggerStartEpoch(int block, int subblock,Tick start)
{
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());
	assert(start>=0);

	logger[block][subblock].setStartEpoch(start);
}

void 
Router_d::setLoggerSchedWakeUp(int block, int subblock, uint32_t delay)
{
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());
	assert(delay>0);

	logger[block][subblock].schedWakeUp(delay);
}

void 
Router_d::pushBackLoggerEpoch(int block, int subblock)
{
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());
	
	return logger[block][subblock].pushBackEpoch();
}

void
Router_d::setLoggerAutoSched(int block, int subblock, bool auto_sched)
{
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());

	logger[block][subblock].setAutoSchedLog(auto_sched);
}

void 
Router_d::setLoggerPrintStatus(int block, int subblock, bool print)
{
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());

	logger[block][subblock].setLogPrintStatus(print);
}

bool 
Router_d::getLoggerAutoSched(int block, int subblock)
{
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());
	
	return logger[block][subblock].getAutoSchedLog();
}


uint32_t 
Router_d::getLoggerEpochInterval(int block, int subblock)
{	
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());

	return logger[block][subblock].getEpochInterval();
}

void 
Router_d::printLogger(std::ostream &out, int block, int subblock)
{	
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());
	logger[block][subblock].printLog(out);
}

void 
Router_d::printLoggerCSV(std::string filename, int block, int subblock)
{	
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());
	logger[block][subblock].printLogCSV(filename);
}


bool 
Router_d::getLoggerPrintStatus(int block, int subblock)
{
	assert(block>=0 && block<logger.size());
	assert(subblock>=0 && subblock<logger[block].size());

	return logger[block][subblock].getLogPrintStatus();
}

