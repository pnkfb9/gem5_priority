w=4*10^6;
eps=0.6;
pll_num=[1];
pll_den=[1/w^2,2*eps/w,1];
pll_tf=tf(pll_num,pll_den);

t=linspace(0,5e-6,5000);
u=[1e9];
i=2;
u_old=0;
y=[u_old,u_old];
t_internal=0;
% From the book "fondamenti di controlli automatici ; bolzern, scattolini, schiavoni"
while(i<size(t,2))

%%% u input wave form %%%%%%%%%%%
	if(i>2200)
		u(i)=1e9;
	elseif(i>1500)
		u(i)=7e8;
	elseif(i>1000)
		u(i)=1e8;
	elseif(i>500 )
		u(i)=5e8;
	else
		u(i)=1e9;
	end

%%	if(i>2800)
%%		u(i)=1e9;
%%	elseif(i>2200)
%%		u(i)=2.5e9;
%%	elseif(i>2000)
%%		u(i)=2e9;
%%	elseif(i>1000)
%%		u(i)=5e8;
%%	elseif(i>500 )
%%		u(i)=3e9;
%%	else
%%		u(i)=1e9;
%%	end
	if(u(i)~=u(i-1))
		u_old=y(i);
		t_internal=0;
	end
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    t_internal=t_internal+1e-9;
	y(i+1)=(u_old+(u(i)-u_old)*(1-1/(sqrt(1-eps^2))*exp(-eps*w*t_internal)*sin(w*t_internal*sqrt(1-eps^2)+acos(eps))));
	
	if(i>500&& i<502)
		fprintf('i=%d u(%d)=%d u_old=%d y(%d+1)=%d \n',i,i,u(i),u_old,i,y(i+1));
	end

	i=i+1;
end
[usim]=lsim(pll_tf,u,t(:,1:end-1));
plot(t(:,1:end-1),u,'black',t,y,'r', t(:,1:end-1),usim,'g', 'linewidth',3);
set(gca(),'fontsize',18)
xlabel('Time (s)','fontsize',20);
ylabel('Frequency (Hz)','fontsize',20);
xlim([0, 4.5e-6]);
grid on;
legend('Required frequency','Analytical PLL Behavior','PLL Single-step Behavior', 'location','southeast')

%plot(t(:,1:end-1),u,'b',t,y,'r',t(:,1:end-1),usim,'g');
