SUFFIX=opt
VCS=2
CPUMODE='detailed'
FREQ='1GHz'
FREQNOC='1GHz'

gem5_home='.'
R=5
num_data_buf_size=4
num_ctrl_buf_size=4

totVicharSlotPerVnet=16

max_non_atomic_pkt_per_vc=4
NUM_CPUS=16

mesh_type='MeshDirCorners'
num_cpus_per_tile=1

num_l2=16; #echo $(( $num_cpus / num_cpus_per_tile ));

NUM_VNET_SPURIOUS=0
l2linesize=64



echo 0.04 > k.txt
echo "testing search_small..."
name='search_small'
nameBIG='stringsearch'
benchname="../benchmarks/mibench_office/${nameBIG}/${name}"

time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=$CPUMODE --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" --caches --l2cache --num-dirs=4 --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --router-frequency=$FREQNOC --clock=$FREQ --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet
