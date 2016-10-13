a=fscanfMat('1000to2000MHz0.6.txt');
b=fscanfMat('1000to2000MHz0.7.txt');
t1=a(:,1);
t2=b(:,1);
y1=1000*ones(t1)./a(:,2);
y2=1000*ones(t2)./b(:,2);
for i=length(t2):length(t1)
y2(i)=y2(length(t2));
end
t=t1-t1(1);
plot(t,y1,t,y2);
