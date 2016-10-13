#include "sleep_net_log_event.hh"

void SleepNetLogEpoch::printEpoch(std::ostream& out)
{
	out<<startCycle<<" --> "<<endCycle<<"TotalPower = "<<TotalPower<<"[W] {@"<<freq<< "Hz  State = "<<state<<"}"<<std::endl;
}
