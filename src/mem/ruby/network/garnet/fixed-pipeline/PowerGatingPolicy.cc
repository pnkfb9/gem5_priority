/*
 * Copyright (c) 2014 Politecnico di Milano
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
 * Authors: Davide Zoni
 */

#include "PowerGatingPolicy.hh"
#include "config/actual_pg_policy.hh"
#include "config/use_apnea_base.hh"
#include "config/use_vnet_reuse.hh"

#include <string>

/* 
 * class PowerGatingPolicy
*/

void PowerGatingPolicy::addRouter(Router_d *router, EventQueue *eventq)
{
	std::string actual_pg_policy = ACTUAL_PG_POLICY;
	// this is just an interface to include other actual policy classes
	//static objects only
	if(actual_pg_policy=="ALL_BUFFER_SENSING_LOAD_NO_PG")
	{
		static AllBufferSensingNoPwrGating policy(eventq);
		policy.addRouterImpl(router);
	}

	else if(actual_pg_policy=="FIRST_CROSSBAR_POWER_GATING")
	{
		static FirstPowerGatingPolicy policy(eventq);
		policy.addRouterImpl(router);
	}
	else if(actual_pg_policy=="APNEA_BASE")
	{// policy for POLIMI-UCY collaboration
		static bool is_apnea_base_flag_set=false;
		static bool is_vnet_reuse_flag_set=false;
		#if USE_APNEA_BASE==1
			is_apnea_base_flag_set=true;
		#endif
		#if USE_VNET_REUSE==1
			is_vnet_reuse_flag_set=true;
		#endif
		assert(is_apnea_base_flag_set && "\n\nCompile using also USE_APNEA_BASE==1\n\n");
		assert(is_vnet_reuse_flag_set && "\n\nCompile using also USE_APNEA_BASE==1\n\n");
		static ApneaPGPolicy policy(eventq);
		policy.addRouterImpl(router);
	}
	else
		panic("\n--- Power Gating policy not defined ---\n\n");
}

PowerGatingPolicy::PowerGatingPolicy(EventQueue *eventq): eventq(eventq) {}

void PowerGatingPolicy::addRouterImpl(Router_d* router){}

PowerGatingPolicy::~PowerGatingPolicy(){}
