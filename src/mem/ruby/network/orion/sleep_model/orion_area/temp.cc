
double OrionConfig::router_area_in_buf()
{
	double bitline_len, wordline_len, xb_in_len, xb_out_len;
	double depth, nMUX, boxArea;
	int req_width;
	router_area->buffer = 0;
	router_area->crossbar = 0;
	router_area->vc_allocator = 0;
	router_area->sw_allocator = 0;

	/* virtual channel allocator area */
	if(info->vc_allocator_type == ONE_STAGE_ARB && info->n_v_channel > 1 && info->n_in > 1){
		if (info->vc_out_arb_model){
			req_width = (info->n_in - 1) * info->n_v_channel;
			switch (info->vc_out_arb_model){
				case MATRIX_ARBITER: //assumes 30% spacing for each arbiter
					router_area->vc_allocator = ((AreaNOR * 2 * (req_width - 1) * req_width) + (AreaINV * req_width)
							+ (AreaDFF * (req_width * (req_width - 1)/2))) * 1.3 * info->n_out * info->n_v_channel * info->n_v_class;
					break;

				case RR_ARBITER: //assumes 30% spacing for each arbiter
					router_area->vc_allocator = ((6 *req_width * AreaNOR) + (2 * req_width * AreaINV) + (req_width * AreaDFF)) * 1.3  
												* info->n_out * info->n_v_channel * info->n_v_class;
					break;

				case QUEUE_ARBITER:
					router_area->vc_allocator = AreaDFF * info->vc_out_arb_queue_info.data_width 
						* info->vc_out_arb_queue_info.n_set * info->n_out * info->n_v_channel * info->n_v_class;

					break;

				default: printf ("error\n");  /* some error handler */
			}
		}

	}
	else if(info->vc_allocator_type == TWO_STAGE_ARB && info->n_v_channel > 1 && info->n_in > 1){
		if (info->vc_in_arb_model && info->vc_out_arb_model){
			/*first stage*/
			req_width = info->n_v_channel;
			switch (info->vc_in_arb_model) {
				case MATRIX_ARBITER: //assumes 30% spacing for each arbiter
					router_area->vc_allocator = ((AreaNOR * 2 * (req_width - 1) * req_width) + (AreaINV * req_width)
							+ (AreaDFF * (req_width * (req_width - 1)/2))) * 1.3 * info->n_in * info->n_v_channel * info->n_v_class;
					break;

				case RR_ARBITER: //assumes 30% spacing for each arbiter
					router_area->vc_allocator = ((6 *req_width * AreaNOR) + (2 * req_width * AreaINV) + (req_width * AreaDFF)) * 1.3 
										* info->n_in * info->n_v_channel * info->n_v_class ;
					break;

				case QUEUE_ARBITER:
					router_area->vc_allocator = AreaDFF * info->vc_in_arb_queue_info.data_width
						* info->vc_in_arb_queue_info.n_set * info->n_in * info->n_v_channel * info->n_v_class ; 

					break;

				default: printf ("error\n");  /* some error handler */
			}

			/*second stage*/
			req_width = (info->n_in - 1) * info->n_v_channel;
			switch (info->vc_out_arb_model) {
				case MATRIX_ARBITER: //assumes 30% spacing for each arbiter
				router_area->vc_allocator += ((AreaNOR * 2 * (req_width - 1) * req_width) + (AreaINV * req_width)
						+ (AreaDFF * (req_width * (req_width - 1)/2))) * 1.3 * info->n_out * info->n_v_channel * info->n_v_class;
				break;

				case RR_ARBITER: //assumes 30% spacing for each arbiter
				router_area->vc_allocator += ((6 *req_width * AreaNOR) + (2 * req_width * AreaINV) + (req_width * AreaDFF)) * 1.3
										* info->n_out * info->n_v_channel * info->n_v_class;
				break;

				case QUEUE_ARBITER:
				router_area->vc_allocator += AreaDFF * info->vc_out_arb_queue_info.data_width
					* info->vc_out_arb_queue_info.n_set * info->n_out * info->n_v_channel * info->n_v_class;

				break;

				default: printf ("error\n");  /* some error handler */
			}


		}
	}
	else if(info->vc_allocator_type == VC_SELECT && info->n_v_channel > 1) {
		switch (info->vc_select_buf_type) {
			case SRAM:
				bitline_len = info->n_v_channel * (RegCellHeight + 2 * WordlineSpacing);
				wordline_len = SIM_logtwo(info->n_v_channel) * (RegCellWidth + 2 * (info->vc_select_buf_info.read_ports
							+ info->vc_select_buf_info.write_ports) * BitlineSpacing);
				router_area->vc_allocator = info->n_out * info->n_v_class * (bitline_len * wordline_len);

				break;

			case REGISTER:
				router_area->vc_allocator = AreaDFF * info->n_out * info->n_v_class* info->vc_select_buf_info.data_width
					* info->vc_select_buf_info.n_set;

				break;

			default: printf ("error\n");  /* some error handler */

		}
	}

	return 0;

}

double SIM_router_area(SIM_router_area_t *router_area)
{
	double Atotal;
	Atotal = router_area->buffer + router_area->crossbar + router_area->vc_allocator + router_area->sw_allocator;

#if( PARM(TECH_POINT) <= 90 )
	fprintf(stdout, "Abuffer:%g\t ACrossbar:%g\t AVCAllocator:%g\t ASWAllocator:%g\t Atotal:%g\n", router_area->buffer, router_area->crossbar, router_area->vc_allocator, router_area->sw_allocator,  Atotal);	

	st_base[0].block_area=router_area->buffer;
	st_base[1].block_area=router_area->crossbar;
	st_base[2].block_area=router_area->vc_allocator;
	st_base[3].block_area=router_area->sw_allocator;


#else
    fprintf(stderr, "Router area is only supported for 90nm, 65nm, 45nm and 32nm\n");
	st_base[0].block_area=0;
	st_base[1].block_area=0;
	st_base[2].block_area=0;
	st_base[3].block_area=0;

#endif
	return Atotal;
}

