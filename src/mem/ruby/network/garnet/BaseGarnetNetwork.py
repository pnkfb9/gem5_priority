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
from Network import RubyNetwork

class BaseGarnetNetwork(RubyNetwork):
    type = 'BaseGarnetNetwork'
    cxx_header = "mem/ruby/network/garnet/BaseGarnetNetwork.hh"
    abstract = True
    ni_flit_size = Param.UInt8(8, "network interface flit size in bytes") # Note: was 16
    vcs_per_vnet = Param.UInt8(4, "virtual channels per virtual network");
    enable_fault_model = Param.Bool(False, "enable network fault model");
    fault_model = Param.FaultModel(NULL, "network fault model");
    
    synthetic_data_pkt_size = Param.UInt8(1,"Data pkt size in flits when using synthetic traffic");

    num_rows=Param.UInt8(4,"Number of rows for the considered 2D-mesh topology");
    num_cols=Param.UInt8(4,"Number of columns for the considered 2D-mesh topology");
    
    totVicharSlotPerVnet = Param.UInt8(8, "VICHAR slots per vnet in the input buffer");
    numFifoResynchSlots=Param.UInt8(4,"Number of slots in the FIFO resynchronizer, when used");

    router_frequency = Param.Counter(1000000000, "initial router frequency for each router");
    #buffers_per_data_vc = Param.Int(4, "Number of flit capacity for each physical FIFO DATA buffer [default 4]");
    #buffers_per_data_vc = Param.Int(1, "Number of flit capacity for each physical FIFO CONTROL buffer [default 1]");


