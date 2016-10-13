 
#include "dvfs.hh"
#include <cassert>

using namespace std;

Dvfs::Dvfs(DvfsParams *p) : SimObject(p), recreateFiles(p->recreateFiles)
{
    assert(theInstance==0); //Don't call it twice
    theInstance=this;
}

Dvfs *Dvfs::instance()
{
    //assert(theInstance); //Has to exist
    return theInstance;
}

void Dvfs::configure(int numRouters)
{
    totalRouterPower.resize(numRouters,0);
    staticRouterPower.resize(numRouters,0);
    dynamicRouterPower.resize(numRouters,0);
    clockRouterPower.resize(numRouters,0);
    routerFrequency.resize(numRouters,0);
    routerVoltage.resize(numRouters,0);
    routerTemperature.resize(numRouters,0);
    updateCounter=0;
}

void Dvfs::configureCpu(int numCpus)
{
    cpuFrequency.resize(numCpus,0);
    cpuVoltage.resize(numCpus,0);
    cpuTemperature.resize(numCpus,0);
}

int   Dvfs::getUpdateCounter()                { return updateCounter; }
int   Dvfs::getNumRouters()                   { return totalRouterPower.size(); }
int   Dvfs::getNumCpus()                      { return cpuFrequency.size(); }
double Dvfs::getTotalRouterPower(int i)       { return totalRouterPower.at(i); }
double Dvfs::getStaticRouterPower(int i)      { return staticRouterPower.at(i); }
double Dvfs::getDynamicRouterPower(int i)     { return dynamicRouterPower.at(i); }
double Dvfs::getClockRouterPower(int i)       { return clockRouterPower.at(i); }
double Dvfs::getAverageRouterFrequency(int i) { return routerFrequency.at(i); }
double Dvfs::getAverageCpuFrequency(int i)    { return cpuFrequency.at(i); }
double Dvfs::getAverageRouterVoltage(int i)   { return routerVoltage.at(i); }
double Dvfs::getAverageCpuVoltage(int i)      { return cpuVoltage.at(i); }
void   Dvfs::setCpuTemperature(int i, double t)    { cpuTemperature.at(i)=t; }
void   Dvfs::setRouterTemperature(int i, double t) { routerTemperature.at(i)=t; }

void Dvfs::runThermalPolicy()
{
    if(thermalPolicyCallback) thermalPolicyCallback(this);
//    for(int i=0;i<cpuTemperature.size();i++)
//        cout<<"cpu"<<i<<" "<<cpuTemperature.at(i)<<endl;
//    for(int i=0;i<routerTemperature.size();i++)
//        cout<<"router"<<i<<" "<<routerTemperature.at(i)<<endl;
}

Dvfs::~Dvfs() {}


Dvfs *Dvfs::theInstance=0;

//

Dvfs *DvfsParams::create()
{
    return new Dvfs(this);
}
