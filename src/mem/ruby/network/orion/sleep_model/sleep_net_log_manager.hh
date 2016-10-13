#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include "mem/ruby/common/Consumer.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"

#include "sim/core.hh"
#include "sleep_net_log_event.hh"

#ifndef __LOG_MANAGER__
#define __LOG_MANAGER__


/* This is public from consumer to be allowed to set events*/
class SleepNetLogManager: public Consumer
{
public:
	SleepNetLogManager(
		std::string name,
		int m_log_id,
		Router_d *router,
		std::function<bool(int)> f_block_power_state,
		std::function<void()> f_perf_event,
		std::function<PowerContainerRT(Tick)> f_power,
		bool log_print_active=false,
		bool auto_resch_log=false,
		uint32_t epochInterval=100000,
		Sleep_transistor_model *stm=NULL);

	~SleepNetLogManager();
	
	void updateStartEpoch(Tick t);
	void updateStartEpoch();
	
	void pushBackEpoch();
	void pushBackEpoch(SleepNetLogEpoch&);


	void wakeup(); /*for the event queue*/
	void print(std::ostream& out)const{out<<logger_name<<std::endl;};

	void setAutoSchedLog(bool auto_sched=false){this->auto_resch_log=auto_sched;};

	void setEpochInterval(uint32_t interval=1){this->epochInterval=interval;};
	void setStartEpoch(Tick start=curTick())
	{
		assert(start>=curTick());
		startEpoch=start;
	};
	void schedWakeUp(uint32_t delay=1){this->scheduleEvent(delay);};
	
	
	bool getAutoSchedLog(){return auto_resch_log;};
	uint32_t getEpochInterval(){return this->epochInterval;};
	
	
	
	
	void printLog(std::ostream&);
	void printLogCSV(std::string);

/*set methods*/
	void setStartLogCycle(double newLogStart){this->startLogCycle=newLogStart;}
	void setEndLogCycle(double newLogEnd){this->endLogCycle=newLogEnd;}

	void setLogPrintStatus(bool state=false){this->log_print_active=state; }

/*get methods*/
	double getStartLogCycle(){return this->startLogCycle;}
	double getEndLogCycle(){return this->endLogCycle;}

	bool getLogPrintStatus(){return this->log_print_active; }
private:
	Tick startLogCycle; /*initial Tick for the epoch*/
	Tick endLogCycle;

	Tick startEpoch;
	Router_d* m_router_ptr;
	std::string logger_name;
	std::vector<SleepNetLogEpoch> logger;

	uint32_t epochInterval;
	bool auto_resch_log;

	std::function<bool(int)> f_block_power_state;
	std::function<void()> f_perf_counter;
	std::function<PowerContainerRT(Tick)> f_power;

	bool log_print_active;
	Sleep_transistor_model* stm_ptr;

	int m_log_id;
};

#endif
