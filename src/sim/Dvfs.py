 
from m5.SimObject import SimObject
from m5.params import *

# This was a helpful guide on how to call C++ from python in gem5:
# https://www.mail-archive.com/gem5-users@gem5.org/msg09060.html
# In particular, don't forget that:
# - the creation of the C++ objects is decoupled in time from the python ones,
#   as C++ objects are created in bulk in m5.instantiate(). Therefore, before
#   calling m5.instantiate() trying to call a method on a python object doesn't
#   call the corresponding C++ object, it returns an error!
# - if the python object is not registered as child of root, its C++ object will
#   never be created, so you need to do
#   dvfs = Dvfs(fuffa = True)
#   root.add_child('Dvfs',dvfs)
#   before m5.instantiate() or it will never work!

class Dvfs(SimObject):
    type = 'Dvfs'
    cxx_class = 'Dvfs'
    abstract = False
    cxx_header = "sim/dvfs.hh"
    
    recreateFiles = Param.Bool("If true, do not append to stats.txt and ruby.stats")
    
    @classmethod
    def export_methods(cls,code):
        code('''
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
''')
