#include "sleep_net_log_manager.hh"

#include "router_d_logger_control.hh"

SleepNetLogManager::SleepNetLogManager(
std::string name,
int m_log_id,
Router_d *router,
std::function<bool(int)> f_block_power_state,
std::function<void()> f_perf_counter,
std::function<PowerContainerRT(Tick)> f_power,
bool log_print_active,
bool auto_resch_log,
uint32_t epochInterval,
Sleep_transistor_model *stm)
:  Consumer(router)
{
		this->logger_name=name;
		this->m_log_id=m_log_id;
		this->m_router_ptr=router;
		this->startLogCycle=curTick();
		this->startEpoch=curTick();
		this->auto_resch_log=auto_resch_log;
		this->epochInterval=epochInterval;
		this->f_block_power_state=f_block_power_state;
		this->f_perf_counter=f_perf_counter;
		this->f_power=f_power;
		this->log_print_active=log_print_active; 
		this->stm_ptr=stm;
}
SleepNetLogManager::~SleepNetLogManager()
{
	logger.clear(); //no needed, but to be sure
}

void
SleepNetLogManager::pushBackEpoch(SleepNetLogEpoch &e)
{
	logger.push_back(e);
}

void
SleepNetLogManager::pushBackEpoch()
{
	SleepNetLogEpoch e;
	
	e.setFreq(m_router_ptr->get_frequency());
	e.setStartCycle(startEpoch);
	e.setEndCycle(curTick());

	f_perf_counter(); //evaluate perf_numbers	
	e.setPBlock(f_power(startEpoch));
	double tempStaticST=(double)(stm_ptr->get_st_num()*stm_ptr->get_st_Ioff());
	e.setPstaticSTNetwork(tempStaticST);
	double tempRC=(double)stm_ptr->get_block_Ceq()*
				((double)stm_ptr->get_st_Rswitch()/(double)stm_ptr->get_st_num());

	e.setRCconstant(tempRC);

	// evaluated from component consumer has member exported by router

	bool is_off=f_block_power_state(m_log_id);
	if(is_off)
		e.setState( OFF ); 
	else
		e.setState( ON ); 
	

	logger.push_back(e);
}
void SleepNetLogManager::updateStartEpoch(Tick t)
{
	assert(t>=curTick());
	startEpoch=t;
}

void SleepNetLogManager::updateStartEpoch()
{
	startEpoch=curTick()+1;
}

void SleepNetLogManager::wakeup()
{
	if(m_router_ptr->get_id()==0)
		printLog(std::cout);
	
	pushBackEpoch();
	updateStartEpoch(); //curTick()+1
	
	//set next event for power evaluation
	if(auto_resch_log)
	{
		this->scheduleEvent(epochInterval);
	}
}

void SleepNetLogManager::printLog(std::ostream& out)
{
	if(log_print_active)
	{
		out<<"@"<<curTick()<<" "<<logger_name<<std::endl;
		for(int j=0; j<logger.size();j++)
		{
			out<<"\t";
			logger[j].printEpoch(out);
		}
	}
	
}

void SleepNetLogManager::printLogCSV(std::string filename)
{
	std::ofstream f_out;
	f_out.open(filename);
	assert(f_out.is_open());
	for(int j=0; j<logger.size();j++)
	{
		logger[j].printEpochCSV(f_out);
	}
	f_out.close();
	
}
