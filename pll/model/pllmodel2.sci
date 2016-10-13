clear; clc; clf

w=1e8; //With these values of w and e the response settles in 100ns
e=0.5;

ystart=1000e-12;
yend=2000e-12;
tend=5/(w*e);

t=[0];
y=[ystart];
i=1;
while((t($)<tend) & (i<1000))
    t(i+1)=t(i)+y(i);
    y(i+1)=ystart+(yend-ystart)*( 1-1/(sqrt(1-e^2))*%e^(-e*w*t(i+1))*sin( w*t(i+1)*sqrt(1-e^2)+acos(e) ) );
    i=i+1;
end

plot(t,y);
