#ifndef VCALLOCATOR_VOQ_D_HH
#define VCALLOCATOR_VOQ_D_HH  

#include "debug/RubyVOQ_VA.hh"
#include "VCallocator_d.hh"

class Router_d;

class VCallocator_VOQ_d: public VCallocator_d
{
	public:
		VCallocator_VOQ_d(Router_d *router)
			:VCallocator_d(router){};
    	//virtual void arbitrate_outvcs();
		virtual void select_outvc(int inport_iter, int invc_iter);
	private:
		// helper functions to implement the real behavior for vc allocation
		void baselineVA(int inport_iter, int invc_iter);
		void baselineVOQ(int inport_iter, int invc_iter, int outport, int next_outport, int next_r_outports);
		void VOQ(int inport_iter, int invc_iter, int outport, int next_outport, int next_r_outports, int next_r_num_local_ports);
		void VOQ_DBBM(int inport_iter, int invc_iter, int outport, int next_outport, int next_r_outports, int next_r_num_local_ports);
		void DBBM(int inport_iter, int invc_iter, int outport, int next_outport, int next_r_outports);
};

#endif

