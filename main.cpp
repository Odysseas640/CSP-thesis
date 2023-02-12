#include "flight_plan.hpp"
#include "check_assignment.hpp"
#include "print_and_copy_results.hpp"
// make && valgrind ./crewas -p Pairings.txt -n 24
// make && valgrind --leak-check=full ./crewas -i -p Pairings.txt -n 24 < test_assignmentz.txt

int main(int argc, char const *argv[]) {
	struct timeval program_start, program_end;
	gettimeofday(&program_start, NULL);

	int nPilots = -2;
	char *pairings_file = NULL;
	char* starttime = NULL;
	char* endtime = NULL;
	int check_assignment_i = 0;
<<<<<<< HEAD
	int naxos_time_limit_seconds = 0;
	int iterations_limit = 987654321;
	int debug_print = 0;
=======
>>>>>>> 0b57f48bbfec2ebf4f821747da225685b1ee3676
	// Read arguments
	read_arguments(argc, argv, nPilots, pairings_file, starttime, endtime, check_assignment_i, naxos_time_limit_seconds, iterations_limit, debug_print);

	DateTime starttime_dt, endtime_dt; // Convert these, so they're easier to manage.
	fill_time_struct(starttime_dt, starttime);
	fill_time_struct(endtime_dt, endtime);
	// Read all flight plans and put them in a vector
	std::vector<flightPlan*> *FPvector = new std::vector<flightPlan*>;
	// printf("Pre-processing......\n");
	int FPs_read = read_file_into_vector(FPvector, pairings_file, starttime_dt, endtime_dt);
	if (FPs_read < 1) {
		printf("ERROR: No flight plans found from starttime to endtime.\n");
		delete FPvector;
		delete[] pairings_file;
		exit(1);
	}
	float IFT = calculate_IFT(FPvector, nPilots);
	set_minutes_from_global_start_date(FPvector, starttime); // Store flight plan start/end datetimes as minutes.
	// printf("......done\n");

	if (check_assignment_i == 1) {
		check_assignment(FPvector, starttime_dt, endtime_dt);
		for (int i = 0; i < (int) FPvector->size(); ++i)
			delete (*FPvector)[i];
		delete FPvector;
		delete[] pairings_file;
		delete[] starttime;
		delete[] endtime;
		exit(0);
	}

	int days = -1, extra_days = -1, rolling_weeks = -2;;
	get_number_of_days(days, extra_days, FPvector, starttime, endtime);
	get_number_of_rolling_weeks(rolling_weeks, days);
	// printf("Pilots: %d, FPs_read: %d, iterationz: %d\n", nPilots, FPs_read, ITERATIONZ);
	// printf("IFT: %f, days0: (%d - %d) extra: %d, ROLLING_WEEKS_0: (%d - %d)\n", IFT, days0, days, extra_days, ROLLING_WEEKS_0, rolling_weeks);

	// Every flight plan has only one pilot. So, one variable for every flight plan,
	// and it gets an Int value that says who's the pilot.
	/////////////////////////////////////////////////////////////////
	naxos::NsProblemManager pm;
	naxos::NsIntVarArray FP;
	for (int i = 0; i < (int) FPvector->size(); ++i)
		FP.push_back(naxos::NsIntVar(pm, 1, nPilots));

	// Constraint 1/3 and 2/3:
	for (int i = 0; i < (int) FPvector->size(); ++i) {
		for (int j = 0; j <= i; ++j) {
			if ((i == j)
			/* i ends before j starts, and j starts >=660 after i ends */
			|| (FPvector->at(i)->end < FPvector->at(j)->start && FPvector->at(j)->start - FPvector->at(i)->end >= 660)
			/* j ends before i starts, and i starts >=660 after j ends */
			|| (FPvector->at(j)->end < FPvector->at(i)->start && FPvector->at(i)->start - FPvector->at(j)->end >= 660))
				continue;
			else
				pm.add(FP[i] != FP[j]);
		}
	}

	// Constraint 3/3 - Pilots must have 2 days off in every rolling week
	// Step 1:
	// Have a variable for every pilot * every flight plan. Like nPilots * 3304.
	// 0/1 value, if the pilot has that plan or not.
	naxos::NsIntVarArray PilotAssignments; // 2D array in 1D format like in Parallel Systems
	for (int ip = 0; ip < nPilots; ++ip)
		for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp)
			PilotAssignments.push_back(FP[ifp] == ip + 1); // True if flight plan ifp has pilot ip
	// Step 2:
	// This array contains, for every pilot, for every day of the global
	// schedule, how many FPs he has that touch that day even for a minute
	// (so that day isn't a day off). Usually from 0 to 2, could be up to 3.
	naxos::NsIntVarArray PilotHasWorkInThatDay; // Size nPilots * days
	naxos::NsDeque<naxos::NsIntVarArray> temps(nPilots * days); // Intermediary array that helps to translate PilotAssignments into PilotHasWorkThatDay
	for (int ip = 0; ip < nPilots; ++ip) {
		for (int d = 0; d < days; ++d) {
			for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
				if (FP_touches_day(FPvector->at(ifp), d))
					temps[ip * days + d].push_back(PilotAssignments[ip * FPvector->size() + ifp]);
			}
			if (temps[ip * days + d].size() > 0) // Flight plans exist that touch day d
				PilotHasWorkInThatDay.push_back(naxos::NsSum(temps[ip * days + d])); // Array shows how many FPs touch that day
			else { // No flight plans at all touch day d
				PilotHasWorkInThatDay.push_back(naxos::NsIntVar(pm, 0, 0));
				PilotHasWorkInThatDay[ip * days + d].set(0); // Put a 0 variable that says...
				// ...pilot has no work that day. He couldn't have work, cause there's no flights.
			}
		}
	}
	// Step 3:
	// This array has size (nPilots * rolling_weeks). It contains, for every pilot ip,
	// for every rolling week iw, how many days of that week are touched by a flight plan assigned to that pilot.
	naxos::NsIntVarArray PilotBusyDaysInRollingWeek;
	// This constraint only matters if there's 6 days or more.
	if (days >= 6) {
		for (int ip = 0; ip < nPilots; ++ip) {
			for (int iw = 0; iw < rolling_weeks; ++iw) {
				if (days >= 7)
					PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * days + iw] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 1] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 2] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 3] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 4] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 5] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 6] > 0));
				else // if (days == 6)
					PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * days + iw] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 1] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 2] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 3] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 4] > 0)
					                               + (PilotHasWorkInThatDay[ip * days + iw + 5] > 0));
				// Step 4:
				// Constraint: Every rolling week must have <= 5 busy days.
				pm.add(PilotBusyDaysInRollingWeek[ip * rolling_weeks + iw] <= 5);
			}
		}
	}
	// Constraint 3/3 - END

	///////////////////////// FLYING TIME OPTIMIZATION - Uses step 1 from constraint 3/3
	// A pseudo-2D array in 1D format, like in Parallel systems, with dimensions nPilots * FPs.
	// Each variable in this array is PilotAssignments[ip][ifp] * FP_flying_time[ifp]
	naxos::NsIntVarArray PilotAssignmentsTimesFlyingTime;
	for (int ip = 0; ip < nPilots; ++ip) {
		for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
			PilotAssignmentsTimesFlyingTime.push_back(PilotAssignments[ip * FPvector->size() + ifp] * FPvector->at(ifp)->flying_time);
		}
	}
	// Then, the flying time of a pilot will be NsSum(pilot's row in the 2D array)
	naxos::NsIntVarArray PilotFlyingTime;
	for (int ip = 0; ip < nPilots; ++ip)
		PilotFlyingTime.push_back(naxos::NsSum(PilotAssignmentsTimesFlyingTime, ip * FPvector->size(), FPvector->size()));
	// Now calculate the optimization parameter V = SUM( (PilotFlyingTime(i) - IFT) ^ 2 )
	naxos::NsIntVarArray PilotFlyingTimeMinusIFTSquared;
	for (int ip = 0; ip < nPilots; ++ip)
		PilotFlyingTimeMinusIFTSquared.push_back((PilotFlyingTime[ip] - IFT) * (PilotFlyingTime[ip] - IFT));
	naxos::NsIntVar V = naxos::NsSum(PilotFlyingTimeMinusIFTSquared, 0, nPilots);
	pm.minimize(V);
	///////////////////////// END OF FLYING TIME OPTIMIZATION
	pm.addGoal(new naxos::NsgLabeling(FP));

	std::vector< std::vector<int> * > *PilotAssignments_Final_Result = NULL;
	std::vector<int> *PilotFlyingTime_Final_Result = NULL;
	std::vector< std::vector<int> * > *PilotHasWorkInThatDay_Final_Result = NULL;
	std::vector< std::vector<int> * > *PilotBusyDaysInRollingWeek_Final_Result = NULL;
	if (naxos_time_limit_seconds > 0)
		pm.timeLimit(naxos_time_limit_seconds);
	int i = 0;
	while (i++ < iterations_limit && pm.nextSolution() != false) {
		gettimeofday(&program_end, NULL);
		if (debug_print == 1) {
			printf(" - Time: %ld:%02ld:%02ld", (program_end.tv_sec - program_start.tv_sec)/3600, ((program_end.tv_sec - program_start.tv_sec)/60)%60, (program_end.tv_sec - program_start.tv_sec)%60);
			printf(", iteration %d: Solution found with V = %ld\n", i, V.value());
		}

		// Copy Pilot Assignments to Final Result, as recommended in the Naxos Solver manual.
		copy_to_final_assignments(PilotAssignments_Final_Result, FPvector, PilotAssignments, nPilots);
		copy_to_final_flying_time(PilotFlyingTime_Final_Result, PilotFlyingTime, nPilots);
		copy_to_final_assignments(PilotHasWorkInThatDay_Final_Result, PilotHasWorkInThatDay, nPilots, days);
		copy_to_final_assignments(PilotBusyDaysInRollingWeek_Final_Result, PilotBusyDaysInRollingWeek, nPilots, rolling_weeks);
	}
	if (debug_print == 0) {
		print_final_assignments(PilotAssignments_Final_Result);
		print_final_assignments_to_file(PilotAssignments_Final_Result);
	}
	else if (debug_print == 1) {
		print_pilot_FPs(PilotAssignments_Final_Result, PilotFlyingTime_Final_Result);
		print_pilot_days(PilotHasWorkInThatDay_Final_Result, nPilots, days, extra_days);
		if (days >= 6)
			print_pilot_weeks(PilotBusyDaysInRollingWeek_Final_Result, nPilots, rolling_weeks, extra_days);

		printf("IFT: %f, days: %d, rolling weeks: %d, extra_days: %d\n", IFT, days, rolling_weeks, extra_days);
	}
	for (int i = 0; i < PilotFlyingTime.size(); ++i) {
		printf("%ld\n", PilotFlyingTime[i].value());
	}
	/////////////////////////////////////////////////////////////////

	for (int i = 0; i < (int) FPvector->size(); ++i)
		delete (*FPvector)[i];
	delete FPvector;
	delete[] pairings_file;
	delete[] starttime;
	delete[] endtime;
	if (PilotAssignments_Final_Result != NULL) {
		for (int i = 0; i < nPilots; ++i)
			delete PilotAssignments_Final_Result->at(i);
		delete PilotAssignments_Final_Result;
	}
	if (PilotFlyingTime_Final_Result != NULL) {
		delete PilotFlyingTime_Final_Result;
	}
	if (PilotHasWorkInThatDay_Final_Result != NULL) {
		for (int i = 0; i < nPilots; ++i)
			delete PilotHasWorkInThatDay_Final_Result->at(i);
		delete PilotHasWorkInThatDay_Final_Result;
	}
	if (PilotBusyDaysInRollingWeek_Final_Result != NULL) {
		for (int i = 0; i < nPilots; ++i)
			delete PilotBusyDaysInRollingWeek_Final_Result->at(i);
		delete PilotBusyDaysInRollingWeek_Final_Result;
	}
	gettimeofday(&program_end, NULL);
	long seconds = (program_end.tv_sec - program_start.tv_sec);
	long micros = ((seconds * 1000000) + program_end.tv_usec) - (program_start.tv_usec);
	float time_taken = ((float) micros) / 1000000;
	if (naxos_time_limit_seconds > 0 && time_taken >= naxos_time_limit_seconds && debug_print == 1)
		printf("Time limit reached.\n");
	if (debug_print == 1)
		printf("Finished. Time taken: %.2f seconds.\n", ((float) micros) / 1000000);
	return 0;
}