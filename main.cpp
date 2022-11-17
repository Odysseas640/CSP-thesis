#include <iostream>
#include <cstring>
#include <sys/time.h>
#include "naxos-2.0.5/core/naxos.h"
#include "flight_plan.hpp"
#define FLIGHT_PLANS_TO_READ -1 // -1 means ALL
#define PILOTZ 49 // Pilots count from 1, not 0

int main(int argc, char const *argv[]) {
	struct timeval program_start, program_end;
	gettimeofday(&program_start, NULL);

	char *pairings_file = new char[256];
	memset(pairings_file, '\0', 256);
	strcpy(pairings_file, "./Pairings.txt");
	// Read all flight plans and put them in a vector
	std::vector<flightPlan*> *FPvector = new std::vector<flightPlan*>;
	printf("Pre-processing......");
	read_file_into_vector(FPvector, pairings_file, FLIGHT_PLANS_TO_READ);
	float IFT = calculate_IFT(FPvector, PILOTZ);

	// Store flight plan start/end datetimes as minutes from GLOBAL_START_DATE (2011/11/1)
	set_minutes_from_global_start_date(FPvector, 1, 11, 2011);
	printf(" done\n");

	// for (int i = 0; i < (int) FPvector->size(); ++i) {
	// 	print((*FPvector)[i]);
	// }
	printf("IFT: %f\n", IFT);
	int DAYZ = get_number_of_days(FPvector);
	int ROLLING_WEEKS = get_number_of_rolling_weeks(FPvector);
	printf("DAYZ: %d, ROLLING_WEEKS: %d\n", DAYZ, ROLLING_WEEKS);

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
			else if (FPvector->at(i)->start >= FPvector->at(j)->end && FPvector->at(i)->start - FPvector->at(j)->end <= 660) // If i starts after j ends, and within 11 hours
				pm.add(FP[i]!=FP[j]);
			else if (FPvector->at(j)->start >= FPvector->at(i)->end && FPvector->at(j)->start - FPvector->at(i)->end <= 660) // If j starts after i ends, and within 11 hours
				pm.add(FP[i]!=FP[j]);
		}
	}

	// Constraint 3/3 - Pilots must have 2 days off in every rolling week
	// Step 1:
	// Have a variable for every pilot * every flight plan. Like 26 * 3304.
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
	naxos::NsDeque<naxos::NsIntVarArray> temps(PILOTZ * DAYZ); // Intermediary array that helps to translate PilotAssignments into PilotHasWorkThatDay
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int d = 0; d < DAYZ; ++d) {
			for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
				if (FP_touches_day(FPvector->at(ifp), d)) {
					temps[ip * DAYZ + d].push_back(PilotAssignments[ip * FPvector->size() + ifp]);
					// printf("FP n. %d touches day %d (%d - %d)\n", ifp, d, FPvector->at(ifp)->start, FPvector->at(ifp)->end);
				}
				// else
				// 	printf("FP n. %d does not touch day %d (%d - %d)\n", ifp, d, FPvector->at(ifp)->start, FPvector->at(ifp)->end);
			}
			if (temps[ip * DAYZ + d].size() > 0) { // Flight plans exist that touch day d
				// The 2 lines below are equivalent, and require the corresponding line to be active in step 3
				PilotHasWorkInThatDay.push_back(naxos::NsSum(temps[ip * DAYZ + d])); // Array shows how many FPs touch that day
				// PilotHasWorkInThatDay.push_back(naxos::NsSum(temps[ip * DAYZ + d]) > 0); // Array shows if an FP touches that day
			}
			else { // No flight plans at all touch day d
				PilotHasWorkInThatDay.push_back(naxos::NsIntVar(pm, 0, 0));
				PilotHasWorkInThatDay[ip * DAYZ + d].set(0); // Put a 0 variable that says...
				// ...pilot has no work that day. He couldn't have work, cause there's no flights.
			}
		}
	}
	// Step 3:
	// This array has size (PILOTZ * ROLLING_WEEKS). It contains, for every pilot ip,
	// for every rolling week iw, how many days of that week are touched by a flight plan assigned to that pilot.
	naxos::NsIntVarArray PilotBusyDaysInRollingWeek;
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int iw = 0; iw < ROLLING_WEEKS; ++iw) {
			if (DAYZ >= 7)
				// The 2 lines below are equivalent, and require the corresponding line to be active in step 2
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ + iw, 7));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 2] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 3] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 4] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 5] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 6] > 0));
			else if (DAYZ == 6)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ + iw, 6));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 2] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 3] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 4] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 5] > 0));
			else if (DAYZ == 5)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ + iw, 6));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 2] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 3] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 4] > 0));
			else if (DAYZ == 4)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ + iw, 6));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 2] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 3] > 0));
			else if (DAYZ == 3)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ + iw, 6));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 2] > 0));
			else if (DAYZ == 2)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ + iw, 6));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * DAYZ + iw + 1] > 0));
			else if (DAYZ == 1)
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * DAYZ + iw, 6));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * DAYZ + iw] > 0));
			// This constraint only matters if there's 6 days or more. Otherwise it's just for consistency.
			// Step 4:
			// Constraint: Every week must have <= 5 busy days.
			pm.add(PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS + iw] <= 5);
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
		printf("  --  SOLUTION FOUND, V = %ld  --\n", V.value());
		// for (int i = 0; i < (int) FPvector->size(); ++i) {
		// 		printf("Flight plan %d -> pilot %ld\n", i+1, FP[i].value());
		// }
		if (++iterationz >= 9)
			break;
	}
	// else
	// 	printf("\n  --  NO SOLUTION FOUND  --\n\n");

	// printf("\n  --  PilotFlyingTime  --\n");
	// for (int ip = 0; ip < PILOTZ; ++ip) {
	// 	// printf("Pilot %d has flying time %ld and FPs:", ip + 1, PilotFlyingTime[ip].value());
	// 	for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
	// 		// if (PilotAssignments[ip * FPvector->size() + ifp].value() == 1)
	// 		// 	printf(" %d", ifp + 1);
	// 		// DIAGNOSTIC FOR STEP 1
	// 		if (PilotAssignments[ip * FPvector->size() + ifp].value() == true && FP[ifp].value() != ip+1)
	// 			printf("MISTAKE\n"); // Boolean == 1, but FP doesn't have this pilot, so MISTAKE
	// 		if (PilotAssignments[ip * FPvector->size() + ifp].value() == false && FP[ifp].value() == ip+1)
	// 			printf("MISTAKE\n");
	// 		// END OF DIAGNOSTIC FOR STEP 1
	// 	}
	// 	// printf("\n");
	// }
	// printf("PilotHasWorkInThatDay\n");
	// for (int ip = 0; ip < PILOTZ; ++ip) {
	// 	printf("Pilot %2d:", ip+1);
	// 	for (int d = 0; d < DAYZ; ++d) {
	// 		printf(" %ld", PilotHasWorkInThatDay[ip * DAYZ + d].value());
	// 	}
	// 	printf("\n");
	// }
	// printf("PilotBusyDaysInRollingWeek\n");
	// for (int ip = 0; ip < PILOTZ; ++ip) {
	// 	printf("Pilot %d busy days in rolling weeks:", ip+1);
	// 	for (int iw = 0; iw < ROLLING_WEEKS; ++iw) {
	// 		printf(" %ld", PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS + iw].value());
	// 		// printf("Pilot %d busy days in week %d: %ld\n", ip+1, iw+1, PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS + iw].value());
	// 	}
	// 	printf("\n");
	// }
	printf("DAYZ: %d, ROLLING_WEEKS weeks: %d, V: %ld\n", DAYZ, ROLLING_WEEKS, V.value());
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