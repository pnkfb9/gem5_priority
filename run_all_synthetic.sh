#!/bin/bash

SUFFIX=.fast
VCS=1
CPUMODE='detailed'
FREQ='1GHz'
FREQNOC='1GHz'

gem5_home='.'
R=5
num_data_buf_size=4
num_ctrl_buf_size=4
max_non_atomic_pkt_per_vc=4
num_cpus=64

totVicharSlotPerVnet=8

mesh_type='Mesh'
num_cpus_per_tile=1
num_dirs=64
mesh_rows=8

num_l2=64; #echo $(( $num_cpus / num_cpus_per_tile ));

NUM_VNET_SPURIOUS=3
SYNTHETIC_DATA_PKT_SIZE=1
l2linesize=64
numFifoResynchSlots=20;


#INJ_RATE=0.3
#TRAFFIC_TYPE=0  #--synthetic 0=uniform 1=tornado 2=bitcomplement 


#echo 0.04 > k.txt
# --sim-cycles=1000000000
# doesn't work --maxpackets=100


SIM_TIME=100000000
for t in {0..3};
do
      TRAFFIC_TYPE=$t;

        #TRAFFIC_TYPE=0;
        # delete old files and dirs for the new traffic type
        rm -r m5out_all_network_test_traffic_type_$TRAFFIC_TYPE;
        rm final_output_network_test.txt;
        mkdir m5out_all_network_test_traffic_type_$TRAFFIC_TYPE;

        for i in {0..4};
        do
	       # i=7;
                INJ_RATE=0.${i}01;
                echo $INJ_RATE;
                name=INJ_RATE_$INJ_RATE;
                time $gem5_home/build/ALPHA_Network_test/gem5$SUFFIX $gem5_home/configs/example/ruby_network_test.py --num-cpus=$num_cpus --caches --l2cache --num-dirs=$num_dirs --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=$mesh_rows --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --synthetic=$TRAFFIC_TYPE --precision=3 --injectionrate=$INJ_RATE --maxtick=10000000000000 --sim-cycles=$SIM_TIME --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc  --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots} --synthetic-data-pkt-size=$SYNTHETIC_DATA_PKT_SIZE > out_network_test.txt

                mv m5out m5out_all_network_test_traffic_type_$TRAFFIC_TYPE/${name}
                echo "############### INJ_RATE${INJ_RATE} ############" >>final_output_network_test.txt;
                tail -n26 out_network_test.txt | head -n20 >>final_output_network_test.txt;

                INJ_RATE=0.${i}20;
                echo $INJ_RATE;
                name=INJ_RATE_$INJ_RATE;


                time $gem5_home/build/ALPHA_Network_test/gem5$SUFFIX $gem5_home/configs/example/ruby_network_test.py --num-cpus=$num_cpus --caches --l2cache --num-dirs=$num_dirs --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=$mesh_rows --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --synthetic=$TRAFFIC_TYPE --precision=3 --injectionrate=$INJ_RATE --maxtick=10000000000000 --sim-cycles=$SIM_TIME --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots} --synthetic-data-pkt-size=$SYNTHETIC_DATA_PKT_SIZE> out_network_test.txt

                mv m5out m5out_all_network_test_traffic_type_$TRAFFIC_TYPE/${name}

                echo "############### INJ_RATE${INJ_RATE} ############" >>final_output_network_test.txt;
                tail -n26 out_network_test.txt | head -n20 >>final_output_network_test.txt;

                INJ_RATE=0.${i}40;
                echo $INJ_RATE;
                name=INJ_RATE_$INJ_RATE;

                time $gem5_home/build/ALPHA_Network_test/gem5$SUFFIX $gem5_home/configs/example/ruby_network_test.py --num-cpus=$num_cpus --caches --l2cache --num-dirs=$num_dirs --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=$mesh_rows --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --synthetic=$TRAFFIC_TYPE --precision=3 --injectionrate=$INJ_RATE --maxtick=10000000000000 --sim-cycles=$SIM_TIME --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots} --synthetic-data-pkt-size=$SYNTHETIC_DATA_PKT_SIZE> out_network_test.txt

                mv m5out m5out_all_network_test_traffic_type_$TRAFFIC_TYPE/${name}
                echo "############### INJ_RATE${INJ_RATE} ############" >>final_output_network_test.txt;
                tail -n26 out_network_test.txt | head -n20 >>final_output_network_test.txt;


                INJ_RATE=0.${i}60;
                echo $INJ_RATE;
                name=INJ_RATE_$INJ_RATE;

            time $gem5_home/build/ALPHA_Network_test/gem5$SUFFIX $gem5_home/configs/example/ruby_network_test.py --num-cpus=$num_cpus --caches --l2cache --num-dirs=$num_dirs --num-l2caches=$num_l2     --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=$mesh_rows --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --synthetic=$TRAFFIC_TYPE --precision=3 --injectionrate=$INJ_RATE --maxtick=10000000000000 --sim-cycles=$SIM_TIME --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc  --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots} --synthetic-data-pkt-size=$SYNTHETIC_DATA_PKT_SIZE> out_network_test.txt

                mv m5out m5out_all_network_test_traffic_type_$TRAFFIC_TYPE/${name}

                echo "############### INJ_RATE${INJ_RATE} ############" >>final_output_network_test.txt;
                tail -n26 out_network_test.txt | head -n20 >>final_output_network_test.txt;

                INJ_RATE=0.${i}80;
                echo $INJ_RATE;
                name=INJ_RATE_$INJ_RATE;

                time $gem5_home/build/ALPHA_Network_test/gem5$SUFFIX $gem5_home/configs/example/ruby_network_test.py --num-cpus=$num_cpus --caches --l2cache --num-dirs=$num_dirs --num-l2caches=$num_l2 --ruby --topology $mesh_type --num-cpus-per-tile=$num_cpus_per_tile --garnet-network fixed --simulation-time=250ms --sample-time=250ms --internal-sampling-freq=1ps --router-frequency=$FREQ --clock=$FREQ --vcs-per-vnet=$VCS --mesh-rows=$mesh_rows --buffers-per-data-vc=$num_data_buf_size --buffers-per-ctrl-vc=$num_ctrl_buf_size --number_of_virtual_networks_spurious=$NUM_VNET_SPURIOUS --l2_cacheline_size=$l2linesize --synthetic=$TRAFFIC_TYPE --precision=3 --injectionrate=$INJ_RATE --maxtick=10000000000000 --sim-cycles=$SIM_TIME --max-non-atomic-pkt-per-vc=$max_non_atomic_pkt_per_vc  --totVicharSlotPerVnet=$totVicharSlotPerVnet --numFifoResynchSlots=${numFifoResynchSlots} --synthetic-data-pkt-size=$SYNTHETIC_DATA_PKT_SIZE> out_network_test.txt

                mv m5out m5out_all_network_test_traffic_type_$TRAFFIC_TYPE/${name}

                echo "############### INJ_RATE${INJ_RATE} ############" >>final_output_network_test.txt;
                tail -n26 out_network_test.txt | head -n20 >>final_output_network_test.txt;


        done

        mv final_output_network_test.txt m5out_all_network_test_traffic_type_$TRAFFIC_TYPE
done
