/*
 * Copyright (c) 2013 Politecnico di Milano
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
 * Authors: Federico Terraneo, Davide Zoni
 */

#ifndef RESYNCHRONIZER_H
#define	RESYNCHRONIZER_H
#include <iostream>
#include "sim/eventq.hh"
#include "sim/core.hh"


class NetworkLink_d;
/**
 * Allows to set the propagation delay, overriding the default value.
 * Must be called during startup, before the Resynchronizer classes are
 * instatiated, otherwise the value will be ignored.
 */
void setPropagationDelayInPicoseconds(Tick d);


/**
 * Base class for the various resynchronizers
 */
class Resynchronizer : public EventManager
{
public:
    Resynchronizer(EventQueue *eq) : EventManager(eq),
            propagationDelay(computePropagationDelay()) {}
    
    virtual bool isRequestPending() const=0;
    virtual bool isRequestAvailable() const=0;
    virtual bool isAcknowkedgePending() const=0;
    virtual bool isAcknowledgeAvailable() const=0;
    virtual void sendRequest()=0;
    virtual void scheduleRequestPropagation()=0;
    virtual void sendAcknowledge()=0;
    virtual void clearRequest()=0;
    virtual void clearAcknowledge()=0;
    virtual void advanceTransmitSide()=0;
    virtual void advanceReceiveSide()=0;
    virtual int getAvailSlots(){return 0;}
    
    virtual ~Resynchronizer() {}
    
protected:
    
    class PropagationEndEvent : public Event
    {
    public:
        PropagationEndEvent(bool *flag)
            : Event(Event::NOC_NetworkLinkFree_Pri, AutoDelete), flag(flag) {}

        virtual void process()
        {
            assert(*flag==true);
            *flag=false;
        }
    private:
        bool *flag;
    };
    
    const Tick propagationDelay;
    
private:
    Resynchronizer(const Resynchronizer&)=delete;
    Resynchronizer& operator=(const Resynchronizer&)=delete;
    
    Tick computePropagationDelay();  
};

/**
 * Dummy resynchronizer. Only works if the NoC is kept at the same frequency
 * and fully synchronous. Can be used to compare the performance of Asynchronous
 * NoCs with the baseline case of a Synchronous NoC.
 * 
 * If you use this resynchronizer but then change the frequency of the NoC the
 * simulation *will* fail, and you probably deserve it.
 */
class DummyResynchronizer : public Resynchronizer
{
public:
    DummyResynchronizer(EventQueue *eq) : Resynchronizer(eq) {}
    
    bool isRequestPending() const { return false; }
    bool isRequestAvailable() const { return true; }
    bool isAcknowkedgePending() const { return false; }
    bool isAcknowledgeAvailable() const { return true; }
    void sendRequest() {}
    void scheduleRequestPropagation() {}
    void sendAcknowledge() {}
    void clearRequest() {}
    void clearAcknowledge() {}
    void advanceTransmitSide() {}
    void advanceReceiveSide() {}
};

/**
 * Resynchronizer simulating an optimized 2-way (aka double data rate) handshake
 * resynchronization scheme. Bot the request and the acknowledge pins are
 * synchronized with two cascaded flip-flops (not one) to avoid metastability.
 */
class HandshakeResynchronizer : public Resynchronizer
{
public:
    HandshakeResynchronizer(EventQueue *eq) : Resynchronizer(eq),
            request(false), reqPropagation(false), reqResyncDelay(0),
            acknowledge(false), ackPropagation(false), ackResyncDelay(0) {}
    
    static const int resyncDelay=1;
    
    bool isRequestPending() const { return request; }
        
    bool isRequestAvailable() const
    {
        return request && reqPropagation==false && reqResyncDelay==0;
    }
    
    bool isAcknowkedgePending() const
    {
        //A request once accepted will turn into an acknowledge,
        //so an ack is pending as soon as the request is
        return request || acknowledge;
    }
    
    bool isAcknowledgeAvailable() const
    {
        return acknowledge && ackPropagation==false && ackResyncDelay==0;
    }
    
    void sendRequest()
    {
        assert(request==false);
        request=true;
        assert(reqResyncDelay==0);
        reqResyncDelay=resyncDelay;
        assert(reqPropagation==false);
        reqPropagation=true;
    }
    
    void scheduleRequestPropagation()
    {
        schedule(new PropagationEndEvent(&reqPropagation),curTick()+propagationDelay);
    }
    
    void sendAcknowledge()
    {
        assert(acknowledge==false);
        acknowledge=true;
        assert(ackResyncDelay==0);
        ackResyncDelay=resyncDelay;
        
        assert(ackPropagation==false);
        ackPropagation=true;
        schedule(new PropagationEndEvent(&ackPropagation),curTick()+propagationDelay);
    }
    
    void clearRequest()
    {
        assert(isRequestAvailable());
        request=false;
    }
    
    void clearAcknowledge()
    {
        assert(isAcknowledgeAvailable());
        acknowledge=false;
    }
    
    void advanceTransmitSide()
    {
        if(acknowledge==false || ackPropagation==true) return;
        if(ackResyncDelay>0) ackResyncDelay--;
    }
    
    void advanceReceiveSide()
    {
        if(request==false || reqPropagation==true) return;
        if(reqResyncDelay>0) reqResyncDelay--;
    }
    
private:
    
    bool request;
    bool reqPropagation;
    int reqResyncDelay;
    bool acknowledge;
    bool ackPropagation;
    int ackResyncDelay;
};

/* 
 * FIFO resynchronizer - Proposed in bi-synchronous fifo for synchronous circuit
 * communication well suited for NoCs in GALS architectures" NOCS2007. 
 */
class FifoResynchronizer : public Resynchronizer
{
public:
    FifoResynchronizer(EventQueue *eq, int fifoSlots, NetworkLink_d* link) : Resynchronizer(eq),
			sendReqTick(0), recvReqTick(0),
			availFifoSlots(fifoSlots), fifoSlots(fifoSlots),
			reqPropagation(false), ackPropagation(false),
			outstandingReq(0), link(link) {}
    inline virtual int getAvailSlots(){return availFifoSlots;} 
    bool isRequestPending() const 
	{//a req is considered pending only if a single slot is used and prog delay is not exausted 
		return (outstandingReq==0)||(recvReqTick==curTick());
	}
        
    bool isRequestAvailable() const
    {//if there are waiting flits but no flit has taken this cycle!!
		return ((outstandingReq>0)&&(recvReqTick<curTick()));
    }
    
    bool isAcknowkedgePending() const
    {	
		assert(availFifoSlots>=0);
		return (availFifoSlots==0)||(sendReqTick==curTick());
    }
    
    bool isAcknowledgeAvailable() const
    {
		return ((availFifoSlots>0)&& (sendReqTick<curTick()));
    }
    
    void sendRequest()
    {// decrease 1 avail slot in the fifo queue
		assert(availFifoSlots>0);
		sendReqTick=curTick();
		availFifoSlots--;
    }
    void scheduleRequestPropagation();
    void sendAcknowledge();
    
    void clearRequest() 
	{
		assert(outstandingReq>0);
		recvReqTick=curTick();
		outstandingReq--;
	}
    
    void clearAcknowledge() {}
    
    void advanceTransmitSide() {} 
    
    void advanceReceiveSide() {}
    
private:

	Tick sendReqTick;
	Tick recvReqTick;

	int availFifoSlots;
	int fifoSlots;

    bool reqPropagation;
    bool ackPropagation;

	int outstandingReq;

	NetworkLink_d* link;

   // this event is used to update the avil slots and pending requests to the
   // corresponding producer consumer, respectively
    class FifoResynchEvent : public Event
    {
    public:
        FifoResynchEvent(int *var)
            : Event(Event::NOC_NetworkLinkFree_Pri, AutoDelete), var(var) {}

        virtual void process()
        {
            assert(*var>=0);
            (*var)++;
			//std::cout<<"outstandingReq "<< *var<<std::endl;
        }
    private:
        int *var;
    };
};

#endif //RESYNCHRONIZER_H
