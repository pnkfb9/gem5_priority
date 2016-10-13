
#ifndef DVFSPOLICY_O3CORE_H
#define DVFSPOLICY_O3CORE_H

#include "../base.hh"
#include "sim/core.hh"

/**
 * Base class for DVFS policies. Also, this calls defines the defualt policy
 * which does nothing
 */
class DVFSPolicyO3Core
{
public:
    /**
     * Called by BaseCPU to add a CPU to the DVFS policy
     * \param cpu the cpu to add to the DVFS policy
     * \param eventq the event queue of the CPU
     */
    static void addCore(BaseCPU *cpu, EventQueue *eventq);
    
    /**
     * Needed by EventWrapper for debugging purposes
     */
    std::string name() const { return typeid(*this).name(); }
    
    virtual ~DVFSPolicyO3Core();
    
protected:
    explicit DVFSPolicyO3Core(EventQueue *eventq);
    
    virtual void addCoreImpl(BaseCPU *cpu);
    
    EventQueue *eventq;
};

#endif //DVFSPOLICY_O3CORE_H
