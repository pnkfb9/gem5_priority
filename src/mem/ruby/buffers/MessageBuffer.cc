/*
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
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

#include <cassert>

#include "base/cprintf.hh"
#include "base/misc.hh"
#include "base/stl_helpers.hh"
#include "debug/RubyQueue.hh"
#include "mem/ruby/buffers/MessageBuffer.hh"
#include "mem/ruby/system/System.hh"
#include "mem/ruby/network/garnet/fixed-pipeline/NetworkInterface_d.hh"
#include "mem/ruby/slicc_interface/NetworkMessage.hh"
#include "mem/ruby/slicc_interface/RubyRequest.hh"

using namespace std;
using m5::stl_helpers::operator<<;

MessageBuffer::MessageBuffer(const string &name)
{
    cpuForDFS=0;
    
    m_msg_counter = 0;
    m_consumer_ptr = NULL;
    m_clockobj_ptr = NULL;

    m_ordering_set = false;
    m_strict_fifo = true;
    m_size = 0;
    m_max_size = -1;
    m_last_arrival_time = 0;
    m_randomization = true;
    m_size_last_time_size_checked = 0;
    m_time_last_time_size_checked = 0;
    m_time_last_time_enqueue = 0;
    m_time_last_time_pop = 0;
    m_size_at_cycle_start = 0;
    m_msgs_this_cycle = 0;
    m_not_avail_count = 0;
    m_priority_rank = 0;
    m_name = name;

    m_stall_msg_map.clear();
    m_input_link_id = 0;
    m_vnet_id = 0;
}

int
MessageBuffer::getSize()
{
    if (m_time_last_time_size_checked == m_clockobj_ptr->curCycle()) {
        return m_size_last_time_size_checked;
    } else {
        m_time_last_time_size_checked = m_clockobj_ptr->curCycle();
        m_size_last_time_size_checked = m_size;
        return m_size;
    }
}

bool
MessageBuffer::areNSlotsAvailable(int n)
{

    // fast path when message buffers have infinite size
    if (m_max_size == -1) {
        return true;
    }

    // determine my correct size for the current cycle
    // pop operations shouldn't effect the network's visible size
    // until next cycle, but enqueue operations effect the visible
    // size immediately
    int current_size = max(m_size_at_cycle_start, m_size);
    if (m_time_last_time_pop < m_clockobj_ptr->curCycle()) {
        // no pops this cycle - m_size is correct
        current_size = m_size;
    } else {
        if (m_time_last_time_enqueue < m_clockobj_ptr->curCycle()) {
            // no enqueues this cycle - m_size_at_cycle_start is correct
            current_size = m_size_at_cycle_start;
        } else {
            // both pops and enqueues occured this cycle - add new
            // enqueued msgs to m_size_at_cycle_start
            current_size = m_size_at_cycle_start+m_msgs_this_cycle;
        }
    }

    // now compare the new size with our max size
    if (current_size + n <= m_max_size) {
        return true;
    } else {
        DPRINTF(RubyQueue, "n: %d, current_size: %d, m_size: %d, "
                "m_max_size: %d\n",
                n, current_size, m_size, m_max_size);
        m_not_avail_count++;
        return false;
    }
}

const MsgPtr
MessageBuffer::getMsgPtrCopy() const
{
    assert(isReady());

    return m_prio_heap.front().m_msgptr->clone();
}

const Message*
MessageBuffer::peekAtHeadOfQueue() const
{
    DPRINTF(RubyQueue, "Peeking at head of queue.\n");
    assert(isReady());

    const Message* msg_ptr = m_prio_heap.front().m_msgptr.get();
    assert(msg_ptr);

    DPRINTF(RubyQueue, "Message: %s\n", (*msg_ptr));
    return msg_ptr;
}

// FIXME - move me somewhere else
int
random_time()
{
    int time = 1;
    time += random() & 0x3;  // [0...3]
    if ((random() & 0x7) == 0) {  // 1 in 8 chance
        time += 100 + (random() % 0xf); // 100 + [1...15]
    }
    return time;
}

void
MessageBuffer::enqueue(MsgPtr message, Time delta)
{
	
	//start enqueue time count for congestion stats
	message->setT_StartCycle(m_clockobj_ptr->curCycle());

	//increment congestion update in flit
//TODO
	assert(m_consumer_ptr!=NULL);
	NetworkInterface_d *net_ni_ptr=dynamic_cast<NetworkInterface_d*>(m_consumer_ptr);
	if(net_ni_ptr!=NULL)
	{
        NetworkMessage *net_msg_ptr = dynamic_cast<NetworkMessage *>(message.get());
        assert(net_msg_ptr);

		//panic("è panico qui");
		GarnetNetwork_d* net_ptr=net_ni_ptr->getNetPtr();
		int num_flits = (int) ceil((double) net_ptr->MessageSizeType_to_int(
    			    net_msg_ptr->getMessageSize())/net_ptr->getNiFlitSize());
        
		net_ni_ptr->incrementOutPkt(num_flits);
		/*if(net_ni_ptr->getObjectId()=="P18NetworkInterface_d5")
		{
			std::cout<<"###### increasing packets at: "<<net_ni_ptr->getObjectId()<<" "
			<<net_ni_ptr->getOutPkt()<<" ## "<< num_flits<<std::endl;
		}*/
	}

//END TODO
    m_msg_counter++;
    m_size++;

    // record current time incase we have a pop that also adjusts my size
    if (m_time_last_time_enqueue < m_clockobj_ptr->curCycle()) {
        m_msgs_this_cycle = 0;  // first msg this cycle
        m_time_last_time_enqueue = m_clockobj_ptr->curCycle();
    }
    m_msgs_this_cycle++;

    if (!m_ordering_set) {
        panic("Ordering property of %s has not been set", m_name);
    }

    // Calculate the arrival time of the message, that is, the first
    // cycle the message can be dequeued.
    assert(delta>0);
    Time current_time = m_clockobj_ptr->curCycle();
    Time arrival_time = 0;
    if (!RubySystem::getRandomization() || (m_randomization == false)) {
        // No randomization
        arrival_time = current_time + delta;
    } else {
        // Randomization - ignore delta
        if (m_strict_fifo) {
            if (m_last_arrival_time < current_time) {
                m_last_arrival_time = current_time;
            }
            arrival_time = m_last_arrival_time + random_time();
        } else {
            arrival_time = current_time + random_time();
        }
    }

    // Check the arrival time
    assert(arrival_time > current_time);
    if (m_strict_fifo) {
        if (arrival_time < m_last_arrival_time) {
            panic("FIFO ordering violated: %s name: %s current time: %d "
                  "delta: %d arrival_time: %d last arrival_time: %d\n",
                  *this, m_name, current_time * m_clockobj_ptr->clockPeriod(),
                  delta * m_clockobj_ptr->clockPeriod(),
                  arrival_time * m_clockobj_ptr->clockPeriod(),
                  m_last_arrival_time * m_clockobj_ptr->clockPeriod());
        }
    }

    // If running a cache trace, don't worry about the last arrival checks
    if (!g_system_ptr->m_warmup_enabled) {
        m_last_arrival_time = arrival_time;
    }

    // compute the delay cycles and set enqueue time
    Message* msg_ptr = message.get();
    assert(msg_ptr != NULL);

    assert(curTick() >= msg_ptr->getLastEnqueueTime() &&
           "ensure we aren't dequeued early");

    msg_ptr->setDelayedCycles(curTick() -
                              msg_ptr->getLastEnqueueTime() +
                              msg_ptr->getDelayedCycles());
    msg_ptr->setLastEnqueueTime(curTick());

    // Insert the message into the priority heap
    MessageBufferNode thisNode(arrival_time, m_msg_counter, message);
    m_prio_heap.push_back(thisNode);
    push_heap(m_prio_heap.begin(), m_prio_heap.end(),
        greater<MessageBufferNode>());

    DPRINTF(RubyQueue, "Enqueue arrival_time: %lld, Message: %s\n",
            arrival_time * m_clockobj_ptr->clockPeriod(), *(message.get()));

    // Schedule the wakeup
    if (m_consumer_ptr != NULL) {
        m_consumer_ptr->scheduleEventAbsolute(arrival_time);
        m_consumer_ptr->storeEventInfo(m_vnet_id);
    } else {
        panic("No consumer: %s name: %s\n", *this, m_name);
    }
}

Time
MessageBuffer::dequeue_getDelayCycles(MsgPtr& message)
{
    dequeue(message);
    return setAndReturnDelayCycles(message);
}

void
MessageBuffer::dequeue(MsgPtr& message)
{
    DPRINTF(RubyQueue, "Dequeueing\n");
    message = m_prio_heap.front().m_msgptr;

    pop();
    DPRINTF(RubyQueue, "Enqueue message is %s\n", (*(message.get())));
}

Time
MessageBuffer::dequeue_getDelayCycles()
{
    // get MsgPtr of the message about to be dequeued
    MsgPtr message = m_prio_heap.front().m_msgptr;

    // get the delay cycles
    Time delayCycles = setAndReturnDelayCycles(message);
    dequeue();

    return delayCycles;
}

void
MessageBuffer::pop()
{
    DPRINTF(RubyQueue, "Popping\n");
    assert(isReady());
    pop_heap(m_prio_heap.begin(), m_prio_heap.end(),
        greater<MessageBufferNode>());
    m_prio_heap.pop_back();

    // record previous size and time so the current buffer size isn't
    // adjusted until next cycle
    if (m_time_last_time_pop < m_clockobj_ptr->curCycle()) {
        m_size_at_cycle_start = m_size;
        m_time_last_time_pop = m_clockobj_ptr->curCycle();
    }
    m_size--;
}

void
MessageBuffer::clear()
{
    m_prio_heap.clear();

    m_msg_counter = 0;
    m_size = 0;
    m_time_last_time_enqueue = 0;
    m_time_last_time_pop = 0;
    m_size_at_cycle_start = 0;
    m_msgs_this_cycle = 0;
}

void
MessageBuffer::recycle()
{
    DPRINTF(RubyQueue, "Recycling.\n");
    assert(isReady());
    MessageBufferNode node = m_prio_heap.front();
    pop_heap(m_prio_heap.begin(), m_prio_heap.end(),
        greater<MessageBufferNode>());
    node.m_time = m_clockobj_ptr->curCycle() + m_recycle_latency;
    m_prio_heap.back() = node;
    push_heap(m_prio_heap.begin(), m_prio_heap.end(),
        greater<MessageBufferNode>());
    m_consumer_ptr->scheduleEventAbsolute(m_clockobj_ptr->curCycle() +
                                          m_recycle_latency);
}

void
MessageBuffer::reanalyzeMessages(const Address& addr)
{
    DPRINTF(RubyQueue, "ReanalyzeMessages\n");
    assert(m_stall_msg_map.count(addr) > 0);

    //
    // Put all stalled messages associated with this address back on the
    // prio heap
    //
    while(!m_stall_msg_map[addr].empty()) {
        m_msg_counter++;
        MessageBufferNode msgNode(m_clockobj_ptr->curCycle() + 1,
                                  m_msg_counter, 
                                  m_stall_msg_map[addr].front());

        m_prio_heap.push_back(msgNode);
        push_heap(m_prio_heap.begin(), m_prio_heap.end(),
                  greater<MessageBufferNode>());

        m_consumer_ptr->scheduleEventAbsolute(msgNode.m_time);
        m_stall_msg_map[addr].pop_front();
    }
    m_stall_msg_map.erase(addr);
}

void
MessageBuffer::reanalyzeAllMessages()
{
    DPRINTF(RubyQueue, "ReanalyzeAllMessages %s\n");

    //
    // Put all stalled messages associated with this address back on the
    // prio heap
    //
    for (StallMsgMapType::iterator map_iter = m_stall_msg_map.begin();
         map_iter != m_stall_msg_map.end();
         ++map_iter) {

        while(!(map_iter->second).empty()) {
            m_msg_counter++;
            MessageBufferNode msgNode(m_clockobj_ptr->curCycle() + 1,
                                      m_msg_counter, 
                                      (map_iter->second).front());

            m_prio_heap.push_back(msgNode);
            push_heap(m_prio_heap.begin(), m_prio_heap.end(),
                      greater<MessageBufferNode>());
            
            m_consumer_ptr->scheduleEventAbsolute(msgNode.m_time);
            (map_iter->second).pop_front();
        }
    }
    m_stall_msg_map.clear();
}

void
MessageBuffer::stallMessage(const Address& addr)
{
    DPRINTF(RubyQueue, "Stalling due to %s\n", addr);
    assert(isReady());
    assert(addr.getOffset() == 0);
    MsgPtr message = m_prio_heap.front().m_msgptr;

    pop();

    //
    // Note: no event is scheduled to analyze the map at a later time.
    // Instead the controller is responsible to call reanalyzeMessages when
    // these addresses change state.
    //
    (m_stall_msg_map[addr]).push_back(message);
}

Time
MessageBuffer::setAndReturnDelayCycles(MsgPtr msg_ptr)
{
    // get the delay cycles of the message at the top of the queue

    // this function should only be called on dequeue
    // ensure the msg hasn't been enqueued
    assert(msg_ptr->getLastEnqueueTime() <= curTick());
    msg_ptr->setDelayedCycles(curTick() -
                             msg_ptr->getLastEnqueueTime() +
                             msg_ptr->getDelayedCycles());
    msg_ptr->setLastEnqueueTime(curTick());
    return msg_ptr->getDelayedCycles();
}

void
MessageBuffer::print(ostream& out) const
{
    ccprintf(out, "[MessageBuffer: ");
    if (m_consumer_ptr != NULL) {
        ccprintf(out, " consumer-yes ");
    }

    vector<MessageBufferNode> copy(m_prio_heap);
    sort_heap(copy.begin(), copy.end(), greater<MessageBufferNode>());
    ccprintf(out, "%s] %s", copy, m_name);
}

void
MessageBuffer::printStats(ostream& out)
{
    out << "MessageBuffer: " << m_name << " stats - msgs:" << m_msg_counter
        << " full:" << m_not_avail_count << endl;
}

bool
MessageBuffer::isReady() const
{
    return ((m_prio_heap.size() > 0) &&
            (m_prio_heap.front().m_time <= m_clockobj_ptr->curCycle()));
}

bool
MessageBuffer::functionalRead(Packet *pkt)
{
    // Check the priority heap and read any messages that may
    // correspond to the address in the packet.
    for (unsigned int i = 0; i < m_prio_heap.size(); ++i) {
        Message *msg = m_prio_heap[i].m_msgptr.get();
        if (msg->functionalRead(pkt)) return true;
    }

    // Read the messages in the stall queue that correspond
    // to the address in the packet.
    for (StallMsgMapType::iterator map_iter = m_stall_msg_map.begin();
         map_iter != m_stall_msg_map.end();
         ++map_iter) {

        for (std::list<MsgPtr>::iterator it = (map_iter->second).begin();
            it != (map_iter->second).end(); ++it) {

            Message *msg = (*it).get();
            if (msg->functionalRead(pkt)) return true;
        }
    }
    return false;
}

uint32_t
MessageBuffer::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;

    // Check the priority heap and write any messages that may
    // correspond to the address in the packet.
    for (unsigned int i = 0; i < m_prio_heap.size(); ++i) {
        Message *msg = m_prio_heap[i].m_msgptr.get();
        if (msg->functionalWrite(pkt)) {
            num_functional_writes++;
        }
    }

    // Check the stall queue and write any messages that may
    // correspond to the address in the packet.
    for (StallMsgMapType::iterator map_iter = m_stall_msg_map.begin();
         map_iter != m_stall_msg_map.end();
         ++map_iter) {

        for (std::list<MsgPtr>::iterator it = (map_iter->second).begin();
            it != (map_iter->second).end(); ++it) {

            Message *msg = (*it).get();
            if (msg->functionalWrite(pkt)) {
                num_functional_writes++;
            }
        }
    }

    return num_functional_writes;
}
