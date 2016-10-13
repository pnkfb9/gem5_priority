./build/ALPHA_MESI_CMP_directory/gem5.opt  ./configs/example/se_alpha_ruby_loop.py --cpu-type=detailed --num-cpus=16 --cmd "tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello" --caches --l2cache --num-dirs=4 --num-l2caches=16 --ruby --topology MeshDirCorners --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=1GHz --clock=1GHz --vcs-per-vnet=1 --mesh-rows=4 --buffers-per-data-vc=4 --buffers-per-ctrl-vc=4 > prova.res
echo "\n"
tail -n7 prova.res

octave --silent --eval 'm=dlmread("./m5out/buf_usage.txt",",",1,2);mean(mean(m))'
echo "\n"

./build/ALPHA_MESI_CMP_directory/gem5.opt ./configs/example/se_alpha_ruby_loop.py --cpu-type=detailed --num-cpus=16 --cmd "tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello" --caches --l2cache --num-dirs=4 --num-l2caches=16 --ruby --topology MeshDirCorners --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=1GHz --clock=1GHz --vcs-per-vnet=2 --mesh-rows=4 --buffers-per-data-vc=4 --buffers-per-ctrl-vc=4 > prova.res 

echo "\n"
tail -n7 prova.res

octave --silent --eval 'm=dlmread("./m5out/buf_usage.txt",",",1,2);mean(mean(m))'
echo "\n"


./build/ALPHA_MESI_CMP_directory/gem5.opt  ./configs/example/se_alpha_ruby_loop.py --cpu-type=detailed --num-cpus=16 --cmd "tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello" --caches --l2cache --num-dirs=4 --num-l2caches=16 --ruby --topology MeshDirCorners --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=1GHz --clock=1GHz --vcs-per-vnet=4 --mesh-rows=4 --buffers-per-data-vc=4 --buffers-per-ctrl-vc=4 > prova.res 

echo "\n" 
tail -n7 prova.res

octave --silent --eval 'm=dlmread("./m5out/buf_usage.txt",",",1,2);mean(mean(m))'
echo "\n"


./build/ALPHA_MESI_CMP_directory/gem5.opt ./configs/example/se_alpha_ruby_loop.py --cpu-type=detailed --num-cpus=16 --cmd "tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello" --caches --l2cache --num-dirs=4 --num-l2caches=16 --ruby --topology MeshDirCorners --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=1GHz --clock=1GHz --vcs-per-vnet=6 --mesh-rows=4 --buffers-per-data-vc=4 --buffers-per-ctrl-vc=4 > prova.res

echo "\n" 
tail -n7 prova.res

octave --silent --eval 'm=dlmread("./m5out/buf_usage.txt",",",1,2);mean(mean(m))'
echo "\n"

./build/ALPHA_MESI_CMP_directory/gem5.opt ./configs/example/se_alpha_ruby_loop.py --cpu-type=detailed --num-cpus=16 --cmd "tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello;tests/test-progs/hello/bin/alpha/linux/hello" --caches --l2cache --num-dirs=4 --num-l2caches=16 --ruby --topology MeshDirCorners --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=1GHz --clock=1GHz --vcs-per-vnet=8 --mesh-rows=4 --buffers-per-data-vc=4 --buffers-per-ctrl-vc=4 > prova.res

echo "\n" 
tail -n7 prova.res

octave --silent --eval 'm=dlmread("./m5out/buf_usage.txt",",",1,2);mean(mean(m))'
echo "\n"


