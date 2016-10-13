SIM_TIME=1s
SAMPLE_TIME=1s
SUFFIX='debug'
VCS=2
CPUMODE='detailed'
FREQ='1GHz'
gem5_home='.'
NUM_CPUS=16
R=5
mkdir m5out_all

# echo 0.01 > k.txt


name=sha
name_folder=sha
benchname="../benchmarks/mibench_security/${name_folder}/${name}"

input_str='../benchmarks/mibench_security/sha/input_small.asc'
for i in {2..16}; do input_str="$input_str;../benchmarks/mibench_security/sha/input_small.asc"; done


time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.$SUFFIX $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=detailed --num-cpus=16 --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=16 --ruby --topology MeshDirCorners --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 #--l1i_size=8kB --l1d_size=8kB --l2_size=128kB


mv m5out m5out_all/${name}
