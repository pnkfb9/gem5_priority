#!/bin/bash

#PROTOCOL=MOESI_CMP_directory
PROTOCOL=MESI_CMP_directory 

SUFFIX=.fast
VCS=2
CPUMODE='detailed'
FREQ='1GHz'
FREQNOC='1GHz'

gem5_home='.'
R=5
num_data_buf_size=4
num_ctrl_buf_size=4

totVicharSlotPerVnet=4

max_non_atomic_pkt_per_vc=4
num_cpus=64

mesh_type='MeshDirCorners'
num_cpus_per_tile=1
mesh_rows=8

##num_l2=$(( $num_cpus / $num_cpus_per_tile ));
num_l2=64

NUM_VNET_SPURIOUS=0
l2linesize=64
numFifoResynchSlots=8

mkdir m5out_all

echo 0.04 > k.txt

echo "OCEAN-contiguous"
name="OCEAN"
rootdir="../v1-splash-alpha/splash2/codes/apps/ocean/contiguous_partitions/"
bench_name="$rootdir${name}"
cmd_options="-p${num_cpus}"

time $gem5_home/build/ALPHA_$PROTOCOL/gem5$SUFFIX $gem5_home/configs/example/se_splash2.py --rootdir=${rootdir} --cpu-type=detailed --num-cpus=$num_cpus --cmd=$bench_name --options="$cmd_options" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --vcs-per-vnet=$VCS --mesh-rows=8 --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots}
mv m5out m5out_all/${name}

echo "RADIX"
name=RADIX
rootdir="../v1-splash-alpha/splash2/codes/kernels/radix/"
bench_name="$rootdir${name}"
cmd_options="-p${num_cpus} -n524288"

time $gem5_home/build/ALPHA_$PROTOCOL/gem5$SUFFIX $gem5_home/configs/example/se_splash2.py --rootdir=${rootdir} --cpu-type=detailed --num-cpus=$num_cpus --cmd=$bench_name --options="$cmd_options" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --vcs-per-vnet=$VCS --mesh-rows=8 --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots}
mv m5out m5out_all/${name}

echo "LU"
name="LU"
rootdir="../v1-splash-alpha/splash2/codes/kernels/lu/contiguous_blocks/"
bench_name="$rootdir${name}"
cmd_options="-p${num_cpus}"

time $gem5_home/build/ALPHA_$PROTOCOL/gem5$SUFFIX $gem5_home/configs/example/se_splash2.py --rootdir=${rootdir} --cpu-type=detailed --num-cpus=$num_cpus --cmd=$bench_name --options="$cmd_options" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --vcs-per-vnet=$VCS --mesh-rows=8 --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots}
mv m5out m5out_all/${name}

echo "WATER-SPATIAL"
name="WATER-SPATIAL"
rootdir="../v1-splash-alpha/splash2/codes/apps/water-spatial/"
bench_name="$rootdir${name}"
cmd_options=""

time $gem5_home/build/ALPHA_$PROTOCOL/gem5$SUFFIX $gem5_home/configs/example/se_splash2.py --input="${rootdir}input.p${num_cpus}" --rootdir=${rootdir} --cpu-type=detailed --num-cpus=$num_cpus --cmd=$bench_name --options="$cmd_options" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --vcs-per-vnet=$VCS --mesh-rows=8 --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots}
mv m5out m5out_all/${name}


echo "RAYTRACE"
name="RAYTRACE"
rootdir="../v1-splash-alpha/splash2/codes/apps/raytrace/"
bench_name="$rootdir${name}"
cmd_options="-p64 ./inputs/car.env"

time $gem5_home/build/ALPHA_$PROTOCOL/gem5$SUFFIX $gem5_home/configs/example/se_splash2.py --rootdir=${rootdir} --cpu-type=detailed --num-cpus=$num_cpus --cmd=$bench_name --options="$cmd_options" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --vcs-per-vnet=$VCS --mesh-rows=$mesh_rows --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots}
mv m5out m5out_all/${name}



echo "CHOLESKY"
name="CHOLESKY"
rootdir="../v1-splash-alpha/splash2/codes/kernels/cholesky/"
bench_name="$rootdir${name}"
cmd_options="-p${num_cpus} ./inputs/tk14.O"

time $gem5_home/build/ALPHA_$PROTOCOL/gem5$SUFFIX $gem5_home/configs/example/se_splash2.py --rootdir=${rootdir} --cpu-type=detailed --num-cpus=$num_cpus --cmd=$bench_name --options="$cmd_options" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --vcs-per-vnet=$VCS --mesh-rows=8 --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots}
mv m5out m5out_all/${name}

echo "FFT"
name="FFT"
rootdir="../v1-splash-alpha/splash2/codes/kernels/fft/"
bench_name="$rootdir${name}"
cmd_options="-p${num_cpus} -m16"

time $gem5_home/build/ALPHA_$PROTOCOL/gem5$SUFFIX $gem5_home/configs/example/se_splash2.py --rootdir=${rootdir} --cpu-type=detailed --num-cpus=$num_cpus --cmd=$bench_name --options="$cmd_options" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --vcs-per-vnet=$VCS --mesh-rows=4 --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots}
mv m5out m5out_all/${name}


echo "WATER-NSQUARED"
name="WATER-NSQUARED"
rootdir="../v1-splash-alpha/splash2/codes/apps/water-nsquared/"
bench_name="$rootdir${name}"
cmd_options=""

time $gem5_home/build/ALPHA_$PROTOCOL/gem5$SUFFIX $gem5_home/configs/example/se_splash2.py --input="${rootdir}input.p${num_cpus}" --rootdir=${rootdir} --cpu-type=detailed --num-cpus=$num_cpus --cmd=$bench_name --options="$cmd_options" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --vcs-per-vnet=$VCS --mesh-rows=$mesh_rows --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots}
mv m5out m5out_all/${name}



echo "BARNES"
name="BARNES"
rootdir="../v1-splash-alpha/splash2/codes/apps/barnes/"
bench_name="$rootdir${name}"
cmd_options=""

time $gem5_home/build/ALPHA_$PROTOCOL/gem5$SUFFIX $gem5_home/configs/example/se_splash2.py --input="${rootdir}input.p${num_cpus}" --rootdir=${rootdir} --cpu-type=detailed --num-cpus=$num_cpus --cmd=$bench_name --options="$cmd_options" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --vcs-per-vnet=$VCS --mesh-rows=$mesh_rows --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots}
mv m5out m5out_all/${name}

echo "FMM"
name="FMM"
rootdir="../v1-splash-alpha/splash2/codes/apps/fmm/"
bench_name="$rootdir${name}"
cmd_options="./inputs/input.2048.p${num_cpus}"

time $gem5_home/build/ALPHA_$PROTOCOL/gem5$SUFFIX $gem5_home/configs/example/se_splash2.py --input="${rootdir}/inputs/input.2048.p${num_cpus}" --rootdir=${rootdir} --cpu-type=detailed --num-cpus=$num_cpus --cmd=$bench_name --options="$cmd_options" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --vcs-per-vnet=$VCS --mesh-rows=4 --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots}
mv m5out m5out_all/${name}




