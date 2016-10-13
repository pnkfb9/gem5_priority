# channel width 8B,4vcs,timing cpu model MESI
SIM_TIME=1s
SAMPLE_TIME=1s
SUFFIX='opt'
VCS=2
CPUMODE='detailed'
FREQ='1GHz'
gem5_home='.'
NUM_CPUS=16
R=5
num_data_buf_size=8
num_ctrl_buf_size=8

mesh_type='MeshDirCornersEnhanced'
num_cpus_per_tile=1

NUM_VNET_SPURIOUS=0
l2linesize=64
numFifoResynchSlots=20

mkdir m5out_all

echo 0.01 > k.txt

name='hello'
benchname=tests/test-progs/hello/bin/alpha/linux/${name}

echo "testing $benchname ..."
time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=${CPUMODE} --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=${NUM_CPUS} --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=${SIM_TIME} --sample-time=${SAMPLE_TIME} --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile

mv m5out m5out_all/${name}


name='susan'
benchname=../benchmarks/mibench_automotive/${name}/${name}

input_str="../benchmarks/mibench_automotive/${name}/input_small.pgm ./m5out/susan_out1.png -c"
for i in {2..16}; do input_str=$input_str";../benchmarks/mibench_automotive/susan/input_small.pgm ./m5out/susan_out$i.png -c"; done;

echo "testing $benchname ..."
time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=${CPUMODE} --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=${NUM_CPUS} --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=${SIM_TIME} --sample-time=${SAMPLE_TIME} --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile

mv m5out m5out_all/${name}


####name='patricia'
####benchname="../benchmarks/mibench_network/${name}/${name}"
####echo "testing $benchname ..."
####input_str="../benchmarks/mibench_network/${name}/small.udp"
####
####time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=${CPUMODE} --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=${NUM_CPUS} --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=${SIM_TIME} --sample-time=${SAMPLE_TIME} --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile 
####
####mv m5out m5out_all/${name}

name='qsort'
benchname="../benchmarks/mibench_automotive/${name}/${name}_small"
echo "testing $benchname ..."
input_str="../benchmarks/mibench_automotive/${name}/input_small.dat"


time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=${CPUMODE} --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str;$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=${NUM_CPUS} --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=${SIM_TIME} --sample-time=${SAMPLE_TIME} --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile

mv m5out m5out_all/${name}

echo "testing sha..."
name=sha
name_folder=sha
benchname="../benchmarks/mibench_security/${name_folder}/${name}"

input_str='../benchmarks/mibench_security/sha/input_small.asc'
for i in {2..16}; do input_str="$input_str;../benchmarks/mibench_security/sha/input_small.asc"; done

time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.$SUFFIX $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=detailed --num-cpus=16 --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=16 --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile
mv m5out m5out_all/${name} 

echo "testing fft..."
name='fft'
nameBIG='FFT'
benchname="../benchmarks/mibench_telecomm/${nameBIG}/${name}"

input_str="4 4096"
for i in {2..16}; do input_str=$input_str";4 4096"; done;

time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=$CPUMODE --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=${NUM_CPUS} --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=${SIM_TIME} --sample-time=${SAMPLE_TIME} --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile

mv m5out m5out_all/${name}


#####echo "testing crc32..."
#####name='crc'
#####nameBIG='CRC32'
#####benchname="../benchmarks/mibench_telecomm/${nameBIG}/${name}"
#####
#####input_str='../benchmarks/mibench_telecomm/adpcm/data/large.pcm'
#####for i in {2..16}; do input_str="$input_str;../benchmarks/mibench_telecomm/adpcm/data/large.pcm"; done
#####
#####time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=$CPUMODE --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o $input_str --caches --l2cache --num-dirs=4 --num-l2caches=${NUM_CPUS} --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=${SIM_TIME} --sample-time=${SAMPLE_TIME} --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile
#####
#####mv m5out m5out_all/${name}



echo "testing search_small..."
name='search_small'
nameBIG='stringsearch'
benchname="../benchmarks/mibench_office/${nameBIG}/${name}"

time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=$CPUMODE --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" --caches --l2cache --num-dirs=4 --num-l2caches=${NUM_CPUS} --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=${SIM_TIME} --sample-time=${SAMPLE_TIME} --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile

mv m5out m5out_all/${name}



echo "testing dijkstra_small..."
name='dijkstra'
benchname="../benchmarks/mibench_network/${name}/${name}_small"

input_str='../benchmarks/mibench_network/dijkstra/input.dat'
for i in {2..16}; do input_str="$input_str;../benchmarks/mibench_network/dijkstra/input.dat"; done

time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=$CPUMODE --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o $input_str --caches --l2cache --num-dirs=4 --num-l2caches=${NUM_CPUS} --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=${SIM_TIME} --sample-time=${SAMPLE_TIME} --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile

mv m5out m5out_all/${name}


echo "testing bitcount 1000..."
name='bitcnts'
nameBIG='bitcount'
benchname="../benchmarks/mibench_automotive/${nameBIG}/${name}"

input_str='75000'
for i in {2..16}; do input_str="$input_str;75000"; done

time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=$CPUMODE --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o $input_str --caches --l2cache --num-dirs=4 --num-l2caches=${NUM_CPUS} --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=${SIM_TIME} --sample-time=${SAMPLE_TIME} --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size  --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile

mv m5out m5out_all/${name}


echo "testing basichmath small..."
name='basicmath_small'
nameBIG='basicmath'
benchname="../benchmarks/mibench_automotive/${nameBIG}/${name}"

time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.${SUFFIX} $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=$CPUMODE --num-cpus=${NUM_CPUS} --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" --caches --l2cache --num-dirs=4 --num-l2caches=${NUM_CPUS} --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=${SIM_TIME} --sample-time=${SAMPLE_TIME} --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile

mv m5out m5out_all/${name}


###name=bf
###name_folder=blowfish
###benchname="../benchmarks/mibench_security/${name_folder}/${name}"
###
###input_str="e ../benchmarks/mibench_security/${name_folder}/input_small.asc ../benchmarks/mibench_security/${name_folder}/output_small9.enc 1234567890abcdeffedcba0987654321"
###for i in {2..16}; do input_str="$input_str;e ../benchmarks/mibench_security/${name_folder}/input_small.asc ../benchmarks/mibench_security/${name_folder}/output_small9.enc 1234567890abcdeffedcba0987654321"; done
###
###time $gem5_home/build/ALPHA_MESI_CMP_directory/gem5.$SUFFIX $gem5_home/configs/example/se_alpha_ruby_loop.py --cpu-type=detailed --num-cpus=16 --cmd "$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname;$benchname" -o "$input_str" --caches --l2cache --num-dirs=4 --num-l2caches=16 --ruby --topology ${mesh_type} --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=4 --numFifoResynchSlots=${numFifoResynchSlots} --num-cpus-per-tile=$num_cpus_per_tile
###
###mv m5out m5out_all/${name}
