
#include <iostream>
#include <stdio.h>
#include <math.h>

#include "orion_area.hh"
#include "base/misc.hh"
#include "mem/ruby/network/orion/OrionConfig.hh"
#include "mem/ruby/network/orion/TechParameter.hh"

double OrionConfig::router_area_vcallocator()
{
	double area_vcallocator=0;
	double bitline_len, wordline_len;
	int req_width;
	/*
	std::cout<<"VCallocator area computation values"<<std::endl;
	std::cout<<"m_in_buf_num_set = "<<m_in_buf_num_set<<std::endl
			<<"m_flit_width = "<<m_flit_width<<std::endl
			<<"m_num_in_port = "<<m_num_in_port <<std::endl
			<<"m_num_vclass = "<<m_num_vclass<<std::endl
			<<"m_num_vchannel = "<<m_num_vchannel<<std::endl
			<<"vcallocator model = "<<this->m_params_map["VA_MODEL"]<<std::endl
			<<"vcallocator in model = "<<this->m_params_map["VA_IN_ARB_MODEL"]<<std::endl
			<<"vcallocator out model = "<<this->m_params_map["VA_OUT_ARB_MODEL"]<<std::endl
			<<"regcellheight = "<<m_tech_param_ptr->get_RegCellHeight()<<std::endl
			<<"regcellwidth = "<<m_tech_param_ptr->get_RegCellWidth()<<std::endl
			<<"BitlineSpacing = "<<m_tech_param_ptr->get_BitlineSpacing()<<std::endl
			<<"AreaNOR = "<<m_tech_param_ptr->get_AreaNOR()<<std::endl
			<<"AreaINV = "<<m_tech_param_ptr->get_AreaINV()<<std::endl
			<<"AreaDFF = "<<m_tech_param_ptr->get_AreaDFF()<<std::endl;

*/
	if(!m_params_map["VA_MODEL"].compare("ONE_STAGE_ARB") && m_num_vchannel >= 1 && m_num_in_port > 1) //FIXME changed m_num_vchannel > 1
	{

		req_width = (m_num_in_port - 1) * m_num_vchannel;
		if (!m_params_map["VA_OUT_ARB_MODEL"].compare("MATRIX_ARBITER"))
		{//assumes 30% spacing for each arbiter
			area_vcallocator = (	(m_tech_param_ptr->get_AreaNOR() * 2 * (req_width - 1) *  req_width) + 
						(m_tech_param_ptr->get_AreaINV() * req_width)
						+ (m_tech_param_ptr->get_AreaDFF() * (req_width * (req_width - 1)/2)) ) * 1.3 * 
						m_num_out_port * m_num_vchannel * m_num_vclass;

			std::cout<<area_vcallocator<<std::endl;
		}
		else if (!m_params_map["VA_OUT_ARB_MODEL"].compare("RR_ARBITER"))
		{//assumes 30% spacing for each arbiter
			area_vcallocator = ( (6 *req_width * m_tech_param_ptr->get_AreaNOR()) + 
						(2 * req_width * m_tech_param_ptr->get_AreaINV()) + 
						(req_width * m_tech_param_ptr->get_AreaDFF())) * 1.3 * 
						m_num_out_port * m_num_vchannel * m_num_vclass;
		}
		else if (!m_params_map["VA_OUT_ARB_MODEL"].compare("QUEUE_ARBITER"))
		{

			panic("VA_OUT_ARB_MODEL QUEUE_ARBITER not supported yet\n");
			/*
				router_area->vc_allocator = AreaDFF * info->vc_out_arb_queue_info.data_width 
					* info->vc_out_arb_queue_info.n_set * m_num_out_port * m_num_vchannel * m_num_vclass;
			*/
		}
		else
		{
			panic("VA_OUT_ARB_MODEL unrecognized\n");
		}
	}

	else if(!m_params_map["VA_MODEL"].compare("TWO_STAGE_ARB") && m_num_vchannel >= 1 && m_num_in_port > 1)//FIXME changed m_num_vchannel > 1
	{

		if (m_params_map["VA_IN_ARB_MODEL"].compare("NO_SET") && m_params_map["VA_OUT_ARB_MODEL"].compare("NO_SET"))
		{	
			/*first stage*/
			req_width = m_num_vchannel;
			if(!m_params_map["VA_IN_ARB_MODEL"].compare("MATRIX_ARBITER"))
			{//assumes 30% spacing for each arbiter
				area_vcallocator = ((m_tech_param_ptr->get_AreaNOR() * 2 * (req_width - 1) * req_width) +
								(m_tech_param_ptr->get_AreaINV() * req_width) +
								(m_tech_param_ptr->get_AreaDFF() * (req_width * (req_width - 1)/2))) * 1.3 * 
								m_num_in_port * m_num_vchannel * m_num_vclass;
			}
			else if(!m_params_map["VA_IN_ARB_MODEL"].compare("RR_ARBITER"))
			{ //assumes 30% spacing for each arbiter
				area_vcallocator = ((6 *req_width * m_tech_param_ptr->get_AreaNOR()) + 
							(2 * req_width * m_tech_param_ptr->get_AreaINV()) + 
							(req_width * m_tech_param_ptr->get_AreaDFF())) * 1.3 * 
							m_num_in_port * m_num_vchannel * m_num_vclass;
			}
			else if(!m_params_map["VA_IN_ARB_MODEL"].compare("QUEUE_ARBITER"))
			{
				panic("VA_IN_ARB_MODEL QUEUE_ARBITER not supported yet\n");
				/*
				area_vcallocator = m_tech_param_ptr->get_AreaDFF() * 
							info->vc_in_arb_queue_info.data_width * 
							info->vc_in_arb_queue_info.n_set * 
							m_num_in_port * m_num_vchannel * m_num_vclass ; 
				*/
			}
			else
			{
				panic("VA_IN_ARB_MODEL unrecognized\n");
			}

			/*second stage*/
			req_width = (m_num_in_port - 1) * m_num_vchannel;
			if(!m_params_map["VA_OUT_ARB_MODEL"].compare("MATRIX_ARBITER"))
			{//assumes 30% spacing for each arbiter
				area_vcallocator += ((m_tech_param_ptr->get_AreaNOR() * 2 * (req_width - 1) * req_width) + 
						(m_tech_param_ptr->get_AreaINV() * req_width) + 
						(m_tech_param_ptr->get_AreaDFF() * (req_width * (req_width - 1)/2))) * 1.3 * 
						m_num_out_port * m_num_vchannel * m_num_vclass;
			}
			else if(!m_params_map["VA_OUT_ARB_MODEL"].compare("RR_ARBITER"))
			{//assumes 30% spacing for each arbiter
				area_vcallocator += ((6 *req_width * m_tech_param_ptr->get_AreaNOR()) + 
							(2 * req_width * m_tech_param_ptr->get_AreaINV()) + 
							(req_width * m_tech_param_ptr->get_AreaDFF())) * 1.3 * 
							m_num_out_port * m_num_vchannel * m_num_vclass;
			}
			else if(!m_params_map["VA_OUT_ARB_MODEL"].compare("QUEUE_ARBITER"))
			{
				panic("VA_OUT_ARB_MODEL QUEUE_ARBITER not supported yet\n");
				/*
				router_area->vc_allocator += AreaDFF * info->vc_out_arb_queue_info.data_width
					* info->vc_out_arb_queue_info.n_set * info->n_out * info->n_v_channel * info->n_v_class;
				*/
			}
			else
			{
				panic("VA_OUT_ARB_MODEL unrecognized\n");
			}
		}
	}
	else if(!m_params_map["VA_MODEL"].compare("VC_SELECT") && m_num_vchannel >= 1) //FIXME changed m_num_vchannel > 1
	{
		if(!m_params_map["VA_BUF_MODEL"].compare("SRAM"))
		{
			bitline_len = m_num_vchannel * (m_tech_param_ptr->get_RegCellHeight() + 2 * m_tech_param_ptr->get_WordlineSpacing());
			wordline_len = log2(m_num_vchannel) *
			(m_tech_param_ptr->get_RegCellWidth() + 2 *
			(1/*info->vc_select_buf_info.read_ports*/ + 
			1/*info->vc_select_buf_info.write_ports*/) * m_tech_param_ptr->get_BitlineSpacing());

			area_vcallocator = m_num_out_port * m_num_vclass * (bitline_len * wordline_len);
		}
		else if(!m_params_map["VA_BUF_MODEL"].compare("REGISTER"))
		{
				area_vcallocator =
				m_tech_param_ptr->get_AreaDFF() * m_num_out_port *
				m_num_vclass * 
				m_flit_width /*info->vc_select_buf_info.data_width*/ * 
				1/*info->vc_select_buf_info.n_set*/;
		}
		else
		{
			panic("VA_BUF_MODEL unrecognized\n");	
		}
	}
	else
	{
		panic("VA_MODEL unrecognized\n");
	}
	return area_vcallocator;
}
double OrionConfig::router_area_swallocator()
{
	double area_swallocator=0;
	int n_switch_in=1,n_switch_out=1;

/*
	std::cout<<"SWallocator area computation values"<<std::endl;
	std::cout<<"m_in_buf_num_set = "<<m_in_buf_num_set<<std::endl
			<<"m_flit_width = "<<m_flit_width<<std::endl
			<<"m_num_in_port = "<<m_num_in_port <<std::endl
			<<"m_num_vclass = "<<m_num_vclass<<std::endl
			<<"m_num_vchannel = "<<m_num_vchannel<<std::endl
			
			<<"IS_IN_SHARED_BUFFER = "<<this->m_params_map["IS_IN_SHARED_BUFFER"]<<std::endl
			<<"IS_OUT_SHARED_BUFFER = "<<this->m_params_map["IS_OUT_SHARED_BUFFER"]<<std::endl
			<<"IS_IN_SHARED_SWITCH = "<<this->m_params_map["IS_IN_SHARED_SWITCH"]<<std::endl
			<<"IS_OUT_SHARED_SWITCH = "<<this->m_params_map["IS_OUT_SHARED_SWITCH"]<<std::endl
			<<"SA_IN_ARB_MODEL = "<<this->m_params_map["SA_IN_ARB_MODEL"]<<std::endl
			<<"SA_OUT_ARB_MODEL = "<<this->m_params_map["SA_OUT_ARB_MODEL"]<<std::endl

			<<"regcellheight = "<<m_tech_param_ptr->get_RegCellHeight()<<std::endl
			<<"regcellwidth = "<<m_tech_param_ptr->get_RegCellWidth()<<std::endl
			<<"BitlineSpacing = "<<m_tech_param_ptr->get_BitlineSpacing()<<std::endl
			<<"AreaNOR = "<<m_tech_param_ptr->get_AreaNOR()<<std::endl
			<<"AreaINV = "<<m_tech_param_ptr->get_AreaINV()<<std::endl
			<<"AreaDFF = "<<m_tech_param_ptr->get_AreaDFF()<<std::endl;
*/
	if(!m_params_map["IS_IN_SHARED_BUFFER"].compare("TRUE"))
	{
		n_switch_in=1; /*FIXME numread ports if shared the inbuf*/
	}
	else if (!m_params_map["IS_IN_SHARED_SWITCH"].compare("TRUE"))
	{
		n_switch_in=1;
	}
	else /*if not shared*/
	{
		n_switch_in = m_num_vchannel * m_num_vclass;
	}

	if(!m_params_map["IS_OUT_SHARED_BUFFER"].compare("TRUE"))
	{	
		n_switch_out=1;/* FIXME numwrite ports if shared the inbuf*/
	}
	else if (!m_params_map["IS_OUT_SHARED_BUFFER"].compare("TRUE"))
	{
		n_switch_out=1;
	}
	else
	{
		n_switch_out = m_num_out_port;
	}

	int req_width;
	/* switch allocator area */
	if(m_params_map["SA_IN_ARB_MODEL"].compare("NO_SET")) 
	{
		req_width = m_num_vchannel * m_num_vclass;

		if(!m_params_map["SA_IN_ARB_MODEL"].compare("MATRIX_ARBITER"))
		{  //assumes 30% spacing for each arbiter
			area_swallocator = ((m_tech_param_ptr->get_AreaNOR() *
						2 * (req_width - 1) * req_width) +
						(m_tech_param_ptr->get_AreaINV() * req_width) 
						+ (m_tech_param_ptr->get_AreaDFF() * (req_width * (req_width - 1)/2))) * 1.3 *
						n_switch_in* m_num_in_port;


		}
		else if(!m_params_map["SA_IN_ARB_MODEL"].compare("RR_ARBITER"))
		{ //assumes 30% spacing for each arbiter
			area_swallocator = ((6 *req_width *
			m_tech_param_ptr->get_AreaNOR()) + (2 * req_width * m_tech_param_ptr->get_AreaINV()) 
						+ (req_width * m_tech_param_ptr->get_AreaDFF())) * 1.3 * 
						n_switch_in * m_num_in_port;
		}
		else if(!m_params_map["SA_IN_ARB_MODEL"].compare("QUEUE_ARBITER"))
		{
			panic("SA_IN_MODEL QUEUE_ARBITER NOT COMPLETED\n");

			/*
			area_swallocator = m_tech_param_ptr->get_AreaDFF() * 
					info->sw_in_arb_queue_info.n_set * 
					info->sw_in_arb_queue_info.data_width
					* in_n_switch * info->n_in;
			*/
		}
		else
		{
			panic("SA_IN_MODEL unrecognized\n");
		}
		
	}
	else
	{
		panic("SA_IN_MODEL undefined\n");
	}
	std::cout<<area_swallocator<<std::endl;
	if(m_params_map["SA_OUT_ARB_MODEL"].compare("NO_SET"))
	{
		//req_width = info->n_total_in - 1;
		req_width = m_num_out_port - 1;
		if(!m_params_map["SA_OUT_ARB_MODEL"].compare("MATRIX_ARBITER"))
		{//assumes 30% spacing for each arbiter
			area_swallocator += ((m_tech_param_ptr->get_AreaNOR() *
						2 * (req_width - 1) * req_width) + 
						(m_tech_param_ptr->get_AreaINV() * req_width) + 
						(m_tech_param_ptr->get_AreaDFF() * (req_width * (req_width - 1)/2))) * 1.3 *
						n_switch_out;
		}
		else if(!m_params_map["SA_OUT_ARB_MODEL"].compare("RR_ARBITER"))
		{ //assumes 30% spacing for each arbiter
			area_swallocator += ((6 *req_width * m_tech_param_ptr->get_AreaNOR()) + 
						(2 * req_width * m_tech_param_ptr->get_AreaINV()) + 
						(req_width * m_tech_param_ptr->get_AreaDFF())) * 1.3 * 
						n_switch_out;
		}
		else if(!m_params_map["SA_OUT_ARB_MODEL"].compare("QUEUE_ARBITER"))
		{
			panic("SA_OUT_MODEL QUEUE_ARBITER NOT COMPLETED\n");
			/*
				area_swallocator += m_tech_param_ptr->get_AreaDFF() * 
							info->sw_out_arb_queue_info.data_width * 
							info->sw_out_arb_queue_info.n_set * info->n_switch_out;
			*/
		}
		else
		{
			panic("SA_OUT_MODEL unrecognized\n");
		}	
	}
	else
	{
		panic("SA_OUT_MODEL undefined\n");
	}

	return area_swallocator;

}
double OrionConfig::router_area_crossbar()
{
	double area_crossbar=0;

	double xb_in_len=0, xb_out_len=0;
	double depth=0, nMUX=0, boxArea=0; /*assigned in function temp vars*/
	//int req_width=0; /*TODO FIXME unused but orion2.0 uses this var*/
	int n_switch_in=1,n_switch_out=1;
	//uint32_t n_switch_in = string_as_T<uint32_t>(m_params_map["CROSSBAR_NUM_IN_SEG"]);
	//uint32_t n_switch_out = string_as_T<uint32_t>(m_params_map["CROSSBAR_NUM_OUT_SEG"]);
	if(!m_params_map["IS_IN_SHARED_BUFFER"].compare("TRUE"))
	{
		n_switch_in = 1; /*FIXME numread ports if shared the inbuf*/
	}
	else if (!m_params_map["IS_IN_SHARED_SWITCH"].compare("TRUE"))
	{
		n_switch_in = 1;
	}
	else /*if not shared*/
	{
		n_switch_in = m_num_in_port;
	}

	if(!m_params_map["IS_OUT_SHARED_BUFFER"].compare("TRUE"))
	{	
		n_switch_out=1;/* FIXME numwrite ports if shared the inbuf*/
	}
	else if (!m_params_map["IS_OUT_SHARED_BUFFER"].compare("TRUE"))
	{
		n_switch_out=1;
	}
	else
	{
		n_switch_out = m_num_out_port;
	}


/*
	std::cout<<"Crossbar area computation values"<<std::endl;
	std::cout<<"m_in_buf_num_set = "<<m_in_buf_num_set<<std::endl
			<<"m_flit_width = "<<m_flit_width<<std::endl
			<<"m_num_in_port = "<<m_num_in_port <<std::endl
			<<"m_num_vclass = "<<m_num_vclass<<std::endl
			<<"m_num_vchannel = "<<m_num_vchannel<<std::endl
			
			<<"IS_IN_SHARED_BUFFER = "<<this->m_params_map["IS_IN_SHARED_BUFFER"]<<std::endl
			<<"IS_OUT_SHARED_BUFFER = "<<this->m_params_map["IS_OUT_SHARED_BUFFER"]<<std::endl
			<<"IS_IN_SHARED_SWITCH = "<<this->m_params_map["IS_IN_SHARED_SWITCH"]<<std::endl
			<<"IS_OUT_SHARED_SWITCH = "<<this->m_params_map["IS_OUT_SHARED_SWITCH"]<<std::endl
			<<"SA_IN_ARB_MODEL = "<<this->m_params_map["SA_IN_ARB_MODEL"]<<std::endl
			<<"SA_OUT_ARB_MODEL = "<<this->m_params_map["SA_OUT_ARB_MODEL"]<<std::endl

			<<"regcellheight = "<<m_tech_param_ptr->get_RegCellHeight()<<std::endl
			<<"regcellwidth = "<<m_tech_param_ptr->get_RegCellWidth()<<std::endl
			<<"BitlineSpacing = "<<m_tech_param_ptr->get_BitlineSpacing()<<std::endl
			<<"AreaNOR = "<<m_tech_param_ptr->get_AreaNOR()<<std::endl
			<<"AreaINV = "<<m_tech_param_ptr->get_AreaINV()<<std::endl
			<<"AreaDFF = "<<m_tech_param_ptr->get_AreaDFF()<<std::endl;

*/

	/* crossbar area */
	if(m_params_map["CROSSBAR_MODEL"].compare("NO_SET")) 
	{
		if(!m_params_map["CROSSBAR_MODEL"].compare("MATRIX_CROSSBAR"))
		{
			xb_in_len = n_switch_in * m_flit_width * m_tech_param_ptr->get_CrsbarCellWidth();  
			xb_out_len = n_switch_out * m_flit_width * m_tech_param_ptr->get_CrsbarCellHeight();  
			area_crossbar = xb_in_len * xb_out_len;
			std::cout<<xb_in_len<<" "<<xb_out_len<<" " <<n_switch_in<<std::endl;
		}
		else if(!m_params_map["CROSSBAR_MODEL"].compare("MULTREE_CROSSBAR"))
		{
			if(!m_params_map["CROSSBAR_MUX_DEGREE"].compare("2"))
			{
				depth = ceil((log2(n_switch_in) / log2(2)));  
				nMUX = pow(2,depth) - 1;
				boxArea = 1.5 *nMUX * m_tech_param_ptr->get_AreaMUX2();
				area_crossbar = n_switch_in * m_flit_width * boxArea * n_switch_out; 
			}
			else if(!m_params_map["CROSSBAR_MUX_DEGREE"].compare("3")) {
				depth = ceil((log2(n_switch_in) / log2(3))); 
				nMUX = ((pow(3,depth) - 1) / 2);
				boxArea = 1.5 * nMUX * m_tech_param_ptr->get_AreaMUX3();
				area_crossbar = n_switch_in * m_flit_width * boxArea * n_switch_out; 
			}
			else if( !m_params_map["CROSSBAR_MUX_DEGREE"].compare("4"))
			{
				depth = ceil((log2(n_switch_in) / log2(4)));
				nMUX = ((pow(4,depth) - 1) / 3);
				boxArea = 1.5 * nMUX * m_tech_param_ptr->get_AreaMUX4();
				area_crossbar = n_switch_in * m_flit_width * boxArea * n_switch_out; 
			}
			else
			{
				panic("CROSSBAR_MUX_DEGREE unrecognized != {2,3,4}\n");
			}
		}
		else
		{
			panic("CROSSBAR_MODEL unrecognized\n");
		}
	}
	else
	{
		panic("CROSSBAR_MODEL undefined\n");
	}

	return area_crossbar;
}

double OrionConfig::router_area_out_buf()
{
	double bitline_len=0, wordline_len=0;
	double area_out_buf=0;
	/* output buffer area */
	if(m_params_map["OUT_BUF_MODEL"].compare("NO_SET")/*info->out_buf*/) 
	{
		if(!m_params_map["OUT_BUF_MODEL"].compare("SRAM"))
		{
			bitline_len = m_out_buf_num_set * /*num flits per buffer*/
					(m_tech_param_ptr->get_RegCellHeight() + 2 * 
					m_tech_param_ptr->get_WordlineSpacing());
			
			wordline_len = m_flit_width * /*num bits per flit*/
				(m_tech_param_ptr->get_RegCellWidth() + 2 *
				( 1/*info->out_buf_info.read_ports */ + 1 /*info->out_buf_info.write_ports*/) * 
				m_tech_param_ptr->get_BitlineSpacing());

			/* input buffer area */
			area_out_buf = m_num_out_port * m_num_vclass * 
					(bitline_len * wordline_len) *
					(m_params_map["IS_OUT_SHARED_BUFFER"].compare("TRUE") ? 1 : m_num_vchannel );
		}
		else if(!m_params_map["OUT_BUF_MODEL"].compare("REGISTER"))
		{
			area_out_buf = m_tech_param_ptr->get_AreaDFF() * 
					m_num_out_port * m_num_vclass * m_flit_width *
					m_out_buf_num_set *
					(!m_params_map["IS_OUT_SHARED_BUFFER"].compare("TRUE") ? 1 : m_num_vchannel );
		}
		else
		{
			panic("IN_BUF_MODEL undefined\n");
		}
	}
	return area_out_buf;	
}
double OrionConfig::router_area_in_buf()
{
	double bitline_len=0, wordline_len=0;
	double area_in_buf=0;
/*
	std::cout<<"Buffer area computation values"<<std::endl;
	std::cout<<"m_in_buf_num_set = "<<m_in_buf_num_set<<std::endl
			<<"m_flit_width = "<<m_flit_width<<std::endl
			<<"m_num_in_port = "<<m_num_in_port <<std::endl
			<<"m_num_vclass = "<<m_num_vclass<<std::endl
			<<"m_num_vchannel = "<<m_num_vchannel<<std::endl
			<<"buffer model = "<<this->m_params_map["IN_BUF_MODEL"]<<std::endl
			<<"regcellheight = "<<m_tech_param_ptr->get_RegCellHeight()<<std::endl
			<<"regcellwidth = "<<m_tech_param_ptr->get_RegCellWidth()<<std::endl
			<<"BitlineSpacing = "<<m_tech_param_ptr->get_BitlineSpacing()<<std::endl;
*/

	/* buffer area */
	/* input buffer area */
	if (m_params_map["IN_BUF_MODEL"].compare("NO_SET")/*info->in_buf*/) 
	{
		if(!m_params_map["IN_BUF_MODEL"].compare("SRAM"))
		{
			bitline_len =  m_in_buf_num_set * /*num flits per buffer*/
					(m_tech_param_ptr->get_RegCellHeight() + 2 *
					m_tech_param_ptr->get_WordlineSpacing());

			wordline_len = m_flit_width * /*num bits per flit*/
					(m_tech_param_ptr->get_RegCellWidth() + 2 *
					( 1/*info->in_buf_info.read_ports */ + 1 /*info->in_buf_info.write_ports*/) * 
					m_tech_param_ptr->get_BitlineSpacing());

			/* input buffer area */
			area_in_buf = m_num_in_port * m_num_vclass * (bitline_len * wordline_len) *
										(!m_params_map["IS_IN_SHARED_BUFFER"].compare("TRUE") ? 1 : m_num_vchannel );
		}
		else if(!m_params_map["IN_BUF_MODEL"].compare("REGISTER"))
		{ 
			area_in_buf = m_tech_param_ptr->get_AreaDFF() * m_num_in_port * m_num_vclass * m_flit_width *
					m_in_buf_num_set * (!m_params_map["IS_IN_SHARED_BUFFER"].compare("TRUE") ? 1 : m_num_vchannel );
		}
		else
		{
			panic("IN_BUF_MODEL undefined\n");
		}
	}
	return area_in_buf;
}


