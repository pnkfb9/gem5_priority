/*
 * Copyright (c) 2012 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
 * Copyright (c) 2009 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/misc.hh"
#include "cpu/testers/rubytest/Check.hh"
#include "cpu/testers/rubytest/RubyTester.hh"
#include "debug/RubyTest.hh"
#include "mem/ruby/common/Global.hh"
#include "mem/ruby/common/SubBlock.hh"
#include "mem/ruby/system/System.hh"
#include "sim/sim_exit.hh"
#include "sim/system.hh"

RubyTester::RubyTester(const Params *p)
  : MemObject(p), checkStartEvent(this),
    _masterId(p->system->getMasterId(name())),
    m_num_cpus(p->num_cpus),
    m_checks_to_complete(p->checks_to_complete),
    m_deadlock_threshold(p->deadlock_threshold),
    m_wakeup_frequency(p->wakeup_frequency),
    m_check_flush(p->check_flush),
    m_num_inst_ports(p->port_cpuInstPort_connection_count)
{
    m_checks_completed = 0;

    //
    // Create the requested inst and data ports and place them on the
    // appropriate read and write port lists.  The reason for the subtle
    // difference between inst and data ports vs. read and write ports is
    // from the tester's perspective, it only needs to know whether a port
    // supports reads (checks) or writes (actions).  Meanwhile, the protocol
    // controllers have data ports (support read and writes) or inst ports
    // (support only reads).
    // Note: the inst ports are the lowest elements of the readPort vector,
    // then the data ports are added to the readPort vector
    //
    for (int i = 0; i < p->port_cpuInstPort_connection_count; ++i) {
        readPorts.push_back(new CpuPort(csprintf("%s-instPort%d", name(), i),
                                        this, i));
    }
    for (int i = 0; i < p->port_cpuDataPort_connection_count; ++i) {
        CpuPort *port = new CpuPort(csprintf("%s-dataPort%d", name(), i),
                                    this, i);
        readPorts.push_back(port);
        writePorts.push_back(port);
    }

    // add the check start event to the event queue
    schedule(checkStartEvent, 1);
}

RubyTester::~RubyTester()
{
    delete m_checkTable_ptr;
    // Only delete the readPorts since the writePorts are just a subset
    for (int i = 0; i < readPorts.size(); i++)
        delete readPorts[i];
}

void
RubyTester::init()
{
    assert(writePorts.size() > 0 && readPorts.size() > 0);

    m_last_progress_vector.resize(m_num_cpus);
    for (int i = 0; i < m_last_progress_vector.size(); i++) {
        m_last_progress_vector[i] = 0;
    }

    m_num_writers = writePorts.size();
    m_num_readers = readPorts.size();

    m_checkTable_ptr = new CheckTable(m_num_writers, m_num_readers, this);
}

BaseMasterPort &
RubyTester::getMasterPort(const std::string &if_name, PortID idx)
{
    if (if_name != "cpuInstPort" && if_name != "cpuDataPort") {
        // pass it along to our super class
        return MemObject::getMasterPort(if_name, idx);
    } else {
        if (if_name == "cpuInstPort") {
            if (idx > m_num_inst_ports) {
                panic("RubyTester::getMasterPort: unknown inst port idx %d\n",
                      idx);
            }
            //
            // inst ports directly map to the lowest readPort elements
            //
            return *readPorts[idx];
        } else {
            assert(if_name == "cpuDataPort");
            //
            // add the inst port offset to translate to the correct read port
            // index
            //
            int read_idx = idx + m_num_inst_ports;
            if (read_idx >= static_cast<PortID>(readPorts.size())) {
                panic("RubyTester::getMasterPort: unknown data port idx %d\n",
                      idx);
            }
            return *readPorts[read_idx];
        }
    }
}

bool
RubyTester::CpuPort::recvTimingResp(PacketPtr pkt)
{
    // retrieve the subblock and call hitCallback
    RubyTester::SenderState* senderState =
        safe_cast<RubyTester::SenderState*>(pkt->senderState);
    SubBlock* subblock = senderState->subBlock;
    assert(subblock != NULL);

    // pop the sender state from the packet
    pkt->senderState = senderState->saved;

    tester->hitCallback(id, subblock);

    // Now that the tester has completed, delete the senderState
    // (includes sublock) and the packet, then return
    delete senderState;
    delete pkt->req;
    delete pkt;
    return true;
}

bool
RubyTester::isInstReadableCpuPort(int idx)
{
    return idx < m_num_inst_ports;
}

MasterPort*
RubyTester::getReadableCpuPort(int idx)
{
    assert(idx >= 0 && idx < readPorts.size());

    return readPorts[idx];
}

MasterPort*
RubyTester::getWritableCpuPort(int idx)
{
    assert(idx >= 0 && idx < writePorts.size());

    return writePorts[idx];
}

void
RubyTester::hitCallback(NodeID proc, SubBlock* data)
{
    // Mark that we made progress
    m_last_progress_vector[proc] = curCycle();

    DPRINTF(RubyTest, "completed request for proc: %d\n", proc);
    DPRINTF(RubyTest, "addr: 0x%x, size: %d, data: ",
            data->getAddress(), data->getSize());
    for (int byte = 0; byte < data->getSize(); byte++) {
        DPRINTF(RubyTest, "%d", data->getByte(byte));
    }
    DPRINTF(RubyTest, "\n");

    // This tells us our store has 'completed' or for a load gives us
    // back the data to make the check
    Check* check_ptr = m_checkTable_ptr->getCheck(data->getAddress());
    assert(check_ptr != NULL);
    check_ptr->performCallback(proc, data, curCycle());
}

void
RubyTester::wakeup()
{
    if (m_checks_completed < m_checks_to_complete) {
        // Try to perform an action or check
        Check* check_ptr = m_checkTable_ptr->getRandomCheck();
        assert(check_ptr != NULL);
        check_ptr->initiate();

        checkForDeadlock();

        schedule(checkStartEvent, curTick() + m_wakeup_frequency);
    } else {
        exitSimLoop("Ruby Tester completed");
    }
}

void
RubyTester::checkForDeadlock()
{
    int size = m_last_progress_vector.size();
    Time current_time = curCycle();
    for (int processor = 0; processor < size; processor++) {
        if ((current_time - m_last_progress_vector[processor]) >
                m_deadlock_threshold) {
            panic("Deadlock detected: current_time: %d last_progress_time: %d "
                  "difference:  %d processor: %d\n",
                  current_time, m_last_progress_vector[processor],
                  current_time - m_last_progress_vector[processor], processor);
        }
    }
}

void
RubyTester::print(std::ostream& out) const
{
    out << "[RubyTester]" << std::endl;
}

RubyTester *
RubyTesterParams::create()
{
    return new RubyTester(this);
}
