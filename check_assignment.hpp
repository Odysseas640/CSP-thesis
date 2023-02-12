#ifndef __ODYS_CHECK_ASSIGNMENT__
#define __ODYS_CHECK_ASSIGNMENT__
#include <vector>
#include <cstdio>
#include <map>
#include "naxos-2.0.5/core/naxos.h"
#include "flight_plan.hpp"

int check_assignment(const std::vector<flightPlan*>*, const DateTime&, const DateTime&);
int check_assignments_11h_gap(const std::vector<std::vector<flightPlan*>*>*);
int check_assignments_rolling_weeks(const std::vector<std::vector<flightPlan*>*>*);

#endif