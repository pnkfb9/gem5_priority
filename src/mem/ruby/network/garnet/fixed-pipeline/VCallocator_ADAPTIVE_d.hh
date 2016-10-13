#ifndef VCALLOCATOR_ADAPTIVE_D_HH
#define VCALLOCATOR_ADAPTIVE_D_HH  

#include <iostream>

#include "debug/RubyVOQ_VA.hh"
#include "VCallocator_d.hh"
#include "config/use_adaptive_routing.hh"

#include "mem/ruby/network/garnet/fixed-pipeline/flit_d.hh"

class Router_d;

//Speculative result struct
struct VCResult
{
    int invc;
    int inport;
    bool result;
};

class VCallocator_ADAPTIVE_d: public VCallocator_d
{
    public:
	VCallocator_ADAPTIVE_d(Router_d *router);
	virtual void init();
	virtual void arbitrate_outvcs();
	virtual void select_outvc(int inport_iter, int invc_iter);

	bool testNAVCA(int outport, InputUnit_d* inport,int invc_iter, int outvc);
	

    private:
	// single function to VA-outport stage, that takes the pktMod arg 
	// to select if it is a 
	// adp->adp, det->det det->adp adp->det 
	// VA-op stage.
	// Moreover the low and high vc are passed as the last two arguments
	VCResult  arbitrate_outvcs_all(int inport, int invc_offset,int outport_iter,int outvc_iter,int pktMod/*0-3*/,int in_low_Vc,int in_high_vc, int out_low_vc,int out_high_vc);

//	VCResult  arbitrate_outvcs_adaptive(int inport, int invc_offset,int outport_iter,int outvc_iter);
//	VCResult  arbitrate_outvcs_deterministic(int inport, int invc_offset,int outport_iter,int outvc_iter);
//	VCResult  arbitrate_outvcs_adaptive_to_deterministic(int inport, int invc_offset,int outport_iter,int outvc_iter);

//	void select_outvc_adaptive(int inport_iter, int invc_iter);
//	void select_outvc_deterministic(int inport_iter, int invc_iter);
//	void select_outvc_adaptive_to_deterministic(int inport_iter, int invc_iter);

	void select_outvc_all(int inport_iter, int invc_iter,int low_vc,int high_vc);

        // ADAPTIVE - First stage of arbitration
        // where all vcs select an output vc to contend for
	std::vector<std::vector<int> > m_round_robin_invc_adaptive;

	// ADAPTIVE - Arbiter for every output vc
        std::vector<std::vector<std::pair<int, int> > > m_round_robin_outvc_adaptive;

        // ADAPTIVE - [outport][outvc][inport][invc]
        // set true in the first phase of allocation
        //std::vector<std::vector<std::vector<std::vector<bool> > > > m_outvc_req_adaptive;

        //std::vector<std::vector<bool> > m_outvc_is_req_adaptive;
	int num_escape_path_vcs;

};
#endif

