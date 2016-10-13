%% transient pass 90% -ln0.01/(e*eps)
t_sim=30e-6;

w_pll=4e6;
eps_pll=0.6;
pll_num=[1];
pll_den=[1/w_pll^2,2*eps_pll/w_pll,1];
pll_tf=tf(pll_num,pll_den);
[y_pll_step,t_pll_step]=step(pll_tf);

w_vreg=3e6;
eps_vreg=0.4;
vreg_num=[1];
vreg_den=[1/w_vreg^2,2*eps_vreg/w_vreg,1];
vreg_tf=tf(vreg_num,vreg_den);
[y_vreg_step,t_vreg_step]=step(vreg_tf);

u=1e8;
t=0:1e-9:t_sim;
for q=1:size(t,2)
	u(q)=uvalueFreq(t(q));	
end




fprintf('Running euler first order method ...\n');
[A_pll,B_pll,C_pll,D_pll]=tf2ss(pll_num,pll_den);
[A_vreg,B_vreg,C_vreg,D_vreg]=tf2ss(vreg_num,vreg_den)


x_pll=[0;0];
y_pll=[1e9];
uk_pll=1e9; %100MHz

% coarse grain pll state init %%%%%%%%%%%%%%%%
x_pll=[(y_pll(1)/C_pll(2))*A_pll(1,2)+B_pll(1)*uk_pll;y_pll(1)/C_pll(2)];
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

x_vreg=[0;0];
y_vreg=[1.1];
uk_vreg=[1.2]; %voltage starts at maximum value

% coarse grain pll state init %%%%%%%%%%%%%%%%
%x_vreg=[(y_vreg(1)/C_vreg(2))*A_vreg(1,2)+B_vreg(1)*uk_vreg;y_vreg(1)/C_vreg(2)];
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


h=1e-9;
tk=1e-9;
tk_vect=tk;


k=1;
i=1;
while(tk<t_sim)

	%euler vreg model
	x_vreg=[x_vreg, x_vreg(:,i)+h*(A_vreg*x_vreg(:,i) + B_vreg*uk_vreg(:,i))];
	y_vreg=[y_vreg, C_vreg*x_vreg(:,i)+D_vreg*uk_vreg(:,i)];

	% euler pll model
	x_pll=[x_pll, x_pll(:,i)+h*(A_pll*x_pll(:,i) + B_pll*uk_pll(:,i))];
	y_pll=[y_pll, C_pll*x_pll(:,i)+D_pll*uk_pll(:,i)];

	abs_diff=abs(y_pll(i+1)-y_pll(i));
	hk_temp=k/abs_diff;
	hk_temp=min(2e-8,hk_temp);

	%update timestep
	%tk(i+1)=tk(i)+1e-9; 	%fixed
	tk(i+1)=tk(i)+hk_temp;	%variable

	uk_pll(i+1)=uvalueFreq(tk(i+1));
	uk_vreg(i+1)=f2v(y_pll(i));

	h=tk(i+1)-tk(i);
	i=i+1;	
end


fprintf('Plotting (%d integration points) ...\n',i);

p1=subplot(2,1,1);
plot(t,u,'black','linewidth',3);
hold on
plot(tk,uk_pll,'black-.',tk,y_pll,'g*','linewidth',1);
xlabel('Time (s)','fontsize',20);
ylabel('Frequency (Hz)','fontsize',20);
set(gca(),'fontsize',18)
xlim([0, t_sim]);
legend('Required frequency','Analytical PLL Behavior','New PLL Multi-step Behavior', 'location','southeast')
grid on;

p2=subplot(2,1,2);
plot(tk,uk_vreg,'black','linewidth',3);
hold on
plot(tk,uk_vreg,'black-.',tk,y_vreg,'r*','linewidth',1);
xlabel('Time (s)','fontsize',20);
ylabel('Voltage (V)','fontsize',20);
set(gca(),'fontsize',18)
grid on;
linkaxes([p1,p2],'x')

