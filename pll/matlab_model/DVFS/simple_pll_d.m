%% transient pass 90% -ln0.01/(e*eps)

w=4e6;
eps=0.6;
pll_num=[1];
pll_den=[1/w^2,2*eps/w,1];
pll_tf=tf(pll_num,pll_den);


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%% PREPARE THE INPUT	%%%%%%%%%%%%%%%%%%%%%%%%%%
% what happens when the transitory is not finished yet
%t=linspace(0,5e-6,3000);
t=0:1e-9:5e-6;

u=[1e9];
i=2;

%%% u input wave form %%%%%%%%%%%
while(i<=size(t,2))

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
	i=i+1;
end
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

pll_tf_d=c2d(pll_tf,1e-9,'tustin');
pll_ss_d=ss(pll_tf_d);
fprintf('Running lsim ...\n');
[u_lsim]=lsim(pll_ss_d,u,t);

fprintf('Running euler first order method ...\n');
[A,B,C,D]=tf2ss(pll_num,pll_den);

x1=[0];
x2=[0];
y=[0];
i=1;
h=1e-9;


tk=1e-9;
tk_vect=tk;
uk=1e8; %100MHz
%while(i<size(t,2))
k=1;
while(tk<5e-6)
	x1(i+1)=x1(i)+h*(A(1,1)*x1(i)+A(1,2)*x2(i)+B(1)*uk(i));
	x2(i+1)=x2(i)+h*(A(2,1)*x1(i)+A(2,2)*x2(i)+B(2)*uk(i));	

	y(i+1)=C(1)*x1(i)+C(2)*x2(i)+D*uk(i);

	abs_diff=abs(y(i+1)-y(i));
	hk_temp=k/abs_diff;
	hk_temp=min(2e-8,hk_temp);
	%update timestep
	%tk(i+1)=tk(i)+1e-9; 	%fixed
	tk(i+1)=tk(i)+hk_temp;	%variable

	uk(i+1)=uvalueFreq(tk(i+1));

	h=tk(i+1)-tk(i);
	i=i+1;	
end

fprintf('Plotting (%d integration points) ...\n',i);
plot(t,u,'black',t,u_lsim,'g','linewidth',4);
hold on
plot(tk,uk,'black--',tk,y,'r*','linewidth',1);
xlabel('Time (s)','fontsize',20);
ylabel('Frequency (Hz)','fontsize',20);
set(gca(),'fontsize',18)

xlim([0, 4.5e-6]);
legend('Required frequency','Analytical PLL Behavior','New PLL Multi-step Behavior', 'location','southeast')
grid on;


