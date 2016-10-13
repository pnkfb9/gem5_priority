/*-------------------------------------------------------------------------
 *                             ORION 2.0 
 *
 *         					Copyright 2009 
 *  	Princeton University, and Regents of the University of California 
 *                         All Rights Reserved
 *
 *                         
 *  ORION 2.0 was developed by Bin Li at Princeton University and Kambiz Samadi at
 *  University of California, San Diego. ORION 2.0 was built on top of ORION 1.0. 
 *  ORION 1.0 was developed by Hangsheng Wang, Xinping Zhu and Xuning Chen at 
 *  Princeton University.
 *
 *  If your use of this software contributes to a published paper, we
 *  request that you cite our paper that appears on our website 
 *  http://www.princeton.edu/~peh/orion.html
 *
 *  Permission to use, copy, and modify this software and its documentation is
 *  granted only under the following terms and conditions.  Both the
 *  above copyright notice and this permission notice must appear in all copies
 *  of the software, derivative works or modified versions, and any portions
 *  thereof, and both notices must appear in supporting documentation.
 *
 *  This software may be distributed (but not offered for sale or transferred
 *  for compensation) to third parties, provided such third parties agree to
 *  abide by the terms and conditions of this notice.
 *
 *  This software is distributed in the hope that it will be useful to the
 *  community, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 *
 *-----------------------------------------------------------------------*/

#include <stdio.h>
#include <math.h>


#include "mem/ruby/network/orion/OrionRouter.hh"

double OrionRouter::router_area_in_buf()
{
	double bitline_len=0, wordline_len=0;
	double area_in_buf=0;
	/* buffer area */
	/* input buffer area */
	if (!this->m_params_map["IN_BUF_MODEL"].compare("NO_SET")/*info->in_buf*/) 
	{
		if(this->m_params_map["IN_BUF_MODEL"].compare("SRAM"))
		{
			bitline_len =  m_in_buf_num_set * (RegCellHeight + 2 * WordlineSpacing);
			wordline_len = m_flit_width * (RegCellWidth + 2 *( 1/*info->in_buf_info.read_ports */
							+ 1 /*info->in_buf_info.write_ports*/) * BitlineSpacing);

			/* input buffer area */
			area_in_buf = m_num_in_port * m_num_vclass * (bitline_len * wordline_len) *
										(m_params_map["IN_BUF_MODEL"].compare("IS_IN_SHARED_BUFFER") ? 1 : m_num_vchannel );
		}
		else if(m_params_map["IN_BUF_MODEL"].compare("REGISTER"))
		{
			area_in_buf = AreaDFF * info->n_in * info->n_v_class * m_flit_width *
					m_in_buf_num_set * (m_params_map["IN_BUF_MODEL"].compare("IS_IN_SHARED_BUFFER") ? 1 : m_num_vchannel );
		}
		else
		{
			panic("IN_BUF_MODEL undefined\n");
		}

	}
	return area_in_buf;
}

























































































































































































































































