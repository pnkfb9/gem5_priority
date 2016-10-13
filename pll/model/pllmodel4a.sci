clear; clc; clf

e=0.6; w=4e6; s=1/(1+2*e*%s/w+%s^2/w^2);
low=1e-9;
mid=2e-9;
hig=1e-8;

// function to setup the 
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

t=[0];
y=[low];
u=[low];
i=1;

//From tf2ss(syslin('c',s))
a12=4194304; a21=-3814697.3; a22=-4800000;
b2=4000000; c1=0.9536743;
h=low;

//State initialization
x1=y(1)/c1;
x2=(h*a21*x1+h*b2*u(1))/(-h^2*a12*a21+h*a22);
x2
while(t(i)<4.45e-6 & i<10000)
    t(i+1)=t(i)+y(i); //Variable integration step
    //t(i+1)=t(i)+low; //Fixed integration step
    u(i+1)=uvalue(t(i+1));
    h=t(i+1)-t(i);
    
    //x2=(x2+h*a21*x1+h*b2*u(i))/(1-h^2*a21*a12-h*a22);
    //x1=x1+h*a12*x2;
	x2_new=(x2+h*a21*x1+h*b2*u(i))/(1-h*a22);
	x1=x1+h*a12*x2_new;
	x2=x2_new;
    y(i+1)=max(5e-10,c1*x1);
    
    i=i+1;
end

t2=0:1e-9:t($);
u2=(hig-low)*step(t2,0)-(hig-mid)*step(t2,0.5e-6)-...
   (mid-low)*step(t2,1e-6)+(mid-low)*step(t2,1.5e-6)+...
   (hig-mid)*step(t2,2e-6);
y2=csim(u2,t2,syslin('c',tf2ss(s)));
y2=low*ones(y2)+y2;

u(1)=hig;
xlabel("Time [s]",'fontsize',2);
ylabel("PLL output  period [s]",'fontsize',2);
ax=gca();
ax.font_size=2;
plot(t,u,'k',t,y,'b');


t2=0:1e-9:t($);
u2=(hig-low)*step(t2,0)-(hig-mid)*step(t2,0.5e-6)-...
   (mid-low)*step(t2,1e-6)+(mid-low)*step(t2,1.5e-6)+...
   (hig-mid)*step(t2,2e-6);
y2=csim(u2,t2,syslin('c',tf2ss(s)));
y2=low*ones(y2)+y2;

plot(t,u,'k',t,y,'r',t2,y2,'g');
// plot(t,u,'k',t,y,'r',t2,y2,'g'); //Plot period
//plot(t,ones(u)./u,'k',t,ones(y)./y,'r',t2,ones(y2)./y2,'g'); //Plot freq
