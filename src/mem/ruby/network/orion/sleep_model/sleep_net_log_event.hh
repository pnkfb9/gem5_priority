#include <iostream>
#include <string>
#include <functional>
#include <fstream>

#include "sim/core.hh"
#ifndef __LOG_EPOCH__
#define __LOG_EPOCH__


enum StateGatedBlock
{
	OFF=0,
	ON,
	ON_OFF,
	OFF_ON,
	INIT_MANAGER
};




class PowerContainerRT
{
	public:
		PowerContainerRT(){PstaticBlock=0;PdynBlock=0;PtotBlock=0;PdynClock=0;readAccess=0;writeAccess=0;};
		double PstaticBlock;
		double PdynBlock;
		double PtotBlock;
		double PdynClock;
		uint32_t readAccess;
		uint32_t writeAccess;
};
class SleepNetLogEpoch
{
public:
	
	SleepNetLogEpoch(double freq=1e9,double startCycle=0,double endCycle=0,
				StateGatedBlock state=OFF,double PdynSTNetwork=0, 
				double PstaticSTNetwork=0,double PdynBlock=0, 
				double PstaticBlock=0,double TotalPowerBlock=0,
				uint32_t readAccess=0,uint32_t writeAccess=0, double RCconstant=0)
	{
		this->freq=freq;
		this->startCycle=startCycle;
		this->endCycle=endCycle;
		this->state=state=state;
		this->PdynSTNetwork=PdynSTNetwork;
		this->PstaticSTNetwork=PstaticSTNetwork;
		this->PdynBlock=PdynBlock;
		this->PstaticBlock=PstaticBlock;
		this->PtotBlock=TotalPowerBlock;
		this->readAccess=readAccess;
		this->writeAccess=writeAccess;
		assert( !(state==OFF_ON && RCconstant!=0) );
		this->RCconstant=RCconstant;
	}
	void printEpoch(std::ostream&);
	void printEpochCSV(std::ofstream& out);


/*set methods*/
	void setFreq(uint32_t freq){this->freq=freq;}
	void setStartCycle(Tick newStart){this->startCycle=newStart;}
	void setEndCycle(Tick newEnd){this->endCycle=newEnd;}
	void setState(StateGatedBlock newState){this->state=newState;}
	void setPdynSTNetwork(double PdynSTNetwork){this->PdynSTNetwork=PdynSTNetwork;}
	void setPstaticSTNetwork(double
	PstaticSTNetwork){this->PstaticSTNetwork=PstaticSTNetwork;}

	void setPBlock(PowerContainerRT pBlock){this->PtotBlock=pBlock.PtotBlock;
						this->PdynBlock=pBlock.PdynBlock;
						this->PstaticBlock=pBlock.PstaticBlock;}
	void setReadAccess(uint32_t readAccess){this->readAccess=readAccess;}
	void setWriteAccess(uint32_t writeAccess){this->writeAccess=writeAccess;}

	void setRCconstant(double RCconstant){this->RCconstant=RCconstant;}

/*get methods*/
	uint32_t getFreq(){return this->freq;}
	Tick getStartCycle(){return this->startCycle;}
	Tick getEndCycle(){return this->endCycle;}
	StateGatedBlock getState(){return this->state;}
	double getPdynSTNetwork(){return this->PdynSTNetwork;}
	double getPstaticSTNetwork(){return this->PstaticSTNetwork;}
	double getPdynBlock(){return this->PdynBlock;}
	double getPstaticBlock(){return this->PstaticBlock;}
	double getPtotBlock(){return this->PtotBlock;}

	uint32_t getReadAccess(){return this->readAccess;}
	uint32_t getWriteAccess(){return this->writeAccess;}

	double getRCconstant(){return this->RCconstant;}

private:
	double freq;	/*frequency of the logic during the epoch*/
	Tick startCycle; /*initial Tick for the epoch*/
	Tick endCycle;
	StateGatedBlock state; 	/*state of the gated logic during the epoch*/
	
	double PdynSTNetwork;
	double PstaticSTNetwork;
	
	double PdynBlock;
	double PstaticBlock;

	double PtotBlock;	/*power due to both logic and st_network*/

	uint32_t readAccess;
	uint32_t writeAccess;

	double RCconstant;

};

#endif
