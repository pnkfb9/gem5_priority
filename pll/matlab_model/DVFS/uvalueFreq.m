function [u]=uvalueFreq(tk)
	u=1e9;
	if(tk>25e-6)		u=1.2e9;
	elseif(tk>20e-6)	u=0.8e9;	
	elseif(tk>17e-6)	u=0.6e9;
	elseif(tk>14e-6)	u=0.7e9;
	elseif(tk>8e-6)		u=0.4e9;
	elseif(tk>2.5e-6)	u=1e9; 	% START - fast initial transition requests
	elseif(tk>1e-6) 	u=7e8;
	elseif(tk>1e-6) 	u=1e8;
	elseif(tk>0.5e-6)	u=5e8;
	else 				u=1e9;% END - fast initial transition requests
	end
end
