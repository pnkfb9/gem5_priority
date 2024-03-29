#!/bin/bash

SUFFIX=.opt
VCS=2
CPUMODE='detailed'
FREQ='1GHz'
FREQNOC='1GHz'

gem5_home='.'
R=5
num_data_buf_size=7
num_ctrl_buf_size=7
max_non_atomic_pkt_per_vc=2
num_cpus=64

mesh_type='MeshDirCorners'
num_cpus_per_tile=1

num_l2=64; #echo $(( $num_cpus / num_cpus_per_tile ));

NUM_VNET_SPURIOUS=2
l2linesize=64

echo 0.04 > k.txt
name=hello
benchname="tests/test-progs/hello/bin/alpha/linux/hello"
echo "testing $benchname ..."

b1=";tests/test-progs/hello/bin/alpha/linux/hello"
bench_str="tests/test-progs/hello/bin/alpha/linux/hello"
for i in {2..64}; do bench_str=$bench_str";tests/test-progs/hello/bin/alpha/linux/hello"; echo $i; done;


time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5$SUFFIX $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=detailed --num-cpus=$num_cpus --cmd "$bench_str" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=8 --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc
