clear; clc; clf

w=10000000; //With these values of w and e the response settles in 1us
e=0.5;
Ts=5e-8;

g=1/(1 +2*e/w*%s +%s^2/w^2);

//Continuous time simulation, for reference
tend=5/(w*e);
tc=0:0.01*tend:tend;
plot(tc,csim('step',tc,g));

//First try: reimplementation of dsimul, as this code will need to be done in C++ 
td=0:Ts:tend;
A=(8-2*Ts^2*w^2)/(Ts^2*w^2+4*e*Ts*w+4); //From tustin discretization, with maxima
B=(-Ts^2*w^2+4*e*Ts*w-4)/(Ts^2*w^2+4*e*Ts*w+4);
C=(Ts^2*w^2)/(Ts^2*w^2+4*e*Ts*w+4);
D=(2*Ts^2*w^2)/(Ts^2*w^2+4*e*Ts*w+4);
E=(Ts^2*w^2)/(Ts^2*w^2+4*e*Ts*w+4);
yd=[];
yoo=0;
yo=0;
u=1; //Step response
for i=1:length(td)
    yd(i)=A*yo +B*yoo +C*u +D*u +E*u;
    yoo=yo;
    yo=yd(i);
end
plot(td,yd);

//Second try: sampling from lagrange formula
ys=( ones(td)-1/(sqrt(1-e^2))*%e^(-e*w*td).*sin( w*td*sqrt(1-e^2)+ones(td)*acos(e) ) );
plot(td,ys);
