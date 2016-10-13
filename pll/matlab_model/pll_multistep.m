w=4e6;
eps=0.6;
pll_num=[1];
pll_den=[1/w^2,2*eps/w,1];
pll_tf=tf(pll_num,pll_den);
%lsim(pll_tf);

% what happens when the transitory is not finished yet
t=linspace(0,5e-6,5000);
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
fprintf('Running lsim ...\n');
size(t)
size(u)
[u_lsim]=lsim(pll_tf,u,t);

plot(t,u,'black',t,u_lsim,'g','linewidth',4)
grid on

% how to replicate the behavior to be used as ode solver in the simulator

% 1) move from tf2ss to get the dynamic system of the process
[A,B,C,D]=tf2ss(pll_num,pll_den)
% 2) use the direct euler method as first solution

x1=[0];
x2=[0];
y=[0];
i=1;
h=1e-9;


% data extracted using matlab with tf2ss, taken form scilab !!!!
a12=4194304; a21=-3814697.3; a22=-4800000;
b2=4000000; 
c1=0.9536743;

% data extracted using matlab with tf2ss. IT SEEMS MATLAB HAS A BUG !!!!
%a11=-4800000; a12=-1.6e13; a21=1;
%b1=1; c2=1.6e13;

A=[0,4194304;-3814697.3,-4800000];
B=[0;4000000];
C=[0.9536743,0];
D=[0];

x_temp=[0,0];


while(i<size(t,2))
	%x_temp(1)=x1(i)+h*(A(1,:)*[x1(i);x2(i)])+h*B(1,:)*u(i);
	%x_temp(2)=x2(i)+h*(A(2,:)*[x1(i);x2(i)])+h*B(2,:)*u(i);

	x_temp(1)=x1(i)+h*a12*x2(i);
	x_temp(2)=x2(i)+h*(a21*x1(i)+a22*x2(i)+b2*u(i));

	x1(i+1)=x_temp(1);
	x2(i+1)=x_temp(2);

	y(i+1)=c1*x1(i);

	i=i+1;
end
hold on;
plot(t,y,'r','linewidth',2)
xlabel('Time (s)','fontsize',20);
ylabel('Frequency (Hz)','fontsize',20);
set(gca(),'fontsize',18)

xlim([0, 4.5e-6]);
legend('Required frequency','Analytical PLL Behavior','New PLL Multi-step Behavior', 'location','southeast')
grid on;
% 3) try to use the modified euler method or the backwards euler method

