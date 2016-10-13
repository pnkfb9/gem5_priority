#ifndef ROUTING_UNIT_VOQ_D_HH
#define ROUTING_UNIT_VOQ_D_HH

#include <string>
#include <map>
#include <utility>
#include "RoutingUnit_d.hh"

#include "debug/RubyVOQ_RC.hh"

class RoutingUnit_VOQ_d: public RoutingUnit_d
{
	public:
	RoutingUnit_VOQ_d(Router_d *router)
		:RoutingUnit_d(router){};

	virtual int routeCompute(flit_d *t_flit);
	int nextRouteCompute(flit_d *t_flit);
	virtual void init();
	void printPartialTopology(std::ostream& out);
	virtual void RC_stage(flit_d *t_flit, InputUnit_d *in_unit, int invc);
	private:
		// additional tables to take care of next step routing. They will be
		// filled in during the init stage of the routing_unit_VOQ_d starting
		// from the routing tables from neighbors. One table per output port
		// reaching a router
		std::vector<std::vector<NetDest>> m_routing_table_next;
    	std::vector<std::vector<int>> m_weight_table_next;
		std::map<int,std::string> m_partial_topology;

};

#endif
