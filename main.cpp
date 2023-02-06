#include <iostream>
#include <cstring>
#include <sys/time.h>
#include "naxos-2.0.5/core/naxos.h"
#include "flight_plan.hpp"
#include "check_assignment.hpp"
int read_arguments(char argc, const char* argv[], int& PILOTZ, char*& pairings_file, char*& starttime, char*& endtime, int& check_assignment_i, int& naxos_time_limit_seconds, int& iterations_limit);
// make && valgrind ./crewas -p Pairings.txt -n 24
// make && valgrind --leak-check=full ./crewas -i -p Pairings.txt -n 24 < test_assignmentz.txt

int main(int argc, char const *argv[]) {
	struct timeval program_start, program_end;
	gettimeofday(&program_start, NULL);

	int PILOTZ = -2;
	char *pairings_file = NULL;
	char* starttime = NULL;
	char* endtime = NULL;
	int check_assignment_i = 0;
	int naxos_time_limit_seconds = 0;
	int iterations_limit = 987654321;
	// Read arguments
	read_arguments(argc, argv, PILOTZ, pairings_file, starttime, endtime, check_assignment_i, naxos_time_limit_seconds, iterations_limit);

	// starttime = new char[17]; // FOR TESTING
	// endtime = new char[17];
	// strcpy(starttime, "2011-11-01/00:00");
	// strcpy(endtime, "2011-11-07/23:59");
	// printf("STARTTIME: %s, ENDTIME: %s\n", starttime, endtime);

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
	float IFT = calculate_IFT(FPvector, PILOTZ);
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

	// printf("FPvector:\n");
	// for (int i = 0; i < (int) FPvector->size(); ++i) {
	// 	print((*FPvector)[i]);
	// 	printf("\n");
	// }
	int DAYZ = -1, DAYZ_2 = -1, extra_days = -1;
	get_number_of_days(DAYZ, DAYZ_2, extra_days, FPvector, starttime, endtime);
	int ROLLING_WEEKS = -1, ROLLING_WEEKS_2 = -2;
	get_number_of_rolling_weeks(ROLLING_WEEKS, ROLLING_WEEKS_2, DAYZ, DAYZ_2);
	// printf("Pilots: %d, FPs_read: %d, iterationz: %d\n", PILOTZ, FPs_read, ITERATIONZ);
	// printf("IFT: %f, DAYZ: (%d - %d) extra: %d, ROLLING_WEEKS: (%d - %d)\n", IFT, DAYZ, DAYZ_2, extra_days, ROLLING_WEEKS, ROLLING_WEEKS_2);

	// Every flight plan has only one pilot. So, one variable for every flight plan,
	// and it gets an Int value that says who's the pilot.
	/////////////////////////////////////////////////////////////////
	naxos::NsProblemManager pm;
	naxos::NsIntVarArray FP;
	for (int i = 0; i < (int) FPvector->size(); ++i)
		FP.push_back(naxos::NsIntVar(pm, 1, PILOTZ));

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
	// This constraint only matters if there's 6 days or more.
	if (DAYZ_2 >= 6) {
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
				else // if (DAYZ_2 == 6)
					// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ_2 + iw, 6));
					PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ_2 + iw] > 0)
					                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 1] > 0)
					                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 2] > 0)
					                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 3] > 0)
					                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 4] > 0)
					                               + (PilotHasWorkInThatDay[ip * DAYZ_2 + iw + 5] > 0));
				// Step 4:
				// Constraint: Every rolling week must have <= 5 busy days.
				pm.add(PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS_2 + iw] <= 5);
				// printf("iw: %d, ROLLING_WEEKS: %d, ROLLING_WEEKS_2: %d\n", iw, ROLLING_WEEKS, ROLLING_WEEKS_2);
				//last normal week | extra days
				//    a b c d e f g|h i j
				// A  1 2 3 4 5 6 7|# # #
				// B  - 1 2 3 4 5 6|7 # #
				// C  - - 1 2 3 4 5|6 7 #
				// D  - - - 1 2 3 4|5 6 7
				// -- I just need to count all the rolling weeks with the extra days for everyone, regardless if they have an FP in the extra days or not.
				// --...because, the problem was that the days off in the "extra" days, might allow the last "normal" days to get too many working days.
				// --...but they won't, because the "normal" rolling weeks will take care of that. Too many busy days is a problem. Too many days off is OK.
			}
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

	std::vector< std::vector<int> * > *PilotAssignments_Final_Result = NULL;
	int i = 0;
	if (naxos_time_limit_seconds > 0)
		pm.timeLimit(naxos_time_limit_seconds);
	while (i++ < iterations_limit && pm.nextSolution() != false) {
		gettimeofday(&program_end, NULL);
		printf("%ld:%02ld:%02ld", (program_end.tv_sec - program_start.tv_sec)/3600, ((program_end.tv_sec - program_start.tv_sec)/60)%60, (program_end.tv_sec - program_start.tv_sec)%60);
		printf("  --  SOLUTION FOUND, V = %ld, i: %d\n", V.value(), i);

		// Copy Pilot Assignments to Final Result, as recommended in the Naxos Solver manual.
		copy_to_final_assignments(PilotAssignments_Final_Result, FPvector, PilotAssignments, PILOTZ);
	}
	// print_final_assignments(PilotAssignments_Final_Result);
	// print_final_assignments_to_file(PilotAssignments_Final_Result);

	printf("\n  --  PilotFlyingTime and FPs  --\n");
	for (int ip = 0; ip < PILOTZ; ++ip) {
		printf("Pilot %d has flying time %ld and FPs (list):", ip + 1, PilotFlyingTime[ip].value());
		for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
			if (PilotAssignments[ip * FPvector->size() + ifp].value() == 1)
				printf(" %d", FPvector->at(ifp)->ID);
			// DIAGNOSTIC FOR STEP 1
			if (PilotAssignments[ip * FPvector->size() + ifp].value() == true && FP[ifp].value() != ip+1) {
				printf("MISTAKE\n"); // Boolean == 1, but FP doesn't have this pilot, so MISTAKE
				exit(1);
			}
			if (PilotAssignments[ip * FPvector->size() + ifp].value() == false && FP[ifp].value() == ip+1) {
				printf("MISTAKE\n");
				exit(1);
			}
			// END OF DIAGNOSTIC FOR STEP 1
		}
		printf("\n");
	}

	printf("  --  PilotHasWorkInThatDay  --\n");
	for (int ip = 0; ip < PILOTZ; ++ip) {
		printf("Pilot %2d:", ip+1);
		for (int d = 0; d < DAYZ_2; ++d) {
			if (DAYZ_2 > DAYZ && d == DAYZ)
				printf(" |");
			printf(" %ld", PilotHasWorkInThatDay[ip * DAYZ_2 + d].value());
		}
		printf("\n");
	}

	printf("  --  PilotBusyDaysInRollingWeek  --\n");
	if (DAYZ_2 >= 6) {
		for (int ip = 0; ip < PILOTZ; ++ip) {
			printf("Pilot %d busy days in rolling weeks:", ip+1);
			for (int iw = 0; iw < ROLLING_WEEKS_2; ++iw) {
				if (ROLLING_WEEKS_2 > ROLLING_WEEKS && iw == ROLLING_WEEKS)
					printf(" |");
				printf(" %ld", PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS_2 + iw].value());
			}
			printf("\n");
		}
	}
	else
		printf("N/A\n");

	printf("IFT (as float): %f, DAYZ: %d / %d, ROLLING_WEEKS: %d / %d\n", IFT, DAYZ, DAYZ_2, ROLLING_WEEKS, ROLLING_WEEKS_2);
	/////////////////////////////////////////////////////////////////

	for (int i = 0; i < (int) FPvector->size(); ++i)
		delete (*FPvector)[i];
	delete FPvector;
	delete[] pairings_file;
	delete[] starttime;
	delete[] endtime;
	if (PilotAssignments_Final_Result != NULL) {
		for (int i = 0; i < PILOTZ; ++i)
			delete PilotAssignments_Final_Result->at(i);
		delete PilotAssignments_Final_Result;
	}
	gettimeofday(&program_end, NULL);
	long seconds = (program_end.tv_sec - program_start.tv_sec);
	long micros = ((seconds * 1000000) + program_end.tv_usec) - (program_start.tv_usec);
	float time_taken = ((float) micros) / 1000000;
	if (naxos_time_limit_seconds > 0 && time_taken >= naxos_time_limit_seconds)
		printf("Time limit reached.\n");
	printf("Finished. Time taken: %.2f seconds.\n", ((float) micros) / 1000000);
	return 0;
}

int read_arguments(char argc, const char* argv[], int& PILOTZ, char*& pairings_file, char*& starttime, char*& endtime, int& check_assignment_i, int& naxos_time_limit_seconds, int& iterations_limit) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i],"-n") == 0) {
			for (int j = 0; j < (int) strlen(argv[i+1]); ++j) {
				if (argv[i+1][j] > '9' || argv[i+1][j] < '0') {
					printf("Number of pilots is not a number. Terminating.\n");
					exit(1);
				}
			}
			PILOTZ = atoi(argv[i+1]);
		}
		else if (strcmp(argv[i],"-p") == 0) {
			pairings_file = new char[strlen(argv[i+1]) + 1];
			strcpy(pairings_file, argv[i+1]);
		}
		else if (strcmp(argv[i],"-s") == 0) { // FIX "DATE/TIME" OR "DATE TIME"
			starttime = new char[17];
			strncpy(starttime, argv[i+1], 16);
		}
		else if (strcmp(argv[i],"-e") == 0) {
			endtime = new char[17];
			strncpy(endtime, argv[i+1], 16);
		}
		else if (strcmp(argv[i],"-i") == 0)
			check_assignment_i = 1;
		else if (strcmp(argv[i],"-t") == 0)
			naxos_time_limit_seconds = atoi(argv[i+1]);
		else if (strcmp(argv[i],"-it") == 0)
			iterations_limit = atoi(argv[i+1]);
	}
	if (PILOTZ < 0) {
		printf("Error: No number of pilots specified.\n");
		exit(1);
	}
	if (pairings_file == NULL) {
		printf("Error: No input file specified.\n");
		exit(1);
	}
	if (starttime == NULL) {
		printf("Error: No start time specified.\n");
		exit(1);
	}
	if (endtime == NULL) {
		printf("Error: No end time specified.\n");
		exit(1);
	}
	return 0;
}