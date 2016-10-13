clear; clc; clf

e=0.6; w=4e6; s=1/(1+2*e*%s/w+%s^2/w^2);
//low=1e-9;
//mid=1.5e-9;
//hig=2e-9;    //With a 1GHz to 500MHz step works
low=1e-9;
mid=5e-9;
hig=1e-8;  //With a 1GHz to 100MHz is not numerically stable

function result=uvalue(t)
    if t<0.5e-6 then result=hig;
    elseif t<1e-6 then result=mid;
    elseif t<1.5e-6 then result=low;
    elseif t<2e-6 then result=mid;
    else result=hig;
    end
endfunction

function result=step(timevec,time)
    result=zeros(timevec);
    result(find(timevec>=time))=1;
endfunction

t=[-low 0];
y=[low low];
u=[low low];
i=2;
while(t(i)<4e-6 & i<10000)
    t(i+1)=t(i)+1e-8; //Fixed Ts
    //t(i+1)=t(i)+y(i); //Variable Ts (does not always work)
    u(i+1)=uvalue(t(i+1));
    Ts=t(i+1)-t(i);
    y(i+1)=(2*(Ts*w*e+1)*y(i) - y(i-1) + Ts^2*w^2*u(i+1))/...
           (2*Ts*w*e+Ts^2*w^2+1);
    y(i+1)=max(5e-10,y(i+1));
    i=i+1;
end

t2=0:1e-9:t($);
u2=(hig-low)*step(t2,0)-(hig-mid)*step(t2,0.5e-6)-...
   (mid-low)*step(t2,1e-6)+(mid-low)*step(t2,1.5e-6)+...
   (hig-mid)*step(t2,2e-6);
y2=csim(u2,t2,syslin('c',tf2ss(s)));
y2=low*ones(y2)+y2;

plot(t,u,'k',t,y,'r',t2,y2,'g');
