SUFFIX=.opt
VCS=2
CPUMODE='detailed'
FREQ='1GHz'
gem5_home='.'
R=5

echo 0.04 > k.txt
name=qsort
benchname="../benchmarks/mibench_automotive/${name}/${name}_small"
echo "testing $benchname ..."
input_str="../benchmarks/mibench_automotive/${name}/input_small.dat"

time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5$SUFFIX $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=detailed --num-cpus=16 --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=16 --ruby --topology MeshDirCorners --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4

