
function [X,FVAL]=octave_min(T,f, bound_low,bound_high)

	f=strcat("@(W)",f);
	str_T=num2str(T);
	mod_f = strrep(f,"T",str_T);
	mod_f
	mod_f=eval(mod_f)
	[X,FVAL,INFO,OUTPUT]=fminbnd(mod_f,bound_low,bound_high);
	X=floor(X*1000)/1000;

	
