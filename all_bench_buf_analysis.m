
filename=sprintf('./m5out/buf_usage.txt');
g=dlmread(filename,',',0,1);
figure, bar3(g')

