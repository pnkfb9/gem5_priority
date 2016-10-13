# Copyright (c) 2012 ARM Limited
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2006-2008 The Regents of The University of Michigan
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

# Simple test script
#
# "m5 test.py"

import optparse
import sys
import socket

import m5
from m5.defines import buildEnv
from m5.objects import *
from m5.util import addToPath, fatal

addToPath('../common')
addToPath('../ruby')
addToPath('../topologies')

import Options
import Ruby
import Simulation
import CacheConfig
from Caches import *
from cpu2000 import *

def get_processes(options):
    """Interprets provided options and returns a list of processes"""

    multiprocesses = []
    inputs = []
    outputs = []
    errouts = []
    pargs = []

    workloads = options.cmd.split(';')
    if options.input != "":
        inputs = options.input.split(';')
    if options.output != "":
        outputs = options.output.split(';')
    if options.errout != "":
        errouts = options.errout.split(';')
    if options.options != "":
        pargs = options.options.split(';')

    idx = 0
    for wrkld in workloads:
        process = LiveProcess()
        process.executable = wrkld

        if len(pargs) > idx:
            process.cmd = [wrkld] + pargs[idx].split()
        else:
            process.cmd = [wrkld]

        if len(inputs) > idx:
            process.input = inputs[idx]
        if len(outputs) > idx:
            process.output = outputs[idx]
        if len(errouts) > idx:
            process.errout = errouts[idx]

        multiprocesses.append(process)
        idx += 1

    if options.smt:
        assert(options.cpu_type == "detailed" or options.cpu_type == "inorder")
        return multiprocesses, idx
    else:
        return multiprocesses, 1


parser = optparse.OptionParser()
Options.addCommonOptions(parser)
Options.addSEOptions(parser)

parser.add_option("--dump-period", type="float", default=1.0,
                  help="Period with which data is dumped [s]")
parser.add_option("--listen", type="int", default=0,
                  help="Listen on port X for connecting to the manager")

if '--ruby' in sys.argv:
    Ruby.define_options(parser)

(options, args) = parser.parse_args()

if args:
    print "Error: script doesn't take any positional arguments"
    sys.exit(1)

multiprocesses = []
numThreads = 1

if options.bench:
    apps = options.bench.split("-")
    if len(apps) != options.num_cpus:
        print "number of benchmarks not equal to set num_cpus!"
        sys.exit(1)

    for app in apps:
        try:
            if buildEnv['TARGET_ISA'] == 'alpha':
                exec("workload = %s('alpha', 'tru64', 'ref')" % app)
            else:
                exec("workload = %s(buildEnv['TARGET_ISA'], 'linux', 'ref')" % app)
            multiprocesses.append(workload.makeLiveProcess())
        except:
            print >>sys.stderr, "Unable to find workload for %s: %s" % (buildEnv['TARGET_ISA'], app)
            sys.exit(1)
elif options.cmd:
    multiprocesses, numThreads = get_processes(options)
else:
    print >> sys.stderr, "No workload specified. Exiting!\n"
    sys.exit(1)


(CPUClass, test_mem_mode, FutureClass) = Simulation.setCPUClass(options)
CPUClass.clock = options.clock
CPUClass.numThreads = numThreads

# Check -- do not allow SMT with multiple CPUs
if options.smt and options.num_cpus > 1:
    fatal("You cannot use SMT with multiple CPUs!")

np = options.num_cpus
system = System(cpu = [CPUClass(cpu_id=i) for i in xrange(np)],
                physmem = SimpleMemory(range=AddrRange("512MB")),
                membus = CoherentBus(), mem_mode = test_mem_mode)

# Sanity check
if options.fastmem and (options.caches or options.l2cache):
    fatal("You cannot use fastmem in combination with caches!")

for i in xrange(np):
    if options.smt:
        system.cpu[i].workload = multiprocesses
    elif len(multiprocesses) == 1:
        system.cpu[i].workload = multiprocesses[0]
    else:
        system.cpu[i].workload = multiprocesses[i]

    if options.fastmem:
        system.cpu[i].fastmem = True

    if options.checker:
        system.cpu[i].addCheckerCpu()

if options.ruby:
    if not (options.cpu_type == "detailed" or options.cpu_type == "timing"):
        print >> sys.stderr, "Ruby requires TimingSimpleCPU or O3CPU!!"
        sys.exit(1)

    options.use_map = True
    Ruby.create_system(options, system)
    assert(options.num_cpus == len(system.ruby._cpu_ruby_ports))

    for i in xrange(np):
        ruby_port = system.ruby._cpu_ruby_ports[i]

        # Create the interrupt controller and connect its ports to Ruby
        system.cpu[i].createInterruptController()
        #system.cpu[i].interrupts.pio = ruby_port.master
        #system.cpu[i].interrupts.int_master = ruby_port.slave
        #system.cpu[i].interrupts.int_slave = ruby_port.master

        # Connect the cpu's cache ports to Ruby
        system.cpu[i].icache_port = ruby_port.slave
        system.cpu[i].dcache_port = ruby_port.slave
        if buildEnv['TARGET_ISA'] == 'x86':
            system.cpu[i].itb.walker.port = ruby_port.slave
            system.cpu[i].dtb.walker.port = ruby_port.slave
else:
    system.system_port = system.membus.slave
    system.physmem.port = system.membus.master
    CacheConfig.config_cache(options, system)

root = Root(full_system = False, system = system)
#root.system.mem_mode = 'timing' FIXME: was in se_alpha_ruby_loop.py, do we need it?

m5.disableAllListeners()
m5.ticks.setGlobalFrequency(options.internal_sampling_freq)

dvfs = Dvfs(recreateFiles = True)
root.add_child('Dvfs',dvfs)

# instantiate configuration
m5.instantiate()

base_freq_tps = m5.ticks.getFrequency(options.base_freq)             #FIXME: what is this number?
sample_freq_tps = m5.ticks.getFrequency(options.sample_time)         #FIXME: what is this number?
simulation_tick_tps = m5.ticks.getFrequency(options.simulation_time) #FIXME: what is this number?
internal_sampling_freq_tps = m5.ticks.getFrequency(options.internal_sampling_freq) #This is the internal high resolution tick

print 'base_freq_tps: ',base_freq_tps,'\nsample_freq_tps: ',sample_freq_tps,'\nsimulation_tick_tps: ',simulation_tick_tps,'\ninternal_sampling_freq_tps: ',internal_sampling_freq_tps,'\ntime_: ',int(sample_freq_tps/simulation_tick_tps),'\ndump_period_s',options.dump_period,'\n'

flag=True
bench_count=0
count_loops=0
num_tick_per_loop=int(internal_sampling_freq_tps*options.dump_period)

print 'num_tick_per_loop: ',int(num_tick_per_loop),'\n'

s=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
conn=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
if options.listen>0:
    s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    s.bind(('',options.listen))
    s.listen(1)
    print '\nWaiting for manager on port ', options.listen
    conn,addr=s.accept()
    print 'Manager connected'

sockbuf=''
def recvline(sock):
	global sockbuf
	while True:
		if '\n' in sockbuf:
			lines=sockbuf.split('\n')
			sockbuf='\n'.join(lines[1:])
			return lines[0]
		else:
			new=sock.recv(4096)
			if new=='': # recv() returns '' if socket closed
				sockbuf=''
				return ''
			else: sockbuf+=new

while flag:
	if options.listen>0:
		while True:
			command=recvline(conn)
			if command == '':
				print 'Socket closed unexpectedly, reconnecting...'
				conn.close()
				conn,addr=s.accept()
				print 'Manager connected'
				continue
			pair=command.split()
			if command == 'exit':
				print 'Exiting.'
				conn.close()
				s.close()
				sys.exit(0)
			elif command == 'begin':
				pass
			elif command == 'end':
				break
			elif command == 'runThermalPolicy':
				dvfs.runThermalPolicy()
			elif pair[0].startswith('cpu'):
				dvfs.setCpuTemperature(int(pair[0][3:]),float(pair[1]))
			elif pair[0].startswith('router'):
				dvfs.setRouterTemperature(int(pair[0][6:]),float(pair[1]))
			else:
				print 'Unsupported command received from socket: "', command, '"'
				conn.close()
				s.close()
				sys.exit(1)
	
	# Run GEM5
	exit_event = m5.simulate(long(num_tick_per_loop))
	print '(Time_stage ',count_loops,') Exiting @', m5.curTick(), 'for #',num_tick_per_loop, '\n'
	# Exit if simulation limit reached, or if exit() has been invoked
	if exit_event.getCause() == "simulate() limit reached": 
		count_loops=count_loops+1
	
	if exit_event.getCause() == "user interrupt received":
		flag = False
	
	if exit_event.getCause() == "target called exit()":
		bench_count=bench_count+1		
		print 'Bench count: ',bench_count,' Total bench: ',options.num_cpus,'\n'
		if bench_count==options.num_cpus:
			flag = False
	
	m5.stats.dump()
	m5.stats.reset()
	
	if options.listen>0:
		#gather stats (for now only NoC power)
		data="begin\n"
		for i in xrange(dvfs.getNumRouters()):
			data+="noc.router:\n";
			data+="  Runtime Dynamic = "+str(dvfs.getDynamicRouterPower(i)+ \
				dvfs.getClockRouterPower(i))+" W\n"
			data+="  Gate Leakage = "+str(dvfs.getStaticRouterPower(i))+" W\n\n"
		for i in xrange(dvfs.getNumRouters()):
			data+="dyn.router"+str(i)+".frequency "+str(dvfs.getAverageRouterFrequency(i))+"\n";
			data+="dyn.router"+str(i)+".voltage "+str(dvfs.getAverageRouterVoltage(i))+"\n";
		for i in xrange(dvfs.getNumCpus()):
			data+="dyn.cpu"+str(i)+".frequency "+str(dvfs.getAverageCpuFrequency(i))+"\n";
			data+="dyn.cpu"+str(i)+".voltage "+str(dvfs.getAverageCpuVoltage(i))+"\n";
		data+="end\n"
		
		
		#inform the manager
		conn.sendall(data)

print 'Exiting @ tick', m5.curTick(), 'because', exit_event.getCause()
conn.close()
s.close()
