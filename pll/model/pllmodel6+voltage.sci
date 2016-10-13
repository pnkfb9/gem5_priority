clear; clc; clf

low=1e8;         //100MHz
mid=5e8;         //500MHz
hig=1e9;         //1GHz
simend=17.95e-6;  //Simulation end time
maxpoints=10000; //Max simulated points, prevents runaway

fthres=[4e8,8e8];
vval=[0.8,0.9,1.0];
settling=1e-6;
settlingv=2e-6;

function result=f2v(f)
    global fthres;
    global vval;
    dex=find(f>=fthres);
    if length(dex)==0 then
        result=vval(1);
    else
        result=vval(dex($)+1);
    end
endfunction

function result=uvalue(t)
    if t<0.5e-6 then result=low;
    elseif t<4e-6 then result=hig;
    elseif t<9e-6 then result=mid;
    elseif t<12e-6 then result=low;
    elseif t<15e-6 then result=mid;
    else result=hig;
    end
endfunction

//Input and output variable initialization
t=[0];
y=[low];
u=[low];
udelayed=[low];
timeout=0;
i=1;
k=8;     //Integration step multiplier
h=k/low; //Integration step

yv=[f2v(u(1))];
uv=[f2v(u(1))];
uvdelayed=[f2v(u(1))];
timeoutv=0;

e=0.6; w=4e6; s=1/(1+2*e*%s/w+%s^2/w^2);
//From tf2ss(syslin('c',s))
a12=4194304; a21=-3814697.3; a22=-4800000;
b2=4000000; c1=0.9536743;
//From e=0.6; w=2e6; s=1/(1+2*e*%s/w+%s^2/w^2); tf2ss(syslin('c',s))
a12v=2097152; a21v=-1907348.6; a22v=-2400000;
b2v=2000000; c1v=0.9536743;

//State initialization
x1=y(1)/c1;
x2=(h*a21*x1+h*b2*u(1))/(-h^2*a12*a21+h*a22);

x1v=yv(1)/c1v;
x2v=(h*a21v*x1v+h*b2v*uv(1))/(-h^2*a12v*a21v+h*a22v);

while(t(i)<simend & i<maxpoints)
    t(i+1)=t(i)+k/y(i); //Variable integration step
    //t(i+1)=t(i)+k/low;  //Fixed integration step
    u(i+1)=uvalue(t(i+1));
    uv(i+1)=f2v(u(i+1));
    h=t(i+1)-t(i);
    
    //Fixed waiting time
    if uv(i+1)>uv(i) then
        timeout=t(i+1)+settling;
    end
    if t(i+1)>timeout then
        udelayed(i+1)=u(i+1);
    else
        udelayed(i+1)=udelayed(i);
    end
    
    if uv(i+1)<uv(i) then
       timeoutv=t(i+1)+settlingv; 
    end
    if t(i+1)>timeoutv then
        uvdelayed(i+1)=uv(i+1);
    else
         uvdelayed(i+1)=uvdelayed(i);
    end
    
    //x2=(x2+h*a21*x1+h*b2*u(i))/(1-h^2*a21*a12-h*a22);
    //x1=x1+h*a12*x2;
    x2=(x2+h*a21*x1+h*b2*udelayed(i))/(1-h*a22);
    x1=x1+h*a12*x2;
    //Prevent clock to become less than 30MHz, because
    //if it becomes zero or negative, the variable integration
    //step will fail. This is not unrealistic as any VCO has
    //a minimum frequency. There is a tradeoff between this
    //value, the k value and the maximum step change that
    //can be simulated without incurring in simulation errors.
    y(i+1)=max(30e6,c1*x1);
    
    x2v=(x2v+h*a21v*x1v+h*b2v*uvdelayed(i))/(1-h*a22v);
    x1v=x1v+h*a12v*x2v;
    yv(i+1)=c1v*x1v;
    
    i=i+1;
end
printf("Similation took %d steps\n",i);
figure(1);
clf;
subplot(211);
plot(t,udelayed,'g',t,u,'k',t,y,'r',t,y,'+'); //Point validation
subplot(212);
plot(t,uvdelayed,'g',t,uv,'k',t,yv,'r',t,yv,'+');

function result=step(timevec,time)
    result=zeros(timevec);
    result(find(timevec>=time))=1;
endfunction

//t2=0:1e-10:t($);
//u2=(hig-low)*step(t2,0)-(hig-mid)*step(t2,0.5e-6)-...
//   (mid-low)*step(t2,1e-6)+(mid-low)*step(t2,1.5e-6)+...
//   (hig-mid)*step(t2,2e-6);
////csim seems to have some numerical problems at simulating
////something with t~1e-9 and u~1e9, so dividinf the u values
//fixfactor=1e4;
//u2=u2./fixfactor;
//y2=csim(u2,t2,syslin('c',tf2ss(s)));
//y2=y2.*fixfactor;
//y2=low*ones(y2)+y2;

figure(0);
clf;
subplot(211);
xlabel("Time [s]",'fontsize',2);
ylabel("PLL frequency [Hz]",'fontsize',2);
ax=gca();
ax.font_size=2;
plot(t,udelayed,'g',t,u,'k',t,y,'r');
e=gce();
e.children(1).thickness=2;
e.children(2).thickness=2;
e.children(3).thickness=2;
legend(['interlock';'set point';'output'],4);
xgrid(color('grey'));

subplot(212);
//xlabel("Time [s]",'fontsize',2);
ylabel("VREG output [V]",'fontsize',2);
ax=gca();
ax.font_size=2;
plot(t,uvdelayed,'g',t,uv,'k',t,yv,'r');
e=gce();
e.children(1).thickness=2;
e.children(2).thickness=2;
e.children(3).thickness=2;
legend(['interlock';'set point';'output'],4);
xgrid(color('grey'));
