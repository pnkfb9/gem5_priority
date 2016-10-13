
#ifndef _SIM_ST_TECHNOLOGY
#define _SIM_ST_TECHNOLOGY


#define PARM_DEGRADATION_ALLOWED	0.15	/* [%] */
#define PARM_WAKEUP_TIME_ALLOWED	10e-9	/* [ns] */



#define ST_TRANSISTOR_TYPE NVT

/* This is for 45 nm LTV */
	#define VDD_SRAM_MIN	0.65

	/* EQUATIONS FOR SLEEP TRANSISTOR BASED ON SPICE CIRCUIT AND PTM MOS MODELS*/

	/*FIXME*/
	#define ION_IOFF_ST_EQUATION	"(-1.943e-08)+(5.304e-13)*W+(1.924e-08)*exp(1/W)+(-1.214e-12)*T+(4.155e-14)*T^2+(-2.604e-10)*(T/W)"
	/*FIXME*/
	
	#define IOFF_ST_EQUATION	"(-1.943e-08)+(5.304e-13)*W+(1.924e-08)*exp(1/W)+(-1.214e-12)*T+(4.155e-14)*T^2+(-2.604e-10)*(T/W)"
	#define ION_ST_EQUATION		"((1.687e-03)+(1.712e-07)*W+(-1.650e-03)*exp(1/W)+(-6.490e-07)*T+(1.450e-09)*T^2+(2.231e-05)*(T/W))"
	#define RON_ST_EQUATION 	"(-4.381e+06)+(2.932e+01)*W+(4.362e+06)*exp(1/W)+(-1.151e+02)*T+(4.587e-01)*T^2+(5.232e+04)*(T/W)"
	#define RSWITCH_ST_EQUATION	"(-1.688e+06)+(1.169e+01)*W+(1.684e+06)*exp(1/W)+(-5.922e+01)*T+(2.524e-01)*T^2+(2.665e+04)*(T/W)"

#endif
