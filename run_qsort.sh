SUFFIX=.opt
VCS=2
CPUMODE='timing'
FREQ='1GHz'
FREQNOC='1GHz'
gem5_home='.'
R=5
sample=1s
sim=1s
num_data_buf_size=7
num_ctrl_buf_size=7
num_cpus=16

mesh_type='MeshDirCorners'
num_cpus_per_tile=1

NUM_VNET_SPURIOUS=0
l2linesize=128

echo 0.04 > k.txt
name=qsort
benchname="../benchmarks/mibench_automotive/${name}/${name}_small"
echo "testing $benchname ..."
input_str="../benchmarks/mibench_automotive/${name}/input_small.dat"

time $gem5_home/build/ALPHA_MOESI_CMP_directory/gem5$SUFFIX $gem5_home/configs/example/se_alpha_ruby_loop_i.py --cpu-type=timing --num-cpus=16 --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=16 --ruby --topology MeshDirCorners --garnet-network fixed --simulation-time=$sim --sample-time=$sample --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize

