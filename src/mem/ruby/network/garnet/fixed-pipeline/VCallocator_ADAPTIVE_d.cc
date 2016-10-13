#include "VCallocator_ADAPTIVE_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/InputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/OutputUnit_d.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"


#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"

#include "mem/ruby/slicc_interface/NetworkMessage.hh"
#include "mem/ruby/network/garnet/NetworkHeader.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"


VCallocator_ADAPTIVE_d::VCallocator_ADAPTIVE_d(Router_d *router)
			:VCallocator_d(router),num_escape_path_vcs(0)
			{
				std::cout<<" VA_ USING ADAPTIVE ROUTING"<<std::endl;
				// at least 2vcs to use the adaptive routing
				//assert(m_vc_per_vnet>1);
			};

void
VCallocator_ADAPTIVE_d::init()
{
    m_input_unit = m_router->get_inputUnit_ref();
    m_output_unit = m_router->get_outputUnit_ref();

    m_num_inports = m_router->get_num_inports();
    m_num_outports = m_router->get_num_outports();
    m_round_robin_invc.resize(m_num_inports);
    m_round_robin_outvc.resize(m_num_outports);
    m_outvc_req.resize(m_num_outports);
    m_outvc_is_req.resize(m_num_outports);
    
	m_round_robin_invc_adaptive.resize(m_num_inports);
    m_round_robin_outvc_adaptive.resize(m_num_outports);
    //m_outvc_req_adaptive.resize(m_num_outports);
    //m_outvc_is_req_adaptive.resize(m_num_outports);

    for (int i = 0; i < m_num_inports; i++) 
	{
        m_round_robin_invc[i].resize(m_num_vcs);
		m_round_robin_invc_adaptive[i].resize(m_num_vcs);
        for (int j = 0; j < m_num_vcs; j++) 
		{
            m_round_robin_invc[i][j] = 0;
			m_round_robin_invc_adaptive[i][j] = 0;

        }
    }

    for (int i = 0; i < m_num_outports; i++) 
	{
        m_round_robin_outvc[i].resize(m_num_vcs);
        m_outvc_req[i].resize(m_num_vcs);
        m_outvc_is_req[i].resize(m_num_vcs);
        
		m_round_robin_outvc_adaptive[i].resize(m_num_vcs);
        //m_outvc_req_adaptive[i].resize(m_num_vcs);
        //m_outvc_is_req_adaptive[i].resize(m_num_vcs);
		
        for (int j = 0; j < m_num_vcs; j++) {
            m_round_robin_outvc[i][j].first = 0;
            m_round_robin_outvc[i][j].second = 0;
            m_outvc_is_req[i][j] = false;
            
			m_round_robin_outvc_adaptive[i][j].first = 0;
            m_round_robin_outvc_adaptive[i][j].second = 0;
            //m_outvc_is_req_adaptive[i][j] = false;
            
			m_outvc_req[i][j].resize(m_num_inports);
			//m_outvc_req_adaptive[i][j].resize(m_num_inports);

            for (int k = 0; k < m_num_inports; k++) 
			{
                m_outvc_req[i][j][k].resize(m_num_vcs);
				//m_outvc_req_adaptive[i][j][k].resize(m_num_vcs);

                for (int l = 0; l < m_num_vcs; l++) {
					m_outvc_req[i][j][k][l] = false;
                    //m_outvc_req_adaptive[i][j][k][l] = false;
                }
            }
        }
    }
	num_escape_path_vcs=(m_router->get_net_ptr())->getAdaptiveRoutingNumEscapePaths();

	#if USE_SPECULATIVE_VA_SA==1
             spec_SWallocator_startup();	
	#endif
}


void
VCallocator_ADAPTIVE_d::arbitrate_outvcs()
{

    VCResult VCResSet;
	// FADP->FADP (pktMod==1)
    for (int outport_iter = 0; outport_iter < m_num_outports; outport_iter++) 
	{
        for (int outvc_iter = 0; outvc_iter < m_num_vcs; outvc_iter++) 
		{

			if (!m_outvc_is_req[outport_iter][outvc_iter]) 
			{
                // No requests for this outvc in this cycle
				//std::cout<<"\tNO outport_iter"<<outport_iter
				//			<<" outvc_iter"<<outvc_iter<<std::endl;
                continue;
            }
			int inport = m_round_robin_outvc[outport_iter][outvc_iter].first;
            int invc_offset = m_round_robin_outvc[outport_iter][outvc_iter].second;
		
			int low_vc,high_vc;
			low_vc=0;
			high_vc=m_vc_per_vnet-num_escape_path_vcs;
			VCResSet = arbitrate_outvcs_all(inport,invc_offset,outport_iter,outvc_iter,1,low_vc,high_vc,low_vc,high_vc);


				//VCResSet = arbitrate_outvcs_adaptive(inport, invc_offset,outport_iter,outvc_iter);
		#if USE_SPECULATIVE_VA_SA==1
			bool spec_succesful = false;
			if(VCResSet.result == true)
			{
				     //Win VCallocation adaptive, try speculation

				spec_succesful = spec_SWallocator(VCResSet.inport, VCResSet.invc, outport_iter, outvc_iter);	
			   	if(spec_succesful)
			   	{					
			   		m_router->print_pipeline_state(m_router->spec_get_switch_buffer_flit(VCResSet.inport), "VA_SPEC_WIN");
			   	}
			   	else
			   	{
					m_router->print_pipeline_state(m_input_unit[VCResSet.inport]->peekTopFlit(VCResSet.invc), "VA_SPEC_LOSE");
			   	}
			} 
			if(!spec_succesful)
				m_router->swarb_req(); // not sure if do this to save 1 cycle		
		#endif	
		}
    }
	// DET->DET (pktMod==0)
    for (int outport_iter = 0; outport_iter < m_num_outports; outport_iter++) 
	{
        for (int outvc_iter = 0; outvc_iter < m_num_vcs; outvc_iter++) 
		{

			if (!m_outvc_is_req[outport_iter][outvc_iter]) 
			{
                // No requests for this outvc in this cycle
				//std::cout<<"\tNO outport_iter"<<outport_iter
				//			<<" outvc_iter"<<outvc_iter<<std::endl;
                continue;
            }
			int inport = m_round_robin_outvc[outport_iter][outvc_iter].first;
            int invc_offset = m_round_robin_outvc[outport_iter][outvc_iter].second;
		
			int low_vc,high_vc;
			low_vc=m_vc_per_vnet-num_escape_path_vcs;
			high_vc=m_vc_per_vnet;
			VCResSet = arbitrate_outvcs_all(inport,invc_offset,outport_iter,outvc_iter,0,low_vc,high_vc,low_vc,high_vc);
			//VCResSet = arbitrate_outvcs_deterministic(inport,invc_offset,outport_iter,outvc_iter);
		#if USE_SPECULATIVE_VA_SA==1	
			bool spec_succesful = false;
			if(VCResSet.result == true)
			{
			     //Win VCallocation adaptive, try speculation
				spec_succesful = spec_SWallocator(VCResSet.inport, VCResSet.invc, outport_iter, outvc_iter);	
				if(spec_succesful)
			    {					
			    	m_router->print_pipeline_state(m_router->spec_get_switch_buffer_flit(VCResSet.inport), "VA_SPEC_WIN");
			    }
			    else
			    {
			 		m_router->print_pipeline_state(m_input_unit[VCResSet.inport]->peekTopFlit(VCResSet.invc), "VA_SPEC_LOSE");
			    }
			} 
			if(!spec_succesful)
				m_router->swarb_req(); // not sure if do this to save 1 cycle		
		#endif
			//}
			//else if (is_adaptive==2)
			//{
			//	std::cout<<"@"<<curTick()<<"R"<<m_router->get_id()
			//			<<" arbitrate_outvcs  deterministic outport_iter("
			//			<<outport_iter<<") outvc_iter("<<outvc_iter<<")"<<std::endl;	
			//	arbitrate_outvcs_adaptive_to_deterministic(inport,invc_offset,outport_iter,outvc_iter);
			//}
				
			
				
		}
    }
	//FADP->DET (pktMod==2)
	for (int outport_iter = 0; outport_iter < m_num_outports; outport_iter++) 
	{
        for (int outvc_iter = 0; outvc_iter < m_num_vcs; outvc_iter++) 
		{
			for (int inport_iter = 0; inport_iter < m_num_outports; inport_iter++) 
			{
        		for (int invc_iter = 0; invc_iter < m_num_vcs; invc_iter++) 
				{
					flit_d* temp_flit=m_input_unit[inport_iter]->peekTopFlit(invc_iter);
					if(temp_flit!=NULL)
					  if(temp_flit->getIsAdaptive()==2)
					  {
							int in_low_vc,in_high_vc;	
							in_low_vc=0;
							in_high_vc=m_vc_per_vnet-num_escape_path_vcs;
							//deterministic routing uses the upper vc per vnet
							int out_low_vc,out_high_vc;
							out_low_vc=m_vc_per_vnet-num_escape_path_vcs;
							out_high_vc=m_vc_per_vnet;	

						VCResSet = arbitrate_outvcs_all(inport_iter,invc_iter,outport_iter,outvc_iter,2,in_low_vc,in_high_vc,out_low_vc,out_high_vc);
						//VCResSet = arbitrate_outvcs_adaptive_to_deterministic(inport_iter,invc_iter,outport_iter,outvc_iter);
						#if USE_SPECULATIVE_VA_SA==1
						bool spec_succesful = false;
				  		if(VCResSet.result == true){
				     		//Win VCallocation adaptive, try speculation
				    			 spec_succesful = spec_SWallocator(VCResSet.inport, VCResSet.invc, outport_iter, outvc_iter);	
				    		    if(spec_succesful)
						     {					
							m_router->print_pipeline_state(m_router->spec_get_switch_buffer_flit(VCResSet.inport), "VA_SPEC_WIN");
						     }
						     else
						     {
							m_router->print_pipeline_state(m_input_unit[VCResSet.inport]->peekTopFlit(VCResSet.invc), "VA_SPEC_LOSE");
						     }
				  		}
						if(!spec_succesful)
							m_router->swarb_req(); // not sure if do this to save 1 cycle	 
						#endif	
					  }
				}
			}
		}
	}
	#if FADP_BACK_DET_TO_FADP_VC == 1	

	//DET->FADP (pktMod==3)
	for (int outport_iter = 0; outport_iter < m_num_outports; outport_iter++) 
	{
        for (int outvc_iter = 0; outvc_iter < m_num_vcs; outvc_iter++) 
		{
			for (int inport_iter = 0; inport_iter < m_num_outports; inport_iter++) 
			{
        		for (int invc_iter = 0; invc_iter < m_num_vcs; invc_iter++) 
				{
					flit_d* temp_flit=m_input_unit[inport_iter]->peekTopFlit(invc_iter);
					if(temp_flit!=NULL)
					  if(temp_flit->getIsAdaptive()==3)
					  {
							int in_low_vc,in_high_vc;	
							in_low_vc=m_vc_per_vnet-num_escape_path_vcs;
							in_high_vc=m_vc_per_vnet;
							//deterministic routing uses the upper vc per vnet
							int out_low_vc,out_high_vc;
							out_low_vc=0;
							out_high_vc=m_vc_per_vnet-num_escape_path_vcs;	

						VCResSet = arbitrate_outvcs_all(inport_iter,invc_iter,outport_iter,outvc_iter,3,in_low_vc,in_high_vc,out_low_vc,out_high_vc);
						//VCResSet = arbitrate_outvcs_adaptive_to_deterministic(inport_iter,invc_iter,outport_iter,outvc_iter);
						#if USE_SPECULATIVE_VA_SA==1
						bool spec_succesful = false;
				  		if(VCResSet.result == true){
				     		//Win VCallocation adaptive, try speculation
				    			 spec_succesful = spec_SWallocator(VCResSet.inport, VCResSet.invc, outport_iter, outvc_iter);	
				    		    if(spec_succesful)
						     {					
							m_router->print_pipeline_state(m_router->spec_get_switch_buffer_flit(VCResSet.inport), "VA_SPEC_WIN");
						     }
						     else
						     {
							m_router->print_pipeline_state(m_input_unit[VCResSet.inport]->peekTopFlit(VCResSet.invc), "VA_SPEC_LOSE");
						     }
				  		}
						if(!spec_succesful)
							m_router->swarb_req(); // not sure if do this to save 1 cycle	 
						#endif	
					  }
				}
			}
		}
	}
	#endif// DET->FADP

}

void
VCallocator_ADAPTIVE_d::select_outvc(int inport_iter, int invc_iter)
{
	// set the vc available if the flit is adaptive or deterministic routing //
//	int low_vc,high_vc;
//	low_vc=0;
//	high_vc=m_vc_per_vnet-num_escape_path_vcs;
	flit_d* temp_flit = m_input_unit[inport_iter]->peekTopFlit(invc_iter);
	assert(temp_flit && "Flit not present in VCallocator_ADAPTIVE_d::select_outvc"); // the flit should be present here, just to be sure

	int isFlitAdaptive=temp_flit->getIsAdaptive();
//	int isAdaptive=m_input_unit[inport_iter]->getIsAdaptive(invc_iter);

	int low_vc,high_vc;


	//if(isAdaptive==1)
	if(isFlitAdaptive==1)
	{
	//	std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()
	//			<<" VA adaptive inport_iter("<<inport_iter
	//				<<"), invc_iter("<<invc_iter<<")"<<std::endl;
		low_vc=0;
		high_vc=m_vc_per_vnet-num_escape_path_vcs;
		select_outvc_all(inport_iter, invc_iter, low_vc, high_vc);
	}	
	else if (isFlitAdaptive==0)
	{
	//	std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()
	//			<<" VA deterministic inport_iter("<<inport_iter
	//				<<"), invc_iter("<<invc_iter<<")"<<std::endl;
		low_vc=m_vc_per_vnet-num_escape_path_vcs;
		high_vc=m_vc_per_vnet;
		select_outvc_all(inport_iter, invc_iter, low_vc, high_vc);
	}
	else if (isFlitAdaptive==2)// from adaptive to deterministic vcs
	{
	//	std::cout<<"@"<<curTick()<<" R"<<m_router->get_id()
	//			<<" VA adaptive to deterministic inport_iter("<<inport_iter
	//				<<"), invc_iter("<<invc_iter<<")"<<std::endl;
		low_vc=m_vc_per_vnet-num_escape_path_vcs;
		high_vc=m_vc_per_vnet;
		select_outvc_all(inport_iter, invc_iter, low_vc, high_vc);
	}
	else if(isFlitAdaptive==3)// from deterministic to fadaptive vcs
	{
	#if FADP_BACK_DET_TO_FADP_VC == 0	
		assert(false&&"BAck to FADP non implemented,yet\n");
	#endif
		low_vc=0;
		high_vc=m_vc_per_vnet-num_escape_path_vcs;
		select_outvc_all(inport_iter, invc_iter, low_vc, high_vc);
	}	
	else 
	{
		std::cout<<isFlitAdaptive<<std::endl;
		assert(false);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// PRIVATE FUNCTIONS  /////////////////////////////////////
VCResult
VCallocator_ADAPTIVE_d::arbitrate_outvcs_all(int inport, int invc_offset,int outport_iter,int outvc_iter,int pktMod/*0-3*/,int low_vc,int high_vc, int out_low_vc,int out_high_vc)
{

	//SPECULATIVE structs support
	VCResult VCExecRes;
	VCExecRes.result = false;
	
//	int low_vc,high_vc;
//	low_vc=0;
//	high_vc=m_vc_per_vnet-num_escape_path_vcs;

	int vnet = get_vnet(outvc_iter);
	int invc_base = vnet*m_vc_per_vnet;
	int num_vcs_per_vnet = m_vc_per_vnet;
	
	m_round_robin_outvc[outport_iter][outvc_iter].second++;

	if (m_round_robin_outvc[outport_iter][outvc_iter].second >= out_high_vc
			|| m_round_robin_outvc[outport_iter][outvc_iter].second<out_low_vc) 
	{
	    m_round_robin_outvc[outport_iter][outvc_iter].second = out_low_vc;
		m_round_robin_outvc[outport_iter][outvc_iter].first++;
	    if (m_round_robin_outvc[outport_iter][outvc_iter].first >= m_num_inports)
		{
	        m_round_robin_outvc[outport_iter][outvc_iter].first = 0;
		}
	}

	for (int in_iter = 0; in_iter < m_num_inports*num_vcs_per_vnet;in_iter++) 
	{
	    invc_offset++;
		if (invc_offset >= high_vc ||invc_offset<low_vc)
		{
	        invc_offset = low_vc;
	        inport++;
	        if (inport >= m_num_inports)
			{
	            inport = 0;
			}
	    }
	    int invc = invc_base + invc_offset;

		flit_d* temp_flit=m_input_unit[inport]->peekTopFlit(invc);
		if(temp_flit!=NULL)
			if(temp_flit->getIsAdaptive()!=pktMod)
				continue;
		if(temp_flit==NULL)
			continue;

	    if (m_outvc_req[outport_iter][outvc_iter][inport][invc]) 
		{
		#if USE_NON_ATOMIC_VC_ALLOC==0
			if(!m_output_unit[outport_iter]->is_vc_idle(outvc_iter, m_router->curCycle()))
				continue;
		#else
			if( !( testNAVCA(outport_iter,m_input_unit[inport],invc,outvc_iter) ||
				m_output_unit[outport_iter]->is_vc_idle(outvc_iter, m_router->curCycle())	) )
				continue;
		#endif
	        m_global_arbiter_activity[vnet]++;

			//check if buf reused
			bool flag_vc_reused=false;
			#if USE_WH_VC_SA==1 //for the buffer reuse
			#if USE_VC_REUSE == 1
			if( m_input_unit[inport]->get_reused_remap_vc_used()[invc]==true )
			{
				flag_vc_reused=true; //check auxiliary info not the one in the packet
				m_input_unit[inport]->get_reused_remap_vc_outvc()[invc]=outvc_iter;
				m_input_unit[inport]->get_reused_remap_vc_outvc_va_cycle()[invc]=m_router->curCycle();
				assert(m_input_unit[inport]->get_reused_remap_vc_outport()[invc]==outport_iter);
				break;
			}
			#endif
			#endif
			if(flag_vc_reused==false)
			{
				#if USE_WH_VC_SA==1 //for the buffer reuse
				#if USE_VC_REUSE == 1
				vnet_in_this_vc_outport[outport_iter][outvc_iter]=
							m_input_unit[inport]->peekTopFlit(invc)->get_vnet();
				assert(vnet_in_this_vc_outport[outport_iter][outvc_iter]!=-2);
				#endif
				#endif


				/*NON_ATOMIC_VC_ALLOC*/
				#if  USE_NON_ATOMIC_VC_ALLOC == 1
				assert( m_output_unit[outport_iter]->get_vc_counter_pkt(outvc_iter, m_router->curCycle())>0);
				//assert(TODO);		
				m_output_unit[outport_iter]->dec_vc_counter_pkt(outvc_iter, m_router->curCycle());
				m_output_unit[outport_iter]->set_vc_busy_to_vnet_id(outvc_iter,m_router->getVirtualNetworkProtocol()/*NOVNETID*/);
				#endif

	        m_input_unit[inport]->grant_vc(invc, outvc_iter,m_router->curCycle());
	        m_output_unit[outport_iter]->update_vc(outvc_iter, inport, invc);
				
			isTurnAllowed(inport,outport_iter);
		#if USE_SPECULATIVE_VA_SA==1
			//If i use speculative sa i save inport and invc from this step and send results to the arbitrate_outvc function
		VCExecRes.invc = invc;
		VCExecRes.inport = inport;
		VCExecRes.result = true;
		#else
			m_router->swarb_req();
		#endif
			// update back the adaptivity flag in the head flit 
			if(pktMod==2)
			{
				flit_d* temp_flit=m_input_unit[inport]->peekTopFlit(invc);
				assert(temp_flit!=NULL);
				temp_flit->setIsAdaptive(0);
			}
			if(pktMod==3)
			{
				flit_d* temp_flit=m_input_unit[inport]->peekTopFlit(invc);
				assert(temp_flit!=NULL);
				temp_flit->setIsAdaptive(1);
			}
	        break;
		}//if(flag_vc_reused==false)
	    }
	}
   return VCExecRes;
}





void
VCallocator_ADAPTIVE_d::select_outvc_all(int inport_iter, int invc_iter, int low_vc,int high_vc)
{

    int outport = m_input_unit[inport_iter]->get_route(invc_iter);
    int vnet = get_vnet(invc_iter);
    int outvc_base = vnet*m_vc_per_vnet;
    int num_vcs_per_vnet = high_vc-low_vc;//m_vc_per_vnet;

	assert(num_vcs_per_vnet>0 &&"Impossible to have 0 vc for ADAPTIVE routing");

    int outvc_offset = m_round_robin_invc[inport_iter][invc_iter];
    m_round_robin_invc[inport_iter][invc_iter]++;

    if (m_round_robin_invc[inport_iter][invc_iter] >= high_vc)
        m_round_robin_invc[inport_iter][invc_iter] = low_vc;

    for (int outvc_offset_iter = 0; outvc_offset_iter < num_vcs_per_vnet;outvc_offset_iter++) 
	{
		//std::cout<<"@"<<curTick()<<" outvc_offset_iter"<<outvc_offset_iter<<std::endl; 
        outvc_offset++;
        if (outvc_offset >= high_vc)
            outvc_offset = low_vc;
        int outvc = outvc_base + outvc_offset;

		if (m_output_unit[outport]->is_vc_idle(outvc, m_router->curCycle())) 
		{
			m_local_arbiter_activity[vnet]++;
		    m_outvc_req[outport][outvc][inport_iter][invc_iter] = true;
		    if (!m_outvc_is_req[outport][outvc])
			m_outvc_is_req[outport][outvc] = true;

			m_round_robin_invc[inport_iter][invc_iter]=outvc_offset;
            return; // out vc acquired 
        }
		////////////////////////////////////////////////////////////////////////////////
		//		NON_ATOMIC_VC_ALLOC
			#if USE_NON_ATOMIC_VC_ALLOC==1
			if(testNAVCA(outport,m_input_unit[inport_iter],invc_iter,outvc))
			{// VA_IP non atomic allocation is possible try to reserve
			
				m_local_arbiter_activity[vnet]++;
			    m_local_arbiter_activity_resettable[vnet]++;
			    m_outvc_req[outport][outvc][inport_iter][invc_iter] = true;
			    if (!m_outvc_is_req[outport][outvc])
					m_outvc_is_req[outport][outvc] = true;
		
				//Speculative statistics
				if(m_outvc_is_req[outport][outvc] == true)
				{
					if( m_input_unit[inport_iter]->peekTopFlit(invc_iter)->get_type() == HEAD_TAIL_ ||
		                         m_input_unit[inport_iter]->peekTopFlit(invc_iter)->get_type() == HEAD_ )
					{
					    Router_d::incGlobalCountStatic();
					    m_router->incLocalCount();
					}
				}
		
				m_round_robin_invc[inport_iter][invc_iter]=outvc_offset;
		    	return; // non atomic out vc acquired
			}
			#endif
		////////////////////////////////////////////////////////////////////////////////
    }
}

//function to test the NAVCA conditions on a non-idle buffer. Used both at VA
//and RC when fully adaptive is used
bool
VCallocator_ADAPTIVE_d::testNAVCA(int outport, InputUnit_d* inport,int invc_iter, int outvc)
{
//		NON_ATOMIC_VC_ALLOC
#if USE_NON_ATOMIC_VC_ALLOC==1
	Consumer *maybeNiface = m_output_unit[outport]->getOutLink_d()->getLinkConsumer();
	NetworkInterface_d *niface=dynamic_cast<NetworkInterface_d*>(maybeNiface);

	if(niface==NULL)
	{
		flit_d* temp_flit= inport->peekTopFlit(invc_iter);
		assert(temp_flit!=NULL);
		int vnet_flit = temp_flit->get_vnet();
	#if FULLY_ADP_TPDS_ROUTING==1
		 //Using new TDPS paper condition: use non atomic vc allocation only if the buffer have enought credit to store all the packet
     	if( ! m_output_unit[outport]->is_vc_idle(outvc, m_router->curCycle()) 
				&& vnet_flit== m_output_unit[outport]->get_vc_busy_to_vnet_id(outvc)
         && m_output_unit[outport]->get_credit_cnt(outvc)>= getPckSizeFromVnet(vnet_flit,temp_flit) )
     #else 
		 //Using our more relaxing condition: use non atomic vc allocation also if there isn't enought space in the input buffer but the Turn is allowed and there are at least one credit
		 //I can use non atomic vc allocation also for one flit packet(HEAD and HEADTAIL)
			if( ! m_output_unit[outport]->is_vc_idle(outvc, m_router->curCycle()) 
				&& vnet_flit== m_output_unit[outport]->get_vc_busy_to_vnet_id(outvc)
				  && m_output_unit[outport]->get_credit_cnt(outvc) >= 
				    (getPckSizeFromVnet(vnet_flit, temp_flit )- inport->getBufferSize(invc_iter))
				      && m_output_unit[outport]->get_credit_cnt(outvc)>=1
				        && (isTurnAllowed(inport->getID(), outport) == true || temp_flit->get_type() == HEAD_TAIL_))			
	#endif
			{
				return true;
			}//if(niface) non_atomic_vc_alloc does not support NIC for now
	}
#endif
	return false;
}


