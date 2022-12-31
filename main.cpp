#include <iostream>
#include <cstring>
#include <sys/time.h>
#include "naxos-2.0.5/core/naxos.h"
#include "flight_plan.hpp"
#include "check_assignment.hpp"
#define ITERATIONZ 1

int main(int argc, char const *argv[]) {
	char starttime[17];
	char endtime[17];
	strcpy(starttime, "2011-11-01/00:00");
	strcpy(endtime, "2012-02-29/23:59"); // When there's no FPs included, segmentation fault. Fix it.
	struct timeval program_start, program_end;
	gettimeofday(&program_start, NULL);

	char *pairings_file = NULL;
	int PILOTZ = -2;
	int check_assignment_i = 0;
	///////////////////////////////////////////// Read arguments
	for (int i = 1; i < argc-1; i++) {
		if (strcmp(argv[i],"-n") == 0) {
			for (int j = 0; j < (int) strlen(argv[i+1]); ++j) {
				if (argv[i+1][j] > '9' || argv[i+1][j] < '0') {
					printf("Number of pilots is not a number. Terminating.\n");
					return 1;
				}
			}
			PILOTZ = atoi(argv[i+1]);
		}
		else if (strcmp(argv[i],"-p") == 0) {
			pairings_file = new char[strlen(argv[i+1]) + 1];
			strcpy(pairings_file, argv[i+1]);
		}
		else if (strcmp(argv[i],"-s") == 0)
			strncpy(starttime, argv[i+1], 16);
		else if (strcmp(argv[i],"-e") == 0)
			strncpy(endtime, argv[i+1], 16);
		else if (strcmp(argv[i],"-i") == 0)
			check_assignment_i = 1;
	}
	if (PILOTZ < 0) {
		printf("Error: No number of pilots specified.\n");
		return(1);
	}
	if (pairings_file == NULL) {
		printf("Error: No input file specified.\n");
		return(1);
	}
	///////////////////////////////////////////// Done reading arguments
	DateTime starttime_dt, endtime_dt;
	fill_time_struct(starttime_dt, starttime);
	fill_time_struct(endtime_dt, endtime);
	printf("STARTTIME: %s, ENDTIME: %s\n", starttime, endtime);
	// Read all flight plans and put them in a vector
	std::vector<flightPlan*> *FPvector = new std::vector<flightPlan*>;
	printf("Pre-processing......\n");
	int FPs_read = read_file_into_vector(FPvector, pairings_file, starttime_dt, endtime_dt);
	float IFT = calculate_IFT(FPvector, PILOTZ);

	// Store flight plan start/end datetimes as minutes.
	set_minutes_from_global_start_date(FPvector, starttime);
	printf("......done\n");

	if (check_assignment_i == 1) {
		check_assignment(FPvector, starttime_dt, endtime_dt);
		for (int i = 0; i < (int) FPvector->size(); ++i)
			delete (*FPvector)[i];
		delete FPvector;
		delete[] pairings_file;
		exit(0);
	}

	printf("FPvector:\n");
	for (int i = 0; i < (int) FPvector->size(); ++i) {
		print((*FPvector)[i]);
	}
	int DAYZ = -1, DAYZ_2 = -1, extra_days = -1;
	if (get_number_of_days(DAYZ, DAYZ_2, extra_days, FPvector, starttime, endtime) < 0)
		return 1;
	int ROLLING_WEEKS = -1, ROLLING_WEEKS_2 = -2;
	get_number_of_rolling_weeks(ROLLING_WEEKS, ROLLING_WEEKS_2, DAYZ, DAYZ_2);
	printf("Pilots: %d, FPs_read: %d, iterationz: %d\n", PILOTZ, FPs_read, ITERATIONZ);
	printf("IFT: %f, DAYZ: (%d - %d) extra: %d, ROLLING_WEEKS: (%d - %d)\n", IFT, DAYZ, DAYZ_2, extra_days, ROLLING_WEEKS, ROLLING_WEEKS_2);

	// Every flight plan has only one pilot. So, one variable for every flight plan,
	// and it gets an Int value that says who's the pilot.
	/////////////////////////////////////////////////////////////////
	naxos::NsProblemManager pm;
	naxos::NsIntVarArray FP;
	for (int i = 0; i < (int) FPvector->size(); ++i)
		FP.push_back(naxos::NsIntVar(pm, 1, PILOTZ));

	// Constraint 1/3:
	// If 2 FPs have the same pilot, they cannot overlap.
	for (int i = 0; i < (int) FPvector->size(); ++i) { // With only this constraint, 100 FPs need 11 pilots.
		for (int j = 0; j < (int) FPvector->size(); ++j) {
			if (i == j)
				continue;
			else if (/* i starts before j ends */ FPvector->at(i)->start <= FPvector->at(j)->end && /* i starts after j starts*/ FPvector->at(i)->start >= FPvector->at(j)->start)
				pm.add(FP[i] != FP[j]);
			else if (/* i ends after j starts */ FPvector->at(i)->end >= FPvector->at(j)->start && /* i ends before j ends */ FPvector->at(i)->end <= FPvector->at(j)->end)
				pm.add(FP[i] != FP[j]);
		}
	}

	// Constraint 2/3:
	// If 2 FPs have the same pilot, they must be 660+ minutes apart.
	for (int i = 0; i < (int) FPvector->size(); ++i) { // With this constraint too, 100 FPs need 24 pilots.
		for (int j = 0; j < (int) FPvector->size(); ++j) {
			if (i == j)
				continue;
			else if (FPvector->at(i)->start >= FPvector->at(j)->end && FPvector->at(i)->start - FPvector->at(j)->end < 660) // If i starts after j ends, and within 11 hours
				pm.add(FP[i]!=FP[j]);
			else if (FPvector->at(j)->start >= FPvector->at(i)->end && FPvector->at(j)->start - FPvector->at(i)->end < 660) // If j starts after i ends, and within 11 hours
				pm.add(FP[i]!=FP[j]);
		}
	}

	// Constraint 3/3 - Pilots must have 2 days off in every rolling week
	// Step 1:
	// Have a variable for every pilot * every flight plan. Like PILOTZ * 3304.
	// 0/1 value, if the pilot has that plan or not.
	naxos::NsIntVarArray PilotAssignments; // 2D array in 1D format like in Parallel Systems
	for (int ip = 0; ip < PILOTZ; ++ip)
		for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp)
			PilotAssignments.push_back(FP[ifp] == ip + 1); // True if flight plan ifp has pilot ip
	// Step 2:
	// This array contains, for every pilot, for every day of the global
	// schedule, how many FPs he has that touch that day even for a minute
	// (so that day isn't a day off). Usually from 0 to 2, could be up to 3.
	naxos::NsIntVarArray PilotHasWorkInThatDay; // Size PILOTZ * DAYS
	naxos::NsDeque<naxos::NsIntVarArray> temps(PILOTZ * DAYZ_2); // Intermediary array that helps to translate PilotAssignments into PilotHasWorkThatDay
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int d = 0; d < DAYZ_2; ++d) {
			for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
				if (FP_touches_day(FPvector->at(ifp), d)) {
					temps[ip * DAYZ_2 + d].push_back(PilotAssignments[ip * FPvector->size() + ifp]);
					// printf("FP n. %d touches day %d (%d - %d)\n", ifp, d, FPvector->at(ifp)->start, FPvector->at(ifp)->end);
				}
				// else
				// 	printf("FP n. %d does not touch day %d (%d - %d)\n", ifp, d, FPvector->at(ifp)->start, FPvector->at(ifp)->end);
			}
			if (temps[ip * DAYZ_2 + d].size() > 0) { // Flight plans exist that touch day d
				// The 2 lines below are equivalent, and require the corresponding line to be active in step 3
				PilotHasWorkInThatDay.push_back(naxos::NsSum(temps[ip * DAYZ_2 + d])); // Array shows how many FPs touch that day
				// PilotHasWorkInThatDay.push_back(naxos::NsSum(temps[ip * DAYZ_2 + d]) > 0); // Array shows if an FP touches that day
			}
			else { // No flight plans at all touch day d
				PilotHasWorkInThatDay.push_back(naxos::NsIntVar(pm, 0, 0));
				PilotHasWorkInThatDay[ip * DAYZ_2 + d].set(0); // Put a 0 variable that says...
				// ...pilot has no work that day. He couldn't have work, cause there's no flights.
			}
		}
	}
	// Step 3:
	// This array has size (PILOTZ * ROLLING_WEEKS_2). It contains, for every pilot ip,
	// for every rolling week iw, how many days of that week are touched by a flight plan assigned to that pilot.
	naxos::NsIntVarArray PilotBusyDaysInRollingWeek;
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int iw = 0; iw < ROLLING_WEEKS_2; ++iw) {
			if (DAYZ_2 >= 7)
				// The 2 lines below are equivalent, and require the corresponding line to be active in step 2
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ_2 + iw, 7));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ_2 + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 2] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 3] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 4] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 5] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 6] > 0));
			else if (DAYZ_2 == 6)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ_2 + iw, 6));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ_2 + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 2] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 3] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 4] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 5] > 0));
			else if (DAYZ_2 == 5)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ_2 + iw, 5));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ_2 + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 2] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 3] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 4] > 0));
			else if (DAYZ_2 == 4)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ_2 + iw, 4));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ_2 + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 2] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 3] > 0));
			else if (DAYZ_2 == 3)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ_2 + iw, 3));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ_2 + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 2] > 0));
			else if (DAYZ_2 == 2)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ_2 + iw, 2));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ_2 + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 1] > 0));
			else if (DAYZ_2 == 1)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ_2 + iw, 1));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ_2 + iw] > 0));
			// This constraint only matters if there's 6 days or more. Otherwise it's just for consistency.
			// Step 4:
			// Constraint: Every week must have <= 5 busy days.
			pm.add(PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS_2 + iw] <= 5); // Original constraint

			// IDEA: Find an expression that says if pilot ip has work in a day between DAYZ and DAYZ_2.
			// Specifically, the latest of these days. If pilot ip has no work in those days, 
			// if (ROLLING_WEEKS == ROLLING_WEEKS_2)
			// 	pm.add(PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS_2 + iw] <= 5);
			// else {
			// 	int extra_days = ROLLING_WEEKS_2 - ROLLING_WEEKS;
			// 	for (int iexd = 0; iexd < extra_days; ++iexd) {
			// 		pm.add(naxos::NsIfThen(naxos::NsSum(), ))
			// 	}
			// }
			// int extra_days = ROLLING_WEEKS_2 - ROLLING_WEEKS;
			// for (int iexd = 0; iexd <= extra_days; ++iexd) {
			// 	pm.add(PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS_2 + iw] <= 5 || naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ_2 + iw, iexd) == 0);
			// }
			// printf("iw: %d, ROLLING_WEEKS: %d, ROLLING_WEEKS_2: %d\n", iw, ROLLING_WEEKS, ROLLING_WEEKS_2);
			if (iw >= ROLLING_WEEKS)
				pm.add(PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS_2 + iw] <= 5 || naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ_2 + iw +7-extra_days, ROLLING_WEEKS_2 - iw) == 0);
			else
				pm.add(PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS_2 + iw] <= 5);
			// IDEA: Subtract from 5, the number of consecutive days, from the latest day after DAYZ that he has work, til endtime.
			// for example, DAYZ=5, DAYZ_2=8, and last day he has work is 6. So it's 5-2=3, instead of 5, in the last rolling week.
			// in the second to last rolling week, it's 5-1=4.
		}
	}
	// Constraint 3/3 - END

	///////////////////////// FLYING TIME OPTIMIZATION - Uses step 1 from constraint 3/3
	// A pseudo-2D array in 1D format, like in Parallel systems, with dimensions PILOTZ * FPs.
	// Each variable in this array is PilotAssignments[ip][ifp] * FP_flying_time[ifp]
	naxos::NsIntVarArray PilotAssignmentsTimesFlyingTime;
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
			PilotAssignmentsTimesFlyingTime.push_back(PilotAssignments[ip * FPvector->size() + ifp] * FPvector->at(ifp)->flying_time);
		}
	}
	// Then, the flying time of a pilot will be NsSum(pilot's row in the 2D array)
	naxos::NsIntVarArray PilotFlyingTime;
	for (int ip = 0; ip < PILOTZ; ++ip)
		PilotFlyingTime.push_back(naxos::NsSum(PilotAssignmentsTimesFlyingTime, ip * FPvector->size(), FPvector->size()));
	// Now calculate the optimization parameter V = SUM( (PilotFlyingTime(i) - IFT) ^ 2 )
	naxos::NsIntVarArray PilotFlyingTimeMinusIFTSquared;
	for (int ip = 0; ip < PILOTZ; ++ip)
		PilotFlyingTimeMinusIFTSquared.push_back((PilotFlyingTime[ip] - IFT) * (PilotFlyingTime[ip] - IFT));
	naxos::NsIntVar V = naxos::NsSum(PilotFlyingTimeMinusIFTSquared, 0, PILOTZ);
	pm.minimize(V);
	///////////////////////// END OF FLYING TIME OPTIMIZATION
	pm.addGoal(new naxos::NsgLabeling(FP));

	int iterationz = 0;
	// pm.timeLimit(10);
	while (pm.nextSolution() != false) {
		gettimeofday(&program_end, NULL);
		printf("%ld:%02ld:%02ld", (program_end.tv_sec - program_start.tv_sec)/3600, ((program_end.tv_sec - program_start.tv_sec)/60)%60, (program_end.tv_sec - program_start.tv_sec)%60);
		printf("  --  SOLUTION FOUND, V = %ld, i+1: %d\n", V.value(), iterationz+1);
		// for (int i = 0; i < (int) FPvector->size(); ++i) {
		// 		printf("Flight plan %d -> pilot %ld\n", i+1, FP[i].value());
		// }
		if (++iterationz >= ITERATIONZ)
			break;
	}
	// else
	// 	printf("\n  --  NO SOLUTION FOUND  --\n\n");
	print_assignments_to_file(FPvector, PilotAssignments, PILOTZ, (int) FPvector->size());

	printf("\n  --  PilotFlyingTime  --\n");
	for (int ip = 0; ip < PILOTZ; ++ip) {
		printf("Pilot %d has flying time %ld and FPs (list):", ip + 1, PilotFlyingTime[ip].value());
		for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
			if (PilotAssignments[ip * FPvector->size() + ifp].value() == 1)
				printf(" %d", FPvector->at(ifp)->ID);
			// DIAGNOSTIC FOR STEP 1
			// if (PilotAssignments[ip * FPvector->size() + ifp].value() == true && FP[ifp].value() != ip+1)
			// 	printf("MISTAKE\n"); // Boolean == 1, but FP doesn't have this pilot, so MISTAKE
			// if (PilotAssignments[ip * FPvector->size() + ifp].value() == false && FP[ifp].value() == ip+1)
			// 	printf("MISTAKE\n");
			// END OF DIAGNOSTIC FOR STEP 1
		}
		printf("\n");
	}
	printf("PilotHasWorkInThatDay\n");
	for (int ip = 0; ip < PILOTZ; ++ip) {
		printf("Pilot %2d:", ip+1);
		for (int d = 0; d < DAYZ_2; ++d) {
			if (DAYZ_2 > DAYZ && d == DAYZ)
				printf(" |");
			printf(" %ld", PilotHasWorkInThatDay[ip * DAYZ_2 + d].value());
		}
		printf("\n");
	}
	printf("PilotBusyDaysInRollingWeek\n");
	for (int ip = 0; ip < PILOTZ; ++ip) {
		printf("Pilot %d busy days in rolling weeks:", ip+1);
		for (int iw = 0; iw < ROLLING_WEEKS_2; ++iw) {
			if (ROLLING_WEEKS_2 > ROLLING_WEEKS && iw == ROLLING_WEEKS)
				printf(" |");
			printf(" %ld", PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS_2 + iw].value());
		}
		printf("\n");
	}
	printf("V: %ld\n", V.value());
	printf("IFT: %f, DAYZ: %d / %d, ROLLING_WEEKS: %d / %d\n", IFT, DAYZ, DAYZ_2, ROLLING_WEEKS, ROLLING_WEEKS_2);
	/////////////////////////////////////////////////////////////////

	for (int i = 0; i < (int) FPvector->size(); ++i)
		delete (*FPvector)[i];
	delete FPvector;
	delete[] pairings_file;
	gettimeofday(&program_end, NULL);
	long seconds = (program_end.tv_sec - program_start.tv_sec);
	long micros = ((seconds * 1000000) + program_end.tv_usec) - (program_start.tv_usec);
	printf("Time taken: %.2f seconds\n", ((float) micros) / 1000000);
	return 0;
}