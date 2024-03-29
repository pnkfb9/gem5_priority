/*
 * Copyright (c) 2008 Princeton University
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
 *
 * Authors: Niket Agarwal
 */

#include "mem/ruby/network/garnet/fixed-pipeline/flitBuffer_d.hh"

flitBuffer_d::flitBuffer_d()
{
    max_size = INFINITE_;
}

flitBuffer_d::flitBuffer_d(int maximum_size)
{
    max_size = maximum_size;
}

bool
flitBuffer_d::isEmpty()
{
    return (m_buffer.size() == 0);
}

bool
flitBuffer_d::isReady(Time curTime)
{
    if (m_buffer.size() != 0 ) {
        flit_d *t_flit = peekTopFlit();
        if (t_flit->get_time() <= curTime)
            return true;
    }
    return false;
}

bool
flitBuffer_d::isReadyForNext(Time curTime)
{
    if (m_buffer.size() != 0 ) {
        flit_d *t_flit = peekTopFlit();
        if (t_flit->get_time() <= (curTime + 1))
            return true;
    }
    return false;
}

void
flitBuffer_d::print(std::ostream& out) const
{
    out << "[flitBuffer: " << m_buffer.size() << "] " << std::endl;
}

bool
flitBuffer_d::isFull()
{
    return (m_buffer.size() >= max_size);
}

void
flitBuffer_d::setMaxSize(int maximum)
{
    max_size = maximum;
}
