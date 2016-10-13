# Copyright (c) 2013 Politecnico di Milano
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
# Authors: Davide Zoni
#
#
# Starting from the MEhDirCorners topology this topology enhanced it enabling
# the possibility for multiple cpus inside the same tile. However, the MC are on
# the corner side of the topology as in the baseline one.
# 
# routers with low ids are internals, while higher ids are for the external/NI
# ones
# 

from m5.params import *
from m5.objects import *

from BaseTopology import SimpleTopology

class MeshDirCornersEnhanced(SimpleTopology):
    description='MeshDirCornersEnhanced'

    def __init__(self, controllers):
        self.nodes = controllers

    # This file contains a special network creation function.  This
    # networks is not general and will only work with specific system
    # configurations.  The network specified is similar to GEMS old file
    # specified network.

    def makeTopology(self, options, IntLink, ExtLink, Router):
        nodes = self.nodes
		# added for the new topology
        num_cpus_per_tile = options.num_cpus_per_tile
#        total_num_routers = 2/num_cpus_per_tile*options.num_cpus
        
        num_routers = options.num_cpus / num_cpus_per_tile# these are the internal routers
        total_num_routers = 2*num_routers
        num_external_routers = total_num_routers-num_routers # router mimiking the enhanced NI

        num_rows = options.mesh_rows

        # First determine which nodes are cache cntrls vs. dirs vs. dma
        cache_nodes = []
        dir_nodes = []
        dma_nodes = []
        for node in nodes:
            if node.type == 'L1Cache_Controller' or \
            node.type == 'L2Cache_Controller':
                cache_nodes.append(node)
            elif node.type == 'Directory_Controller':
                dir_nodes.append(node)
            elif node.type == 'DMA_Controller':
                dma_nodes.append(node)

        # Obviously the number or rows must be <= the number of routers
        # and evenly divisible.  Also the number of caches must be a
        # multiple of the number of routers and the number of directories
        # must be four.

		# considering num_routers as internal routers not connected to any PE,
		# L2 or MC
        assert(num_rows <= num_routers) # ok checked
        num_columns = int(num_routers / num_rows) # ok checked
        assert(num_columns * num_rows == num_routers)

		# the cache_nodes are distributed on the external routers which mimic
		# the enhanced NI

        print("cpus: {}  router: {} [internal: {} external: {}] rows: {} cols: {}\n".format(options.num_cpus,total_num_routers,num_routers,num_external_routers,num_rows,num_columns))
        caches_per_router, remainder = divmod(len(cache_nodes), num_external_routers)
        #caches_per_router, remainder = divmod(len(cache_nodes), num_routers)
        assert(remainder == 0)
        assert(len(dir_nodes) == 4)

        # Create the routers in the mesh, both external and internal

		
        routers = [Router(router_id=i) for i in range(total_num_routers)]
        
		# link counter to set unique link ids
        link_count = 0

        # Connect each cache controller to the appropriate external router
        ext_links = []
        for (i, n) in enumerate(cache_nodes):
            cntrl_level, router_id = divmod(i, num_external_routers)
            # router_id = the internal router, while 
			# router_id_ext = the corresponding external router offsets by
			# num_routers
            router_id_ext = router_id + num_routers

            assert(cntrl_level < caches_per_router)
            ext_links.append(ExtLink(link_id=link_count, ext_node=n,
                                    int_node=routers[router_id_ext]))

            link_count += 1

        ####################################################################
        ###### Connect the dir nodes to the corners    #####################
        ####################################################################
        ext_links.append(ExtLink(link_id=link_count, ext_node=dir_nodes[0],
                                int_node=routers[0+num_routers]))
        link_count += 1
        ext_links.append(ExtLink(link_id=link_count, ext_node=dir_nodes[1],
                                int_node=routers[num_columns - 1+num_routers]))
        link_count += 1
        ext_links.append(ExtLink(link_id=link_count, ext_node=dir_nodes[2],
                                int_node=routers[num_routers - num_columns+num_routers]))
        link_count += 1
        ext_links.append(ExtLink(link_id=link_count, ext_node=dir_nodes[3],
                                int_node=routers[num_routers - 1+num_routers]))
        link_count += 1
        #####################################################################
		#################### END CONNECT DIR NODES  #########################
		#####################################################################

        # Connect the dma nodes to router 0.  These should only be DMA nodes.
        for (i, node) in enumerate(dma_nodes):
            assert(node.type == 'DMA_Controller')
            ext_links.append(ExtLink(link_id=link_count, ext_node=node, int_node=routers[0]))

        # Create the mesh links.  First row (east-west) links then column
        # (north-south) links
        int_links = []
        for row in xrange(num_rows):
            for col in xrange(num_columns):
                if (col + 1 < num_columns):
                    east_id = col + (row * num_columns)
                    west_id = (col + 1) + (row * num_columns)
                    int_links.append(IntLink(link_id=link_count,
                                            node_a=routers[east_id],
                                            node_b=routers[west_id],
                                            weight=1))
                    link_count += 1

        for col in xrange(num_columns):
            for row in xrange(num_rows):
                if (row + 1 < num_rows):
                    north_id = col + (row * num_columns)
                    south_id = col + ((row + 1) * num_columns)
                    int_links.append(IntLink(link_id=link_count,
                                            node_a=routers[north_id],
                                            node_b=routers[south_id],
                                            weight=2))
                    link_count += 1

        # connect the external routers with the associate internal ones
        for n in xrange(num_routers):
            n_ext = n + num_routers
            int_links.append(IntLink(link_id=link_count, node_a=routers[n], node_b=routers[n_ext], weight=1))
            link_count += 1

        return routers, int_links, ext_links
