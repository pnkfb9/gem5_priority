#ifndef SLEEP_TRANSISTOR_MODEL_H
#define SLEEP_TRANSISTOR_MODEL_H

#include <iostream>

#include <cstdio>
#include <cmath>

#include <regex>
#include <string>
#include <cassert>

#include "sleep_transistor_model_define.hh"
#include "config/use_octave.hh"




class Sleep_transistor_model
{
	public:
		static void open_embedded_octave();
		static void close_embedded_octave();

		Sleep_transistor_model(int opt_for=3, bool en_perf_constr=false,
		double Vdd=1.0, int tech_point=45,int worst_temp=70, 
		int min_wl_ratio=1, int	max_wl_ratio=10, bool en_wakeup_constr=true,
		bool en_vddmin_constr=false)
		{
			assert(min_wl_ratio >= 1);
			assert(max_wl_ratio <= 10);
			assert(!(opt_for<1 || opt_for>3));

			this->Vdd=Vdd; /* FIXME int -> double*/
			this->opt_for=opt_for;
			this->tech_point=tech_point;
			this->worst_temp=worst_temp;
			this->min_wl_ratio=min_wl_ratio;
			this->max_wl_ratio=max_wl_ratio;

			this->Rst_on_required_perf = 0;
			this->Rst_on_required_vddmin=0;
			this->Rst_switch_required_wakeup=0;  /*tao=5*R*C*/
		
			this->Ioff_st=0;
			this->Ion_st=0;
			this->Ron_st=0;
			this->Rswitch_st=0;
		
			this->Area_st=0;
			this->H_st=0;
			this->W_st=0;
		
		
			this->num_st_power_network=0;	
			this->area_power_network=0;

			this->block_area=0;

			this->block_power_dyn=0;
			this->block_power_static=0;
			this->block_Rswitch=0;
			this->block_Ceq=0;
			this->block_Imax=0;
			
			this->block_avg_energy=0;

			this->en_wakeup_constr=en_wakeup_constr;
			this->en_vddmin_constr=en_vddmin_constr;
			this->en_perf_constr=en_perf_constr;
		};
		void set_block_Imax(double block_Imax){this->block_Imax=block_Imax;};
		void set_block_Ceq(double block_Ceq){this->block_Ceq=block_Ceq;};
		void set_block_Rswitch(double block_Rswitch){this->block_Rswitch=block_Rswitch;};
		void set_block_power_static(double block_power_static){this->block_power_static=block_power_static;};
		void set_block_power_dyn(double block_power_dyn){this->block_power_dyn=block_power_dyn;};
		void set_block_area(double block_area){this->block_area=block_area; };

		double get_block_Imax(){return this->block_Imax;};
		double get_block_Ceq(){return this->block_Ceq;};
		double get_block_Rswitch(){return this->block_Rswitch;};
		double get_block_power_static(){return this->block_power_static;};
		double get_block_power_dyn(){return this->block_power_dyn;};
		double get_block_area(){return this->block_area; };	
		
		double get_st_Ion(){return this->Ion_st;};
		double get_st_Ioff(){return this->Ioff_st;};

		double get_st_Ron(){return this->Ron_st;};
		double get_st_Rswitch(){return this->Rswitch_st;};
		uint32_t get_st_num(){return this->num_st_power_network;};





		void get_opt_st_parameters(int w_temp=0);
		int optimize_ST_base();
		
		void printSleepTransistorConfig(std::ostream &out,std::string);
		void printBlockArea(std::ostream&,std::string);
		void printBlockPower(std::ostream&,std::string);

		void get_router_R_requirements();

		void get_st_area();
		void compute_sleep_network();
	private:

	/*functions*/
	void testEmbeddedOctave();

	int get_opt_st_allowed_W(int);
	
	/*variables former #define*/
	double Vdd;
	int opt_for;
	int tech_point; /*nm*/
	int worst_temp; /*K*/
	int min_wl_ratio;
	int max_wl_ratio;

	
	double Rst_on_required_perf;
	double Rst_on_required_vddmin;
	double Rst_switch_required_wakeup;  /*tao=5*R*C*/

	double Ioff_st;
	double Ion_st;
	double Ron_st;
	double Rswitch_st;

	double Area_st;
	double H_st;
	double W_st;


	int num_st_power_network;	
	

	double area_power_network;
	double block_power_dyn;
	double block_power_static;
	double block_area;
	double block_Rswitch;
	double block_Ceq;
	double block_Imax; 
	double block_avg_energy;

//enable constraints to design sleep transistor network
	bool en_wakeup_constr;
	bool en_vddmin_constr;
	bool en_perf_constr;


};


//#ifdef __cplusplus
//extern "C"{
//#endif
//#include "SIM_router.h"
//#include "SIM_time.h"

//st_struct st_base[4];

//void get_sleep_network_area(SIM_router_info_t *info, SIM_router_area_t *router_area);


//int optimize_ST_base();

//#ifdef __cplusplus
//}
//#endif

#endif /* ORION_PRINT_H */





