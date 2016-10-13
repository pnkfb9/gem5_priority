#ifndef ROUTING_UNIT_ADAPTIVE_D_HH
#define ROUTING_UNIT_ADAPTIVE_D_HH

#include <string>
#include <map>
#include <utility>
#include "RoutingUnit_d.hh"

#include "debug/RubyVOQ_RC.hh"

class RoutingUnit_ADAPTIVE_d: public RoutingUnit_d
{
	public:
		RoutingUnit_ADAPTIVE_d(Router_d *router) :RoutingUnit_d(router){};

		//virtual int routeCompute(flit_d *t_flit);
		virtual void RC_stage(flit_d *t_flit, InputUnit_d *in_unit, int invc);
	private:
		int routeComputeAdaptive(flit_d *t_flit);		//YX for test purposes only

		// checks all the outlinks looking for one available one otherwise return
		// -1 and the packet is routed on the deterministic path
		int checkAdaptiveOutputLinkAvailable(flit_d*,InputUnit_d*,int invc);
		
		// checks all the outlinks, using a next hop less loaded inport, looking
		// for one  one otherwise return -1 and the packet is routed on the
		// deterministic path
		int checkLessLoadAdaptiveOutputLinkAvailable(flit_d*,InputUnit_d*,int invc);
		
		// checks all the outlinks, using a next frequency hop ordering, looking
		// for one available one otherwise return -1 and the packet is routed on
		// the deterministic path
		int checkFreqAdaptiveOutputLinkAvailable(flit_d*,InputUnit_d*,int invc);

		// computes the route using the XY deterministic routing algorithm
		int routeComputeDeterministic(flit_d *t_flit);

		int routeComputeOld(flit_d *t_flit);

		// it is the same in VCallocator_ADAPTIVE with the flit_d as additional
		// parameter, since at the time of RC the flit hasn't been written to
		// buf, yet.
		bool testNAVCA(int outport, InputUnit_d* inport,int invc_iter, int outvc,flit_d*);

};

#endif
