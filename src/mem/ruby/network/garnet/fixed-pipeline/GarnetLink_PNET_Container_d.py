#
# Authors: Davide Zoni

from m5.params import *
from m5.proxy import *
from ClockedObject import ClockedObject
from BasicLink import BasicIntLink, BasicExtLink

class NetworkLink_PNET_Container_d(ClockedObject):
    type = 'NetworkLink_PNET_Container_d'
    cxx_header = "mem/ruby/network/garnet/fixed-pipeline/NetworkLink_PNET_Container_d.hh"
    link_id = Param.Int(Parent.link_id, "link id")
    link_latency = Param.Int(Parent.latency, "link latency")
    vcs_per_vnet = Param.Int(Parent.vcs_per_vnet,
                              "virtual channels per virtual network")
    virt_nets = Param.Int(Parent.number_of_virtual_networks,
                          "number of virtual networks")
    virt_nets_spurious = Param.Int(Parent.number_of_virtual_networks_spurious,
                          "number of virtual networks spurious to be used with VNET_REUSE")
    channel_width = Param.Int(Parent.bandwidth_factor,
                              "channel width == bw factor")

class CreditLink_PNET_Container_d(NetworkLink_PNET_Container_d):
    type = 'CreditLink_PNET_Container_d'
    cxx_header = "mem/ruby/network/garnet/fixed-pipeline/CreditLink_PNET_Container_d.hh"

# Interior fixed pipeline links between routers
class GarnetIntLink_PNET_Container_d(BasicIntLink):
    type = 'GarnetIntLink_PNET_Container_d'
    cxx_header = "mem/ruby/network/garnet/fixed-pipeline/GarnetLink_PNET_Container_d.hh"
    # The detailed fixed pipeline bi-directional link include two main
    # forward links and two backward flow-control links, one per direction
    nls = []
    # In uni-directional link
    nls.append(NetworkLink_PNET_Container_d()); 
    # Out uni-directional link
    nls.append(NetworkLink_PNET_Container_d());
    network_links = VectorParam.NetworkLink_PNET_Container_d(nls, "forward links")

    cls = []
    # In uni-directional link
    cls.append(CreditLink_PNET_Container_d());
    # Out uni-directional link
    cls.append(CreditLink_PNET_Container_d());
    credit_links = VectorParam.CreditLink_PNET_Container_d(cls, "backward flow-control links")

# Exterior fixed pipeline links between a router and a controller
class GarnetExtLink_PNET_Container_d(BasicExtLink):
    type = 'GarnetExtLink_PNET_Container_d'
    cxx_header = "mem/ruby/network/garnet/fixed-pipeline/GarnetLink_PNET_Container_d.hh"
    # The detailed fixed pipeline bi-directional link include two main
    # forward links and two backward flow-control links, one per direction
    nls = []
    # In uni-directional link
    nls.append(NetworkLink_PNET_Container_d());
    # Out uni-directional link
    nls.append(NetworkLink_PNET_Container_d());
    network_links = VectorParam.NetworkLink_PNET_Container_d(nls, "forward links")

    cls = []
    # In uni-directional link
    cls.append(CreditLink_PNET_Container_d());
    # Out uni-directional link
    cls.append(CreditLink_PNET_Container_d());
    credit_links = VectorParam.CreditLink_PNET_Container_d(cls, "backward flow-control links")
