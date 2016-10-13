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
from BaseGarnetNetwork import BaseGarnetNetwork

class GarnetNetwork_d(BaseGarnetNetwork):
    type = 'GarnetNetwork_d'
    cxx_header = "mem/ruby/network/garnet/fixed-pipeline/GarnetNetwork_d.hh"
    buffers_per_data_vc = Param.UInt8(Parent.buffers_per_data_vc, "buffers per data virtual channel");
    buffers_per_ctrl_vc = Param.UInt8(Parent.buffers_per_ctrl_vc, "buffers per ctrl virtual channel");
    totVicharSlotPerVnet = Param.UInt8(Parent.totVicharSlotPerVnet, "VICHAR slots per vnet in the input buffer");
    max_non_atomic_pkt_per_vc = Param.UInt8(Parent.max_non_atomic_pkt_per_vc, "Maximum pkt storable at the same time in a single VC [ACTIVE WHEN USE_NON_ATOMIC_VC_ALLOC=true]");
    adaptive_routing_num_escape_paths = Param.UInt8(1, "escape paths for the adaptive routing algorithm if any");

    synthetic_data_pkt_size = Param.UInt8(Parent.synthetic_data_pkt_size,"Data pkt size in flits when using synthetic traffic");
