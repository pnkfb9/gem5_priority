# Authors: Davide Zoni

from m5.params import *
from m5.proxy import *
from BasicRouter import BasicRouter

class GarnetRouter_PNET_Container_d(BasicRouter):
    type = 'GarnetRouter_PNET_Container_d'
    cxx_class = 'Router_PNET_Container_d'
    cxx_header = "mem/ruby/network/garnet/fixed-pipeline/Router_PNET_Container_d.hh"
    vcs_per_vnet = Param.Int(Parent.vcs_per_vnet,
                              "virtual channels per virtual network")
    virt_nets = Param.Int(Parent.number_of_virtual_networks,
                          "number of virtual networks")
    virt_nets_spurious = Param.Int(Parent.number_of_virtual_networks_spurious,
                          "number of virtual networks spurious to be used with VNET_REUSE")
   
	# param moved to garnet network to be distributed to both routers and NICs
    totVicharSlotPerVnet = Param.UInt8(Parent.totVicharSlotPerVnet, "VICHAR slots per vnet in the input buffer");
    numFifoResynchSlots=Param.UInt8(Parent.numFifoResynchSlots,"Number of slots in the FIFO resynchronizer, when used");
    router_frequency = Param.Counter(Parent.router_frequency, "initial router frequency for each router");
