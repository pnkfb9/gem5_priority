SUFFIX=.opt
VCS=2
CPUMODE='detailed'
gem5_home='.'
R=5

# echo 0.04 > k.txt
name=qsort
benchname="../benchmarks/mibench_automotive/${name}/${name}_small"
echo "testing $benchname ..."
input_str="../benchmarks/mibench_automotive/${name}/input_small.dat"

time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5$SUFFIX $gem5_home/configs/example/se_alpha_ruby_periodic_dump.py --cpu-type=detailed --num-cpus=2 --cmd "$benchname;$benchname" -o "$input_str;$input_str" --caches --l2cache --num-dirs=2 --num-l2caches=2 --ruby --topology Mesh --garnet-network fixed --internal-sampling-freq=1ps --router-frequency=500MHz --clock=3GHz --vcs-per-vnet=$VCS --mesh-rows=1 --dump-period=0.0001 --listen=1234 --l1i_size=8kB --l1d_size=8kB --l2_size=128kB

