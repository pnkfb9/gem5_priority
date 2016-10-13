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

#include <iostream>
#include "Resynchronizer.hh"
#include "NetworkLink_d.hh"

using namespace std;

static Tick resyncDelayInPicoseconds=10; //Default value
static bool tooLate=false;

void setPropagationDelayInPicoseconds(Tick d)
{
    assert(tooLate==false);
    resyncDelayInPicoseconds=d;
}

//
// class Resynchronizer
//

Tick Resynchronizer::computePropagationDelay()
{
    tooLate=true;
    Tick result=resyncDelayInPicoseconds*SimClock::Frequency/1000000000000ull;
    if(result==0) fatal("simulation tick is too high to handle propagation delay");
    return result;
}

/*
 * TODO: update use cases!!!
 * 
* Use cases:
* target period < this period
*                     Now         Resynchronized here
*                      |                   |
*                       ____      ____      ____      ____      ____
* target period 4: ____|    |____|    |____|    |____|    |____|    |____
*                       _________           _________           _________
* this period 8:   ____|         |_________|         |_________|
* Delay=1
* 
*                               Now Resynchronized here
*                                |         |        
*                       ____      ____      ____      ____      ____
* target period 4: ____|    |____|    |____|    |____|    |____|    |____
*                       _________           _________           _________
* this period 8:    ___|         |_________|         |_________|
* Delay=0
* 
* target period > this period
*                     Now         Resynchronized here
*                      |                   |
*                       _________           _________           _________
* target period 8: ____|         |_________|         |_________|
*                       ____      ____      ____      ____      ____
* this period 4:   ____|    |____|    |____|    |____|    |____|    |____
* Delay=2
* 
*                     Now             Resynchronized here
*                      |                     |
*                       _________           _________           _________
* target period 8: ____|         |_________|         |_________|
*                         ____      ____      ____      ____      ____
* this period 4:     ____|    |____|    |____|    |____|    |____|    |__
* Delay=2
* 
*/

void
FifoResynchronizer::scheduleRequestPropagation()
{
	schedule(
			new FifoResynchEvent(&outstandingReq),
			curTick()+link->getLinkConsumer()->getClockedObject()->clockPeriod()+propagationDelay
		);
}

    
void 
FifoResynchronizer::sendAcknowledge()
{
	schedule(new FifoResynchEvent(&availFifoSlots),curTick()+link->getLinkSource()->getClockedObject()->clockPeriod()+propagationDelay);
}
