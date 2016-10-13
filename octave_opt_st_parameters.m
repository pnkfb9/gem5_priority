function [Ioff,Ion,Ron,Rswitch]=octave_opt_st_parameters(T_in,W_in, f_ioff,f_ion,f_ron,f_rswitch)

	T_in,W_in
	
	f_ioff=strcat("@(T,W)",f_ioff);
	f_ion=strcat("@(T,W)",f_ion);
	f_ron=strcat("@(T,W)",f_ron);
	f_rswitch=strcat("@(T,W)",f_rswitch);

	f_ioff=eval(f_ioff)
	f_ion=eval(f_ion)
	f_ron=eval(f_ron)
	f_rswitch=eval(f_rswitch)
	
	Ioff=feval(f_ioff,T=T_in,W=W_in);
	Ion=feval(f_ion,T=T_in,W=W_in);
	Ron=feval(f_ron,T=T_in,W=W_in);
	Rswitch=feval(f_rswitch,T=T_in,W=W_in);

	% Since approximation some small values are less than zero!! tobe fixed
	if(Ioff<0)
	 Ioff=1e-12;
	end
	if(Ion<0)
	 Ion=1e-12;
	end
	if(Ron<0) 
	 Ron=0;
	end
	if(Rswitch<0) 
	 Ron=0;
	end
	
