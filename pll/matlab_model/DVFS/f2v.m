function [v]=f2v(freq)
	if(freq>=0.8e9)		v=1;
	elseif(freq>=0.5e9)	v=0.9;
	else				v=0.8;
	end
end
