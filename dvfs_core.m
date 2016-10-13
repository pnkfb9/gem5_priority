
function result=dvfs_core(temperature)
	% global variables
	global olderror=zeros(size(temperature));
	global oldfreq=2000*ones(size(temperature));
	global i=0;
	
	setpoint=0;
	if(i<0.2)
		setpoint=78+273.15; % °C -> K
	elseif(i<0.5)
		setpoint=70+273.15; % °C -> K
	else
		setpoint=75+273.15; % °C -> K
	endif
	i=i+1e-4;
	disp(temperature);
	disp(i);
	
	% constants
	minfreq=100;
	maxfreq=2000;
	
	error=setpoint-temperature;
	% 0.0107+25000/s, discretized using backward euler, Ts=1e-4
	oldfreq=oldfreq+2.5107*error-0.0107*olderror;
	oldfreq=max(min(oldfreq,maxfreq),minfreq); % saturation
	
	result=oldfreq*1e6; % convert MHz to Hz
end
