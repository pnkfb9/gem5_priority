/*
 * Copyright (c) 2014 Politecnico di Milano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Davide Zoni
 *
 * Modified: 
 *  2015-Feb-16 -> added NIC support in configuration to mimic NoCs w/o APNEA
 * 		for regression. Moreover, the initial state machine for the upstream router
 * 		to manage the pending responses has been implemented. Last, the
 * 		USE_APNEA_BASE flag has been added to scons and linked with the
 * 		actual_pg_policy. Assert traps the use of vnet-reuse with the apnea.
 *		The APNEA can now change the number of used VC on all routers and nic
 *		regardless the number of implemented ones. Regression tests has been
 *		done against baseline and vnet-reuse with 3,6 and 3,4,5,6 vcs. No
 *		regressions with navca has been done until now.
 *		TODO: -> clean up the code
 *			  -> implement the baseline policy	
 *
 *  2015-Feb-13 -> complete the basic configuration in the APNEA_BASE policy 
 *
 *	2015-Feb-12 -> added the APNEA_BASE policy structure
 */

#ifndef POWER_GATING_POLICY_HH
#define POWER_GATING_POLICY_HH

#include "Router_d.hh"
#include "Switch_d.hh"
#include "sim/core.hh"
#include <fstream>
#include <map>
#include <utility>

#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"

class PowerGatingPolicy
{
	public:
		static void addRouter(Router_d *router, EventQueue *eventq);
		virtual ~PowerGatingPolicy();

	protected:
		explicit PowerGatingPolicy(EventQueue *eventq);
		virtual void addRouterImpl(Router_d *router);
		EventQueue *eventq;
};


/* APNEA - Adaptive Power gating for NoC Buffers
 * The policy is used for the power gating methodology on buffers. Six lines are
 * added per each router pairs; three in each direction. They are the req+ re-
 * and ack, nack. 
 * The req+ and req- signal the downstream router to switch a new
 * buffer on due to the increase pressure on the upstream router for that OP.
 * The ack and the nack signals are used by the downstream router to signal the
 * success of the required operation to the upstream one (ack) or the failure of
 * such request.
 * 
 * For each OP the upstream router implements a state machine with two states:
 * idle: used when nothing has been requested
 * wait: a request has been done but not completed yet
 *
 * The upstream router sees a portion of the implemented VCs, the switched ON,
 * and can allocate only to them. Moreover, it can request to increase or
 * decrease such number of VCs by the req+ and req- to the downstream router.
 *
 * Using the done signal, the downstream can accomodate variable switch ON time,
 * or even negate the request due to reliability or additional issues.
 *
 * Advantages of this solution:
 * 1) Simplicity
 * 2) The downstream router is still in charge to satisfy the switch ON switch
 * OFF.
 * 3) NAVCA can be used for performance in a plain way.
 * 4) Both pkt-sw and VNET-reuse can be used for compress the traffic in few
 * VCs.
 * 5) A static remapping block can be used to face reliability issues. In
 * particular, the remap is valid starting from the buffer switch ON till its
 * switch OFF. Moreover, it allows to avoid the usage of faulty buffers.
 * 6) The methodology can be easily combined with DVFS for aggressive power
 * saving. However, resynch must be put on the additional control power gating
 * links to avoid metastability issues.
 *
 *
 * APNea generate events for other routers and processed events for the router
 * where it is running.
 * APnea is implemented with 3 event classes:
 * 1) main loop; it run at each cycle on all routers
 * 2) request class that schedules an event on a specific router. When processed
 * it imposes the decision of the destination router on the buffer switch
 * ON/OFF, then a response event is generated for the requesting router.
 * 3) response class schedules an event ack or nack for a requesting router that
 * is processed by the requestor. During the process the requesting router
 * change its APNea state for that OP and eventually the number of used VCs.
 *
 *
 * A change is required in the VA to support a variable number of VCs per OP
 * that can be updated at run-time. VC are always allocated from the first
 * available with lower ID to ease the higher id VC switch off.
 * CURRENT ISSUE 	- VC0 is always ON, no reliability 
 * 						(use remapper and recirculator).
 * 					 
*/
enum apneaSS_UP_type {APNEA_IDLE_UP	, APNEA_REQ_PLUS_UP	, APNEA_REQ_PLUS_PLUS_UP	, APNEA_REQ_MINUS_UP	, NUM_APNEASS_UP_TYPE};
enum apneaSS_DW_type {APNEA_IDLE_DW	, APNEA_WORKING_DW	, NUM_APNEASS_DW_TYPE_DW	};
#define APNEA_INIT_START_VC 0
#define APNEA_INIT_FINAL_VC 3

#define APNEA_INIT_START_VC_NIC 0
#define APNEA_INIT_FINAL_VC_NIC 3

class ApneaPGPolicy: public PowerGatingPolicy
{
	public:
		ApneaPGPolicy(EventQueue* eventq)
			: PowerGatingPolicy(eventq),epochTick(1000)
			{};
		virtual void addNicImpl(NetworkInterface_d* nic)
		{
			//check if NIC has already been inserted
			assert(	apneaNicMap.find(nic->get_id())==apneaNicMap.end());

			// ApneaBase: all the NIC are inserted, only for the initial
			// configuaration not managed during run-time	
			apneaNicMap[nic->get_id()] = nic;
					
			//configure all the structure and generate a single MainLoop event
			//for all the managed router
			eventq->schedule( new ApneaConfigurationEvent(this), curTick()+1 );

			configDone=false; //used to have a single mainLoop event during simulation
		}
		virtual void addRouterImpl(Router_d *router)
		{
			//check if router has already been inserted 	
			assert(	apneaRouterMap.find(router->get_id())==apneaRouterMap.end()); 
	
			// ApneaBase: all the routers are inserted	
			apneaRouterMap[router->get_id()] = router;
					
			//configure all the structure and generate a single MainLoop event
			//for all the managed routers
			eventq->schedule( new ApneaConfigurationEvent(this), curTick()+1 );

			configDone=false; //used to have a single mainLoop event during simulation
		}
		// for the inner classes to reschedule events
		EventQueue *getQueue() { return eventq; }

		bool configDone;
		
		std::map<int,Router_d*>& getApneaRouterMapRef(){ return apneaRouterMap;};
		std::map<int,NetworkInterface_d*>& getApneaNicMapRef(){ return apneaNicMap;};

		std::map<Router_d*,std::map<int,Router_d*>>& getApneaTopologyUpRef() { return apneaTopologyUp; };
		std::map<Router_d*,std::map<int,Router_d*>>& getApneaTopologyDwRef() { return apneaTopologyDw; };

		std::map<Router_d*,std::map<int,enum apneaSS_UP_type>>& getApneaStateMachineUpRef()	{ return apneaStateMachineUp;};
		std::map<Router_d*,std::map<int,enum apneaSS_DW_type>>& getApneaStateMachineDwRef()	{ return apneaStateMachineDw;};

		std::map<Router_d*,std::map<int,bool>>& getApneaPendingUpRef(){return apneaPendingUp;};
		std::map<Router_d*,std::map<int,bool>>& getApneaPendingDwRef(){return apneaPendingDw;};

	private:
	// POLICY MEMBER FUNCTIONS
		// print the complete APNEA PG POLICY configuration for a specific router
		void printConfiguration(std::ostream& out, int rId)
		{
			if(!configDone)
				out<<"@"<<curTick()<< "WARNING - configuration not completed, yet -"<<std::endl;

			//printing configuration
			assert(apneaRouterMap.find(rId)!=apneaRouterMap.end());
			std::map<int,Router_d*>::iterator it_r = apneaRouterMap.find(rId);
			Router_d* r =it_r->second;			

			out<<"----------------------------------------------------------------"<<std::endl;
			out<<" R"<<r->get_id()<<std::endl;
			out<<std::endl;
			std::map<int,Router_d*> tUp = apneaTopologyUp[r];
			std::map<int,Router_d*>::iterator it_tUp;
			out<<"\tTopology As Upstream (per OP): |";
			for(it_tUp = tUp.begin(); it_tUp!=tUp.end(); it_tUp++)
				out<<" OP"<<it_tUp->first<< " -> "<<" dwR"<<(it_tUp->second)->get_id()<<" |";	
			out<<std::endl;
			
			std::map<int,Router_d*> tDw = apneaTopologyDw[r];
			std::map<int,Router_d*>::iterator it_tDw;
			out<<"\tTopology As Downstream (per IP): |";
			for(it_tDw = tDw.begin(); it_tDw!=tDw.end(); it_tDw++)
				out<<" IP"<<it_tDw->first<< " -> "<<" upR"<<(it_tDw->second)->get_id()<<" |";
			out<<std::endl;
			out<<std::endl;

			std::map<int,enum apneaSS_UP_type> ssUp = apneaStateMachineUp[r];
			std::map<int,enum apneaSS_UP_type>::iterator it_ssUp;
			out<<"\tStateMachine As Upstream (per OP): |";
			for(it_ssUp = ssUp.begin(); it_ssUp != ssUp.end(); it_ssUp++ )
				out<<" OP"<<it_ssUp->first<<" State"<<it_ssUp->second<<" |";	
			out<<std::endl;
					
			std::map<int,enum apneaSS_DW_type> ssDw = apneaStateMachineDw[r];
			std::map<int,enum apneaSS_DW_type>::iterator it_ssDw;
			out<<"\tStateMachine As Downstream (per IP): |";
			for(it_ssDw = ssDw.begin(); it_ssDw != ssDw.end(); it_ssDw++ )
				out<<" OP"<<it_ssDw->first<<" State"<<it_ssDw->second<<" |";	
			out<<std::endl;
			out<<std::endl;
			std::map<int,bool> pendingUp = apneaPendingUp[r];
			std::map<int,bool>::iterator it_pendingUp;
			out<<"\tPending As Upstream (per OP): |";
			for(it_pendingUp=pendingUp.begin(); it_pendingUp!=pendingUp.end(); it_pendingUp++)
				out<<" OP"<<it_pendingUp->first<<" Pending? "<<it_pendingUp->second<<" |";
			out<<std::endl;

			std::map<int,bool> pendingDw = apneaPendingDw[r];
			std::map<int,bool>::iterator it_pendingDw;
			out<<"\tPending As Upstream (per IP): |";
			for(it_pendingDw=pendingDw.begin(); it_pendingDw!=pendingDw.end(); it_pendingDw++)
				out<<" OP"<<it_pendingDw->first<<" Pending? "<<it_pendingDw->second<<" |";
			out<<std::endl;
			out<<std::endl;		
			out<<"----------------------------------------------------------------"<<std::endl;
		};	
	// POLICY MEMBER VARIBLES
		
		// list of routers managed by the policy <routerId,router*>
		std::map<int,Router_d*> apneaRouterMap;

		//list of the NICs for the initial configuration on the usable outvc only!!!!
		std::map<int,NetworkInterface_d*> apneaNicMap;

		// topologies to schedule req+ req- events and answer ones. Mismatch
		// between IP and OP numbering
		std::map<Router_d*,std::map<int,Router_d*>> apneaTopologyUp;
		std::map<Router_d*,std::map<int,Router_d*>> apneaTopologyDw;

		//both upstream and downstream routers have their own state machine 
		std::map<Router_d*,std::map<int,enum apneaSS_UP_type>> apneaStateMachineUp;
		std::map<Router_d*,std::map<int,enum apneaSS_DW_type>> apneaStateMachineDw;

		// per router pending from UP and per router pending from DW
		std::map<Router_d*,std::map<int,bool>> apneaPendingUp;
		std::map<Router_d*,std::map<int,bool>> apneaPendingDw;

		//the periodic tick of the apnea main event loop. HINT: clockPeriod, i.e. 1 per cycle
		Tick epochTick; 

	// POLICY MEMBER EVENT CLASSES

		
		class ApneaConfigurationEvent: public Event
		{// scheduled only at the beginning. When all routers are collected
		 // builds the topology and initializes the state machines
			public:
				ApneaConfigurationEvent(ApneaPGPolicy* parent )
					:Event(Default_Pri,AutoDelete), parent(parent)
					{};
				void process()
				{
				#if USE_APNEA_BASE==0
						assert(false && "\n\n\nImpossible to use APNEA without enable the USE_APNEA_BASE flag\n\n\n");
				#else
					if(parent->configDone)
						// executed once to configure everything and to startup the mainLoop envet
						return;
					std::map<int,Router_d*>& rMap = parent->getApneaRouterMapRef();
					std::map<int,NetworkInterface_d*>& nicMap = parent->getApneaNicMapRef();

					std::map<Router_d*,std::map<int,Router_d*>>& rTopologyUp = parent->getApneaTopologyUpRef();
					std::map<Router_d*,std::map<int,Router_d*>>& rTopologyDw = parent->getApneaTopologyDwRef();

					std::map<Router_d*,std::map<int,enum apneaSS_UP_type>>& ssUp = parent->getApneaStateMachineUpRef();
					std::map<Router_d*,std::map<int,enum apneaSS_DW_type>>& ssDw  = parent->getApneaStateMachineDwRef();

					std::map<Router_d*,std::map<int,bool>>& pendingUp = parent->getApneaPendingUpRef();
					std::map<Router_d*,std::map<int,bool>>& pendingDw = parent->getApneaPendingDwRef();

					std::cout<<"@"<<curTick()<<" Configuring ApneaPGPolicy - ApneaConfigurationEvent::process -"<<std::endl;
					for(int i=0;i<rMap.size();i++)
					{
						//check all OPs to see the consumer
						Router_d* r = rMap[i];
						assert(r->get_id()==i); //got the correct router?	
						std::cout<<"Configuring R"<<r->get_id()<<"..."<<std::endl;

						//downstream topology, stateSpace (SS) and pendingReq 
						std::vector<InputUnit_d *>& ipVect = r->get_inputUnit_ref();
						for(int j=0;j<ipVect.size();j++)
						{
							// init for this router the pending DW vector to NO pending
							pendingDw[r].insert(std::make_pair(j,0));
							// init DW stateMachine to idle for all input port
							ssDw[r].insert(std::make_pair(j,APNEA_IDLE_DW));
						
							// make topology for downstream	
							OutputUnit_d* op_temp = dynamic_cast<OutputUnit_d*>(ipVect[j]->getInLink_d()->getLinkSource());
							if(op_temp!=NULL)
							{
								assert( dynamic_cast<Router_d*>(op_temp->get_router()) );
								rTopologyDw[r].insert(std::make_pair(j,op_temp->get_router()));
								// SET THE INITIAL NUMBER OF USED VCs between routers
								r->setInitVcOp(j,APNEA_INIT_START_VC);
								r->setFinalVcOp(j,APNEA_INIT_FINAL_VC);
							}
							else
							{// NIC not considered for now
								NetworkInterface_d* ni_temp = dynamic_cast<NetworkInterface_d*>(ipVect[j]->getInLink_d()->getLinkSource());
								assert(ni_temp);
								std::cout<<"R"<<r->get_id()<<" IP"<<j<<" sources from a NIC"<<std::endl;
								ni_temp->setInitVcOp(APNEA_INIT_START_VC_NIC);
								ni_temp->setFinalVcOp(APNEA_INIT_FINAL_VC_NIC);

							}
						}

						//upstream topology  (Rup.OP -> Rwd.IP): stateSpace (SS) and pendingAck
						std::vector<OutputUnit_d *>& opVect = r->get_outputUnit_ref();
						for(int j=0;j<opVect.size();j++)
						{
							// init for this router the Up pending vector to NO pending
							pendingUp[r].insert(std::make_pair(j,0));
							// init UP stateMachine to idle for all outport port
							ssUp[r].insert(std::make_pair(j,APNEA_IDLE_UP));
						
							// make topology for upstream	
							InputUnit_d* ip_temp = dynamic_cast<InputUnit_d*>(opVect[j]->getOutLink_d()->getLinkConsumer());
							if(ip_temp!=NULL)
							{
								assert( dynamic_cast<Router_d*>(ip_temp->get_router()) );
								rTopologyUp[r].insert(std::make_pair(j,ip_temp->get_router()));
								// SET THE INITIAL NUMBER OF USED VCs between routers
								r->setInitVcOp(j,APNEA_INIT_START_VC);
								r->setFinalVcOp(j,APNEA_INIT_FINAL_VC);
							}
							else
							{// NIC not considered for now
								NetworkInterface_d* ni_temp = dynamic_cast<NetworkInterface_d*>(opVect[j]->getOutLink_d()->getLinkConsumer());
								assert(ni_temp);
								std::cout<<"R"<<r->get_id()<<" OP"<<j<<" destination to a NIC"<<std::endl;								
								//however set the used outvc properly!!!!
								r->setInitVcOp(j,APNEA_INIT_START_VC);
								r->setFinalVcOp(j,APNEA_INIT_FINAL_VC);
								// SET THE INITIAL NUMBER OF USED VCs between routers and NICs
								ni_temp->setInitVcOp(APNEA_INIT_START_VC_NIC);
								ni_temp->setFinalVcOp(APNEA_INIT_FINAL_VC_NIC);
							}
						}

						std::cout<<" DONE"<<std::endl;					
					}
					std::cout<<"@"<<curTick()<<" Configuring ApneaPGPolicy ... DONE"<<std::endl;	
					// configuration done. do NOT process the event again
					parent->configDone=true;

					for(int i=0;i<rMap.size();i++)
					{//check the configuration
						Router_d* r = rMap[i];
						assert(r->get_id()==i); //got the correct router?
						parent->printConfiguration(std::cout,r->get_id());				
					}
					// schedule the first main loop event (NOTE -1tick to gain
					// what lost in the configuration and stay aligned with the
					// "clock")
					parent->getQueue()->schedule( new ApneaMainEvent(parent), curTick()+parent->epochTick-1);
				#endif
				};
			private:
				ApneaPGPolicy *parent;

		};

		class ApneaMainEvent: public Event
		{	// main loop event of the Apnea power gating policy running at fiixed
			// frequency.

			public:
				ApneaMainEvent(ApneaPGPolicy *parent)
					:Event(Default_Pri,AutoDelete), parent(parent)
					{};

				virtual void process()
				{
				#if USE_APNEA_BASE==1
			//		std::cout<<std::endl<<"@"<<curTick()<<" ApneaMainEvent::process()"<<std::endl;	
					
			//		// scan all routers to generate upstream requests
					std::map<int,Router_d*>& rMap = parent->getApneaRouterMapRef();

					std::map<Router_d*,std::map<int,Router_d*>>& rTopologyUp = parent->getApneaTopologyUpRef();
					std::map<Router_d*,std::map<int,Router_d*>>& rTopologyDw = parent->getApneaTopologyDwRef();

					std::map<Router_d*,std::map<int,enum apneaSS_UP_type>>& ssUp = parent->getApneaStateMachineUpRef();
					std::map<Router_d*,std::map<int,enum apneaSS_DW_type>>& ssDw  = parent->getApneaStateMachineDwRef();

					std::map<Router_d*,std::map<int,bool>>& pendingUp = parent->getApneaPendingUpRef();
					std::map<Router_d*,std::map<int,bool>>& pendingDw = parent->getApneaPendingDwRef();
					
					for(int i=0;i<rMap.size();i++)
					{
						Router_d* r = rMap[i];
						assert(r->get_id()==i); //got the correct router?	
						std::cout<<"\tR"<<r->get_id()<<"...";
			  		
						//upstream (Rup.OP -> Rwd.IP)
						std::vector<OutputUnit_d *>& opVect = r->get_outputUnit_ref();
						std::map<int,bool> opPendingUp_map = (pendingUp.find(r))->second;
						std::map<int,enum apneaSS_UP_type> opSSUp_map = (ssUp.find(r))->second;

						for(int j=0;j<opVect.size();j++)
						{//if event is pending only get it without do anything else
							if((opPendingUp_map.find(j))->second)
							{//event is pending on router.op (r,j)

								//check SSup
								assert( (opSSUp_map.find(j))->second != APNEA_IDLE_UP 
										&& (opSSUp_map.find(j))->second != NUM_APNEASS_UP_TYPE );
								//modify the used VCs set in the VA for this OP
								if( (opSSUp_map.find(j))->second == APNEA_REQ_PLUS_UP )
								{//increase by one the the usable VCs
									r->incFinalVcOp(j);
									// back to idle state
									opSSUp_map[j] = APNEA_IDLE_UP;
								}
								else if( (opSSUp_map.find(j))->second == APNEA_REQ_PLUS_PLUS_UP )
								{
									r->incFinalVcOp(j);
									r->incFinalVcOp(j);
									// back to idle state
									opSSUp_map[j] = APNEA_IDLE_UP;
								}
								else if( (opSSUp_map.find(j))->second == APNEA_REQ_MINUS_UP )
								{//nothing since decremented before. Only return to IDLE to schedule other events
									// back to idle state
									opSSUp_map[j] = APNEA_IDLE_UP;
								}
								else
									assert(false&& "\n\nSS_UP_ State Not Managed\n\n");

							}
							else
							{// No pending, try to schedule a req event
								if( (opSSUp_map.find(j))->second == APNEA_IDLE_UP);
								{// check the pressure on the VA stage

								}
								//else	
								// Perhaps it is waiting for an already requested event
							}

						}
				//		//downstream ( Rup.IP -> Rwd.OP | Rup.IP -> {BUF_ON|BUF_OFF} )
				//		std::vector<InputUnit_d *>& ipVect = r->get_inputUnit_ref();
				//		for(int j=0;j<ipVect.size();j++)
				//		{
				//		}

					}
			//		//reschedule the main event of apnea
					parent->getQueue()->schedule ( new ApneaMainEvent(parent), curTick()+parent->epochTick );
				#endif
				}
			private:
				ApneaPGPolicy* parent;
				// split the process in two parts; upstream and downstream. Both
				// eventGeneration and pendingStatus are analyzed in both
				// functions
		//		void processUpstreamOp(Router_d* r, OutputUnit_d* op, int opId, )
		//		{
		//		};
		//		void processDownstreamIp(int inport)
		//		{
		//		};
		};

};
/*
	This is the first power gating policy using the new fashion model to
	schedule events. It resembles the DFS module to uniform the code

	This policy acts on the switch only, using a threshold approach
*/
class FirstPowerGatingPolicy: public PowerGatingPolicy
{
	public:
		std::ofstream dump; 
		static const uint32_t threshold_high=10;
		static const uint32_t threshold_low=2;
		FirstPowerGatingPolicy(EventQueue *eventq)
			: PowerGatingPolicy(eventq),dump("m5out/dump_pg.txt")
		{};

		virtual void addRouterImpl(Router_d *router)
		{
			// this policy works for a single router only
			if(router->get_id()==3)
			{
				dump<<"start_tick\tend_tick\tfreq\ttot_power\tdyn_power"
                 "\tstat_power\tsw_is_on\tcong_r"<<std::endl;

				// fire the first event 
				eventq->schedule(new FirstPowerGatingEvent(this, router,0,0),initialDelay);
			}
		}
		EventQueue *getQueue() { return eventq; }


	private:

		class ApplyFirstPowerGatingEvent: public Event
		{
		//apply the actuator and evaluate power	
			public:
				ApplyFirstPowerGatingEvent(FirstPowerGatingPolicy *parent, Router_d* router,bool sw_state)
						:Event(Default_Pri,AutoDelete),parent(parent),r(router),apply_sw_state(sw_state){};
				void process()
				{
					//once here apply the actuation on the switch then compute
					//power to avoid power computation when on-off event
					//happened in the between

					//std::cout<<"\t\t@"<<curTick()<<" APPLYING STATE: "<<r->getActualPowerStateSW(0)<<"->"<<apply_sw_state;
					r->printSWPower(parent->dump);

					//switch the crossbar to the new state
					r->getSwitch()->setPowerState(apply_sw_state);
					//std::cout<<" FINAL STATE: "<<r->getSwitch()->getPowerState()<<std::endl;
				}
			private:
				FirstPowerGatingPolicy* parent;
				Router_d *r;
				bool apply_sw_state;
		};
		class FirstPowerGatingEvent: public Event
		{
			public:
				FirstPowerGatingEvent(FirstPowerGatingPolicy *parent, Router_d* router, int count, uint32_t timeout)
					:Event(Default_Pri,AutoDelete), parent(parent), r(router), count(count), timeout(timeout){};

				virtual void process()
				{

					// here the actual power gating policy
					const uint32_t limit_timeout=1000000; //I must switch the crossbar on

					const uint32_t limit_to_change=5*samplePeriod;
					const uint32_t cmd_delay_ON=1000;
					const uint32_t cmd_delay_OFF=1;
					
					if(count>=limit_to_change)
            		{
                		count=0;
						
						uint32_t actual_congestion = r->getCongestion();
						bool sw_actual_state = r->getActualPowerStateSW(0);
						bool sw_next_state = 0;

						if(actual_congestion >= parent->threshold_high)
							// switch on the crossbar
							sw_next_state=1;	
						else if(parent->threshold_low >= actual_congestion)
							// switch off the crossbar
							sw_next_state=0;	
						else
							sw_next_state = sw_actual_state;

						// set timeout to avoid starvation
						timeout+=samplePeriod;
						if(timeout>=limit_timeout)
						{
							timeout=0;
							sw_next_state=1;
						}
						else if(sw_actual_state==1)
							timeout=0;
							
			
						//std::cout<<"@"<<curTick()
						//		<<" power_gating_policy state: "<<sw_actual_state
						//		<< " next_state: "<<sw_next_state<<std::endl;
						// actually schedule to change state
						
						if(sw_next_state!=sw_actual_state)
						{
							// this is the old way but schedules on the router
							// frequency basis, we want something more accurate
							//r->setPowerStateSW(sw_next_state, cmd_delay);
							
							uint32_t cmd_delay=cmd_delay_OFF;
							if(sw_next_state)
								cmd_delay=cmd_delay_ON;

							parent->getQueue()->schedule
										(
											new ApplyFirstPowerGatingEvent(parent,r,sw_next_state),
											curTick()+cmd_delay
										);
						}
					}
					r->printSWPower(parent->dump);
					// schedule a new event
					parent->getQueue()->schedule
							(
								new
								FirstPowerGatingEvent(parent,r,count+samplePeriod,timeout),
								curTick()+parent->samplePeriod
							);
				}
			private:
				FirstPowerGatingPolicy* parent;
				Router_d *r;
				int count;
				uint32_t timeout;
		};
		static const uint32_t initialDelay=10000; //10ns
		static const uint32_t samplePeriod=10000; //10ns
};


/**
 * This is used to collect data on buffer usage without actuations
 */
class AllBufferSensingNoPwrGating : public PowerGatingPolicy
{
public:
    
	std::map<int,std::ofstream*> dump_f;

    EventQueue *getQueue() { return eventq; }
    
    AllBufferSensingNoPwrGating(EventQueue *eventq) : PowerGatingPolicy(eventq) {}
	
	~AllBufferSensingNoPwrGating()
	{
		for(std::map<int,std::ofstream*>::iterator it=dump_f.begin(); it!=dump_f.end(); it++)
		{
			if(it->second!=NULL)
				it->second->close();	
		}
		dump_f.clear();
	}

    virtual void addRouterImpl(Router_d *router)
    {
        using namespace std;
        //if(router->get_id()==5||router->get_id()==0)
		// add the router to be managed here
        {
			std::stringstream str_pwr;
			str_pwr<<"m5out/dump_buf_usage_router"<<router->get_id()<<".txt";

			dump_f[router->get_id()]=new ofstream(str_pwr.str());
			assert((*dump_f[router->get_id()]).is_open());
        }
		// one event for each router even if not added FIXME
        eventq->schedule(new PowerGatingEvent(router,this),initialDelay);
    }
    
private:
    class PowerGatingEvent : public Event
    {
    public:
        PowerGatingEvent(Router_d *r, AllBufferSensingNoPwrGating *parent) 
				: Event(Default_Pri, AutoDelete), r(r), parent(parent) {}
        
        virtual void process()
        {
			const int samplePeriod=100000; //100000ps=100ns

			if(r->get_id()==5||r->get_id()==0)
        	{// add the router to be managed here
				std::map<int,std::ofstream*>::iterator it_pwr = parent->dump_f.find(r->get_id());
				r->computeAndPrintBufUsageRunTime(*(it_pwr->second));
			}
            parent->getQueue()->schedule(new PowerGatingEvent(r,parent), curTick()+samplePeriod);
        }
    private:
        Router_d *r;
        AllBufferSensingNoPwrGating *parent;
    };
    
    static const Tick initialDelay=10000ull;
};


#endif
