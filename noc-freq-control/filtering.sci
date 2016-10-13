clear;clc;clf
a=fscanfMat('plldump_qsort_16_R5_4vcs.txt');
b=fscanfMat('dump_qsort_small16_R5_4vcs.txt');

k=0.035;

figure(0);
plot(a(:,1),1000*ones(a(:,2))./a(:,2));

// figure(1);
u=min(1,0.1+k*b(:,15));
// Discretized PLL model, with Ts=10MHz, using the following maxima code
// g:1/(1 + 0.0000003*s + 6.25e-14*s^2);
// ratsimp(subst(s=(z-1)/(z*(1/10000000)),g));
pll=(4*%z^2)/(41*%z^2-62*%z+25);
filt=(1-0.99)/(%z-0.99);
t=1:length(u);
clf; plot(t,dsimul(tf2ss(pll*filt),u'));

rout=1.848/(%z-0.8462)//2.57/(%z-1);
l=pll*filt*rout;
figure(2);
clf;
//bode(syslin('d',k*l));
evans(l,7);
zgrid;
figure(3);
plot(dsimul(tf2ss((k*l)/(1+k*l)),ones(1:1000)));
