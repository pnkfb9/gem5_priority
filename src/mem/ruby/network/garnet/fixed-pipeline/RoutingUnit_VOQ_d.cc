#include <iostream>
#include <cassert>
#include <string>

#include "RoutingUnit_VOQ_d.hh"

#include "base/cast.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/RoutingUnit_d.hh"
#include "mem/ruby/slicc_interface/NetworkMessage.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"


int
RoutingUnit_VOQ_d::routeCompute(flit_d *t_flit)
{
    MsgPtr msg_ptr = t_flit->get_msg_ptr();
    NetworkMessage* net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
    NetDest msg_destination = net_msg_ptr->getInternalDestination();

/* compute this route*/
    int output_link = -1;
    int min_weight = INFINITE_;

    for (int link = 0; link < m_routing_table.size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) {
            if (m_weight_table[link] >= min_weight)
                continue;
            output_link = link;
            min_weight = m_weight_table[link];
        }
    }

    if (output_link == -1) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }
/* compute the next route */
	int output_link_next = -1;
    int min_weight_next = INFINITE_;

    for (int link_next = 0; link_next < m_routing_table_next[output_link].size(); link_next++) {
        if(msg_destination.intersectionIsNotEmpty(m_routing_table_next.at(output_link)[link_next])) {
            if (m_weight_table_next.at(output_link)[link_next] >= min_weight_next)
                continue;
            output_link_next = link_next;
            min_weight_next = m_weight_table_next.at(output_link)[link_next];
        }
    }

    DPRINTF(RubyVOQ_RC, "@%lld Router_%d->RC_VOQ dest %s outport: %d next_outport: %d ",
								curTick(),m_router->get_id(),msg_destination,output_link,output_link_next);
	if (output_link_next == -1) 
	{
		if(m_routing_table_next.at(output_link).size()==0)
			DPRINTF(RubyVOQ_RC,"No next hop\n");
		else
        {
			fatal("Fatal Error:: No Route exists from this Router.");
        	exit(0);
		}
    }
	else
	{
		DPRINTF(RubyVOQ_RC,"\n");
	}
    return output_link;
}

int
RoutingUnit_VOQ_d::nextRouteCompute(flit_d *t_flit)
{
    MsgPtr msg_ptr = t_flit->get_msg_ptr();
    NetworkMessage* net_msg_ptr = safe_cast<NetworkMessage *>(msg_ptr.get());
    NetDest msg_destination = net_msg_ptr->getInternalDestination();

/* compute this route*/
    int output_link = -1;
    int min_weight = INFINITE_;

    for (int link = 0; link < m_routing_table.size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) {
            if (m_weight_table[link] >= min_weight)
                continue;
            output_link = link;
            min_weight = m_weight_table[link];
        }
    }

    if (output_link == -1) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }
/* compute the next route */
	int output_link_next = -1;
    int min_weight_next = INFINITE_;

    for (int link_next = 0; link_next < m_routing_table_next[output_link].size(); link_next++) {
        if(msg_destination.intersectionIsNotEmpty(m_routing_table_next.at(output_link)[link_next])) {
            if (m_weight_table_next.at(output_link)[link_next] >= min_weight_next)
                continue;
            output_link_next = link_next;
            min_weight_next = m_weight_table_next.at(output_link)[link_next];
        }
    }

	if (output_link_next == -1) 
	{
		if(m_routing_table_next.at(output_link).size()!=0)
		{
			fatal("Fatal Error:: No Route exists from this Router.");
        	exit(0);
		}
	}
    return output_link_next;

}


void RoutingUnit_VOQ_d::init()
{
	//	retrive routing tables from neighbors starting from output links
	std::vector<OutputUnit_d *>& outports=m_router->get_outputUnit_ref();

	uint32_t numNeighborRouter=0;
	// we resize a routing next table per each link while some of them are
	// directed to end nodes thus will be empty for the next_routing_table
	m_routing_table_next.resize(m_routing_table.size());
	m_weight_table_next.resize(m_routing_table.size());

	// we should have 1 outport per outlink
	assert(m_routing_table.size()==outports.size());

	//std::cout<<"Router"<<m_router->get_id()<<std::endl;
	for(int i=0;i<outports.size();i++)
	{
		Consumer* o = outports.at(i)->getOutLink_d()->getLinkConsumer();
		// test the outputlink numbering
		//	std::cout<<"\t"<<i<<" " <<o->getObjectId()<<std::endl;
		m_partial_topology.insert(std::make_pair(i,o->getObjectId()));	

		if(o->getObjectId().find("Router")!=std::string::npos)
		{
			
			//std::cout<<"\t" <<o->getObjectId()<<std::endl;
			ClockedObject *obj = outports.at(i)->getOutLink_d()->getLinkConsumer()->getClockedObject();
			numNeighborRouter++;
			// copy both the routing table and the weights table of the next router here
			Router_d* next_r =  dynamic_cast<Router_d*>(obj);
			
			// test link between two routers
			//std::cout<<"Router"<<m_router->get_id()
			//		<<" output_link_"<<i
			//		<<" NextRouter"<<next_r->get_id()<<std::endl;
			
			assert(next_r!=0);
			std::vector<NetDest>& nextRoutingTable = next_r->getRoutingTable();
			std::vector<int>& nextRoutingTableWeights = next_r->getRoutingTableWeights();
			for(int j=0;j<nextRoutingTable.size();j++)
			{
				 m_routing_table_next.at(i).push_back(nextRoutingTable.at(j));
				 m_weight_table_next.at(i).push_back(nextRoutingTableWeights.at(j));
			}
		}
	}
	printPartialTopology(std::cout);
}

void
RoutingUnit_VOQ_d::printPartialTopology(std::ostream& out)
{
	out<<"Router"<<m_router->get_id()<<std::endl;
	for(std::map<int,std::string>::iterator i = m_partial_topology.begin();
									i!=m_partial_topology.end();i++)
	{
		out<<"\toutlink_"<<i->first<<" nextHop: "<<i->second<<std::endl;
	}
}

void
RoutingUnit_VOQ_d::RC_stage(flit_d *t_flit, InputUnit_d *in_unit, int invc)
{
    int outport = routeCompute(t_flit);
    in_unit->updateRoute(invc, outport, m_router->curCycle());
	int next_outport = nextRouteCompute(t_flit);
	in_unit->updateNextRoute(invc, next_outport);
    t_flit->advance_stage(VA_, m_router->curCycle());
    m_router->vcarb_req();
}


