#ifndef __ODYS_PRINT_AND_COPY__
#define __ODYS_PRINT_AND_COPY__
#include <iostream>
#include <cstring>
#include <sys/time.h>
#include "naxos-2.0.5/core/naxos.h"
#include "flight_plan.hpp"
#include "check_assignment.hpp"

void copy_to_final_assignments(std::vector< std::vector<int> * > *&, const std::vector<flightPlan*>*, const naxos::NsIntVarArray& PilotAssignments, int);
void copy_to_final_flying_time(std::vector<int> *&, const naxos::NsIntVarArray&, int);
void copy_to_final_assignments(std::vector< std::vector<int> * > *&PilotHasWorkInThatDay_Final_Result, const naxos::NsIntVarArray& PilotHasWorkInThatDay, int pilotz, int days2);
void print_final_assignments(const std::vector< std::vector<int> * > *);
void print_final_assignments_to_file(const std::vector< std::vector<int> * > *);
void print_pilot_FPs(std::vector< std::vector<int> * > *&PilotAssignments_Final_Result, std::vector<int> *PilotFlyingTime_Final_Result);
void print_pilot_days(std::vector< std::vector<int> * > *&PilotHasWorkInThatDay_Final_Result, int nPilots, int days2, int extra_days);
void print_pilot_weeks(std::vector< std::vector<int> * > *&PilotBusyDaysInRollingWeek_Final_Result, int nPilots, int rolling_weeks, int extra_days);
int read_arguments(char argc, const char* argv[], int& nPilots, char*& pairings_file, char*& starttime, char*& endtime, int& check_assignment_i, int& naxos_time_limit_seconds, int& iterations_limit, int& debug_print);
#endif