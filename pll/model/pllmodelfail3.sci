
clf; clc;

function result=step(timevec,time)
    result=zeros(timevec);
    result(find(timevec>=time))=1;
endfunction

function result=twopoles(timevec,st,en)
    result=st*ones(timevec)+(en-st)*...
        (ones(timevec)-1/(sqrt(1-e^2))*%e^(-e*w*timevec).*...
        sin(w*timevec*sqrt(1-e^2)+ones(timevec)*acos(e)));
endfunction

e=0.6; w=4e6; s=1/(1+2*e*%s/w+%s^2/w^2);
t=0:1e-8:3e-6;

u=step(t,0)-0.5*step(t,0.5e-6)-0.5*step(t,1e-6)+...
  0.5*step(t,1.5e-6)+0.5*step(t,2e-6);
y1=csim(u,t,syslin('c',tf2ss(s)));

y2a=twopoles(0:1e-8:0.5e-6-1e-8,0,1);
y2b=twopoles(0:1e-8:0.5e-6-1e-8,y2a($),0.5);
y2c=twopoles(0:1e-8:0.5e-6-1e-8,y2b($),0);
y2d=twopoles(0:1e-8:0.5e-6-1e-8,y2c($),0.5);
y2e=twopoles(0:1e-8:1.e-6,y2d($),1);
y2=[y2a y2b y2c y2d y2e];

plot(t,u,'k',t,y1,'g',t,y2,'r');
