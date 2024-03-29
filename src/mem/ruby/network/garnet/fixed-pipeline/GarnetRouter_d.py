# Copyright (c) 2008 Princeton University
# Copyright (c) 2009 Advanced Micro Devices, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Steve Reinhardt
#          Brad Beckmann

from m5.params import *
from m5.proxy import *
from BasicRouter import BasicRouter

class GarnetRouter_d(BasicRouter):
    type = 'GarnetRouter_d'
    cxx_class = 'Router_d'
    cxx_header = "mem/ruby/network/garnet/fixed-pipeline/Router_d.hh"
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
