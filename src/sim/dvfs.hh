
#include "params/Dvfs.hh"
#include "sim/sim_object.hh"
#include <vector>
#include <functional>

#ifndef DVFS_H
#define DVFS_H

class Dvfs : public SimObject
{
public:
    /**
     * Constructor is meant to be called by python only once
     */
    Dvfs(DvfsParams *p);
    
    /**
     * Everyone else should call this
     */
    static Dvfs *instance();

    void configure(int numRouters);
    void configureCpu(int numCpus);
    
    bool recreateStatsFiles() const { return recreateFiles; }
    
    void setTotalRouterPower(int i, double p) { totalRouterPower.at(i)=p; }
    void setStaticRouterPower(int i, double p) { staticRouterPower.at(i)=p; }
    void setDynamicRouterPower(int i, double p) { dynamicRouterPower.at(i)=p; }
    void setClockRouterPower(int i, double p) { clockRouterPower.at(i)=p; }
    
    void setAverageRouterFrequency(int i, double f) { routerFrequency.at(i)=f; }
    void setAverageCpuFrequency(int i, double f) { cpuFrequency.at(i)=f; }
    void setAverageRouterVoltage(int i, double v) { routerVoltage.at(i)=v; }
    void setAverageCpuVoltage(int i, double v) { cpuVoltage.at(i)=v; }
    
    double getCpuTemperature(int i) { return cpuTemperature.at(i); }
    double getRouterTemperature(int i) { return routerTemperature.at(i); }
    
    void updateCompleted() { updateCounter++; }
    
    void registerThermalPolicyCallback(std::function<void (Dvfs *)> tpc)
    {
        thermalPolicyCallback=tpc;
    }
    
    // Functions exported to python -- begin
    int getUpdateCounter();
    int getNumRouters();
    int getNumCpus();
    double getTotalRouterPower(int i);
    double getStaticRouterPower(int i);
    double getDynamicRouterPower(int i);
    double getClockRouterPower(int i);
    double getAverageRouterFrequency(int i);
    double getAverageCpuFrequency(int i);
    double getAverageRouterVoltage(int i);
    double getAverageCpuVoltage(int i);
    void setCpuTemperature(int i, double t);
    void setRouterTemperature(int i, double t);
    void runThermalPolicy();
    // Functions exported to python -- end
    
    virtual ~Dvfs();
    
private:
    static Dvfs *theInstance;
    
    std::vector<double> totalRouterPower;
    std::vector<double> staticRouterPower;
    std::vector<double> dynamicRouterPower;
    std::vector<double> clockRouterPower;
    
    std::vector<double> routerFrequency;
    std::vector<double> cpuFrequency;
    std::vector<double> routerVoltage;
    std::vector<double> cpuVoltage;
    
    std::vector<double> cpuTemperature;
    std::vector<double> routerTemperature;
    int updateCounter;
    //True if stats.txt and ruby.stats have to be recreated at every stats.dump()
    bool recreateFiles;
    std::function<void (Dvfs *)> thermalPolicyCallback;
};

#endif //DVFS_H
