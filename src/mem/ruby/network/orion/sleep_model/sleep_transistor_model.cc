#include "sleep_transistor_model.hh"

#include <cassert>
#include <limits>

#if USE_OCTAVE

/*OCTAVE includes*/
#include <octave-3.6.2/octave/oct.h>
#include <octave-3.6.2/octave/octave.h>
#include <octave-3.6.2/octave/parse.h>
#include <octave-3.6.2/octave/toplev.h>

#endif
//#include "SIM_port.h"

/*this is for area models*/
#include "./cacti_area_sleep/io.h"
#include "./cacti_area_sleep/area.h"
#include "./cacti_area_sleep/basic_circuit.h"
#include "./cacti_area_sleep/parameter.h"
#include "./cacti_area_sleep/crossbar.h"
#include "./cacti_area_sleep/arbiter.h"
#include "./cacti_area_sleep/router.h"
#include "./cacti_area_sleep/technology.h"

void Sleep_transistor_model::open_embedded_octave()
{
#if USE_OCTAVE
	const char * argvv[]={"","--silent"};
	octave_main(2,(char **)argvv,true /*embedded*/);
#endif
}
void Sleep_transistor_model::close_embedded_octave()
{
#if USE_OCTAVE
	do_octave_atexit ();
#endif
}
void Sleep_transistor_model::compute_sleep_network()
{/*no account for wiring*/
	int numst_due_to_ron=0;
	int numst_due_to_rswitch=ceil(this->Rswitch_st/Rst_switch_required_wakeup);

	assert( !(
			this->en_vddmin_constr==false &&
			this->en_perf_constr==false &&
			this->en_wakeup_constr==false
		) );
	if (Rst_on_required_perf>Rst_on_required_vddmin && this->en_vddmin_constr)
		numst_due_to_ron=this->Ron_st/Rst_on_required_vddmin;
	else
		if(this->en_perf_constr)
			numst_due_to_ron=ceil(this->Ron_st/Rst_on_required_perf);
		else
			numst_due_to_ron=0;


	if(numst_due_to_ron>numst_due_to_rswitch && this->en_wakeup_constr)
		this->num_st_power_network=numst_due_to_ron;
	else
		this->num_st_power_network=numst_due_to_rswitch;
	
	this->area_power_network=this->num_st_power_network*this->Area_st;

}
void Sleep_transistor_model::get_st_area()
{
	double tech_point_um = (double)((double)this->tech_point/(double)1000);
	/*from cacti_interface() to init transistor geometries allowind
	 * for folding and others.*/
	CactiArea::g_ip = new CactiArea::InputParameter();
	CactiArea::g_ip->parse_cfg("cache.cfg");
	CactiArea::init_tech_params_cacti( tech_point_um/*double technology in um*/, true/*bool is_tag*/);
	/*end cacti init*/

	CactiArea::Component *ptr_component=new CactiArea::Component();
	CactiArea::Area area_st=ptr_component->compute_return_gate_area(SLEEP_TR,
							1,
							this->W_st,
							tech_point_um,
							CactiArea::g_tp.cell_h_def);

	this->Area_st = area_st.get_area();
	this->H_st = area_st.get_h();
	this->W_st = area_st.get_w();
	/* estimate for the aspect ratio of the various component is still missing */
	/* it is possible to evaluate for router crossbar buffers and
	 * arbitrator*/
}
//	void get_top_side_block_area(SIM_router_info_t *info, SIM_router_area_t *router_area)
//	{
//	/* Adding 2 horizontal stripes for pmos and vvdd (virtual vdd)  */
//		std::cout<<"Computed area for each considered component in the router"<<std::endl
//			<<"	router_area->buffer = "<<router_area->buffer<<std::endl
//			<<"	router_area->crossbar ="<<router_area->crossbar<<std::endl
//			<<"	router_area->vc_allocator = "<<router_area->vc_allocator<<std::endl
//			<<"	router_area->sw_allocator = "<<router_area->sw_allocator<<std::endl;
//		std::cout<<std::endl;	
//		std::cout<<"Computed top border lenght to dimension the sleep network for each component in the router"<<std::endl
//			<<"	router_area->buffer_TOP_SIDE = "<<sqrt(router_area->buffer)<<std::endl
//			<<"	router_area->crossbar_TOP_SIDE ="<<sqrt(router_area->crossbar)<<std::endl
//			<<"	router_area->vc_allocator_TOP_SIDE = "<<sqrt(router_area->vc_allocator)<<std::endl
//			<<"	router_area->sw_allocator_TOP_SIDE = "<<sqrt(router_area->sw_allocator)<<std::endl;
//	}

	/* Estimates the parameters required for the sleep transistor logic in
	 * function of the gated block */
	void Sleep_transistor_model::get_router_R_requirements()
	{
		this->Rst_on_required_perf = (PARM_DEGRADATION_ALLOWED)*this->block_Rswitch/(1-PARM_DEGRADATION_ALLOWED);
		this->Rst_on_required_vddmin = ((double)Vdd-(double)VDD_SRAM_MIN)/ this->block_Imax;
		this->Rst_switch_required_wakeup = PARM_WAKEUP_TIME_ALLOWED/(5*this->block_Ceq);  /*tao=5*R*C*/
	}


	/* we compute Ioff, Ion, Ron, Rswitch, it is also useful to have an
	 * estimate of the area following the LAMBDA RULES (lamda in Orion2.0).
	*/
	void Sleep_transistor_model::get_opt_st_parameters(int W_opt)
	{
		assert(W_opt!=0 || this->W_st!=0);
		if(W_opt==0)
		{
			W_opt=this->W_st;
		}
#if USE_OCTAVE
		//const char * argvv[]={"","--silent"};
		//octave_main(2,(char **)argvv,true /*embedded*/);
		octave_value_list f_args;

		f_args(0) = (int) this->worst_temp;
		f_args(1) = (int) W_opt;
		f_args(2) = IOFF_ST_EQUATION;/*"@(W,T) W^2+T"; test equation*/ /*Ioff*/
		f_args(3) = ION_ST_EQUATION; /*Ion*/
		f_args(4) = RON_ST_EQUATION; /*Ron*/
		f_args(5) = RSWITCH_ST_EQUATION; /*Rswitch*/

		octave_value_list result = feval("octave_opt_st_parameters",f_args,4);
		
		this->Ioff_st=result(0).scalar_value();
		this->Ion_st=result(1).scalar_value();
		this->Ron_st=result(2).scalar_value();
		this->Rswitch_st=result(3).scalar_value();
#endif
		//do_octave_atexit ();
	}


	/* PRIVATE
	   Starting from optimal W we get the close allowed W value 	 
	 * FIXME: units, ? [nm] ?
	 * FIXME: input width data is INTEGER right now
	 */
	int Sleep_transistor_model::get_opt_st_allowed_W(int W_opt)
	{
		int W_temp_prev=this->tech_point*this->min_wl_ratio;
		if(this->max_wl_ratio>=2)
		{
			std::cout<<this->min_wl_ratio <<" "<<this->max_wl_ratio <<std::endl;
			int W_temp=this->tech_point * (this->min_wl_ratio+1);
			std::cout<<W_temp<<std::endl;
			for(int i=this->min_wl_ratio;i<this->max_wl_ratio;i++)
			{
				if( abs(W_temp_prev-W_opt)>abs(W_temp-W_opt))
				{
					W_temp_prev=W_temp;
					W_temp=W_temp+this->tech_point;
				}
				else
					return W_temp_prev;
			}
		}
		return W_temp_prev;
	}
	/* given the worst temperature to obtain the best base transistor. 
	NOTE worst temperature is different from actual temperature */
	int Sleep_transistor_model::optimize_ST_base()
	{

#if USE_OCTAVE
		//const char * argvv[]={"","--silent"};
		//octave_main(2,(char **)argvv,true /*embedded*/);
	
		octave_value_list f_args;
		int is_max=0;
		f_args(0) = (double)(this->worst_temp);
		if(this->opt_for==1)
		{/*note this is a maximization while octave compute minimum*/
			f_args(1) = ION_IOFF_ST_EQUATION;
			is_max=true;
		}
		else if(this->opt_for==2)
			f_args(1) = IOFF_ST_EQUATION;
		else if(this->opt_for==3)
		{/*note this is a maximization*/
			f_args(1) = ION_ST_EQUATION;
			is_max=1;
		}
		else
		{
			std::cout<<"ERROR unable to optimize the best base"<<
					"sleep transistor [PARM_OPT range {1,2,3}]"<<std::endl;
			exit(1);
		}
		f_args(2) = (this->tech_point*this->min_wl_ratio);
		f_args(3) = (this->tech_point*this->max_wl_ratio);
		f_args(4) = is_max;

		octave_value_list result = feval ("octave_min",f_args,1);
		int w=0;
		if(is_max)
		{
			std::cout<<" W="<<result(0).scalar_value()<<" Y="<<-result(1).scalar_value()<<std::endl;
		}	
		else
		{
			std::cout<<" W="<<result(0).scalar_value()<<" Y="<<result(1).scalar_value()<<std::endl;
		}
		w=result(0).scalar_value();
		/* started from the founded optimal w value reshape to a
		 * technology available w value */	
		this->W_st=get_opt_st_allowed_W(w);
		
		std::cout<<"FINAL TRANSISTOR: W = "<<this->W_st<<std::endl;	
		return this->W_st;
#else
		std::cout<<"I'm not using Octave optimization, return 45nm"<<std::endl;
		return 45;
#endif
		//do_octave_atexit ();

	}

	void Sleep_transistor_model::testEmbeddedOctave()
	{
     		//const char * argvv [] = {"" /* name of program, not relevant */, "--silent"};
     	  	//octave_main (2, (char **) argvv, true /* embedded */);

#if USE_OCTAVE
     		octave_value_list functionArguments; /*this object is used to prepare octave function call*/
     		functionArguments (0) = 2;	
     		functionArguments (1) = "A.N. Other";
     		Matrix inMatrix (2, 3);
     		inMatrix (0, 0) = 10;
     		inMatrix (0, 1) = 9;
     		inMatrix (0, 2) = 8;
     		inMatrix (1, 0) = 7;
     		inMatrix (1, 1) = 6;
     		functionArguments (2) = inMatrix;
     		const octave_value_list result = feval ("exampleOctaveFunction", functionArguments, 1);
     		/*
		std::cout << "resultScalar is " << result (0).scalar_value () << std::endl;
     		std::cout << "resultString is " << result (1).string_value () << std::endl;
     		std::cout << "resultMatrix is\n" << result (2).matrix_value (); 
		*/
     		//do_octave_atexit ();
#endif
	}
void Sleep_transistor_model::printSleepTransistorConfig(std::ostream& out,std::string component_name)
{
	out<<"Technology Parameters for "<< component_name<<" :"<<std::endl
		<<"\tVdd [V] = "<<Vdd<<std::endl
		<<"\ttech_point [nm] = "<<tech_point<<std::endl
		<<"\tworst_temp [K] = "<<worst_temp<<std::endl
		<<"\tmin_wl_ratio = "<<min_wl_ratio<<std::endl
		<<"\tmax_wl_ratio = "<<max_wl_ratio<<std::endl;
	
	out<<"Single sleep transistor parameters"<<std::endl
		<<"\tIoff_st [A] = "<<Ioff_st<<std::endl
		<<"\tIon_st [A] = "<<Ion_st<<std::endl
		<<"\tRon_st [ohm] = "<<Ron_st<<std::endl
		<<"\tRswitch_st [ohm] = "<<Rswitch_st<<std::endl
		<<"\tArea [H*W] [um^2 ??] = "<<Area_st <<"[ "<<H_st<<" * "<<W_st<<"]"<<std::endl;
	
	out<<"Power gated block parameters"<<std::endl
		<<"\t block_area [um^2] = "<<block_area<<std::endl
		<<"\t block_power_dyn [W] = "<<block_power_dyn<<std::endl
		<<"\t block_power_static [W] = "<<block_power_static<<std::endl
		<<"\t block_Rswitch [W] = "<<block_Rswitch<<std::endl	
		<<"\t block_Ceq [F] = "<<block_Ceq<<std::endl
		<<"\t block_Imax [A] = "<<block_Imax<<std::endl
		<<"\t block_avg_energy [J] = "<<block_avg_energy<<std::endl;

	out<<"Optimization requirements"<<std::endl;
	if(!this->en_perf_constr)
		out<<" NOT USED ";
	out<<"\t Rst_on_required_perf [ohm] = "<<Rst_on_required_perf<<std::endl;
	if(!this->en_vddmin_constr)
		out<<" NOT USED ";
	out<<"\t Rst_on_required_vddmin [ohm] = "<<Rst_on_required_vddmin<<std::endl;
	if(!this->en_wakeup_constr)
		out<<" NOT USED ";
	out<<"\t Rst_switch_required_wakeup [ohm] = "<<Rst_switch_required_wakeup<<std::endl; 

	
	out<<std::endl<<"SLEEP NETWORK"<<std::endl
		<<"\t num_st_power_network = "<<this->num_st_power_network<<std::endl
		<<"\t area_power_network = "<<this->area_power_network<<std::endl
		<<std::endl;
}

void Sleep_transistor_model::printBlockArea(std::ostream& out,std::string component_name)
{
	out<<"\tblock_area [um^2] = "<<block_area<<std::endl;
}

void Sleep_transistor_model::printBlockPower(std::ostream& out,std::string component_name)
{
	out<<"\tblock_total_power [W] = "<<block_power_dyn+block_power_static
		<<" [ block_dynamic_power [W] = "<<block_power_dyn
		<<"   block_power_static [W] =	"<<block_power_static<<" ]"<<std::endl;
}
