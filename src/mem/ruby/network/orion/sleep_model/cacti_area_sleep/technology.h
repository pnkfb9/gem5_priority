#ifndef __TECHNOLOGY__
#define __TECHNOLOGY__

#include "basic_circuit.h"
#include "parameter.h"

namespace CactiArea
{
double wire_resistance_cacti(double resistivity, double wire_width, double wire_thickness,
    double barrier_thickness, double dishing_thickness, double alpha_scatter);

double wire_capacitance_cacti(double wire_width, double wire_thickness, double wire_spacing, 
    double ild_thickness, double miller_value, double horiz_dielectric_constant,
    double vert_dielectric_constant, double fringe_cap);

void init_tech_params_cacti(double technology, bool is_tag);

}

#endif
