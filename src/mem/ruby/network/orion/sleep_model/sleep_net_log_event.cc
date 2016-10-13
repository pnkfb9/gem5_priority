#include "sleep_net_log_event.hh"

void SleepNetLogEpoch::printEpoch(std::ostream& out)
{
	out<<startCycle<<" -> "<<endCycle
		<<" PTotBlock="<<PtotBlock<<"[W]"
		<<" PDynBlock="<<PdynBlock <<"[W]"
		<<" PStatBlock="<<PstaticBlock <<"[W]"
		<<" PSTstatic="<<PstaticSTNetwork <<"[W]"
		<<"5*RCconst="<<5*RCconstant<<"[s]"
		<<" {@"<<freq<< "Hz State = "<<state<<"}"<<std::endl;
}

void SleepNetLogEpoch::printEpochCSV(std::ofstream& out)
{
	out<<startCycle<<","<<endCycle<<","
		<<PtotBlock<<","
		<<PdynBlock <<","
		<<PstaticBlock <<","
		<<PstaticSTNetwork <<","
		<<5*RCconstant<<","
		<<freq<<","
		<<state
		<<std::endl;
}
