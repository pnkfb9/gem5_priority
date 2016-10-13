SIM_TIME=1s
SAMPLE_TIME=1s
SUFFIX='opt'
VCS=4
CPUMODE='detailed'
FREQ='2GHz'
gem5_home='.'

num_cpus=12
num_l2=4; #echo $(( $num_cpus / num_cpus_per_tile ));
mesh_type='MeshDirCornersEnhanced'
num_cpus_per_tile=3

name=qsort_large_loop
name_folder=qsort
benchname="../benchmarks/mibench_automotive/${name_folder}/${name}"

input_str='../benchmarks/mibench_automotive/qsort/input_large.dat'
for i in {2..16}; do input_str="$input_str;../benchmarks/mibench_automotive/qsort/input_large.dat"; done

time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.$SUFFIX $gem5_home/configs/example/se_alpha_ruby_periodic_dump.py --cpu-type=detailed --num-cpus=$num_cpus --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=2 --dump-period=0.0001 --listen=1234 --l1i_size=64kB --l1d_size=64kB --l2_size=1MB --cacheline_size=64 --l2_cacheline_size=128
