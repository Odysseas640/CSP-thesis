#include <iostream>
#include <cstring>
#include "naxos-2.0.5/core/naxos.h"
#include "flight_plan.hpp"
#define FLIGHT_PLANS_TO_READ 150 // -1 means ALL
#define PILOTZ 26 // Pilots count from 1, not 0

int main(int argc, char const *argv[]) {
	char *pairings_file = new char[256];
	memset(pairings_file, '\0', 256);
	strcpy(pairings_file, "./Pairings.txt");
	// Read all flight plans and put them in a vector
	std::vector<flightPlan*> *flight_plans_vector = new std::vector<flightPlan*>;
	printf("Pre-processing......");
	read_file_into_vector(flight_plans_vector, pairings_file, FLIGHT_PLANS_TO_READ);
	float IFT = calculate_IFT(flight_plans_vector, PILOTZ);

	// Store flight plan start/end datetimes as minutes from GLOBAL_START_DATE (2011/11/1)
	set_minutes_from_global_start_date(flight_plans_vector, 1, 11, 2011);
	printf(" done\n");

	// for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
	// 	print((*flight_plans_vector)[i]);
	// }
	printf("IFT: %f\n", IFT);
	int DAYZ = get_number_of_days(flight_plans_vector);
	int ROLLING_WEEKS = get_number_of_rolling_weeks(flight_plans_vector);

	// Every flight plan has only one pilot. So, one variable for every flight plan,
	// and it gets an Int value that says who's the pilot.
	/////////////////////////////////////////////////////////////////
	naxos::NsProblemManager pm;
	naxos::NsIntVarArray FP;
	naxos::NsIntVarArray FP_start, FP_end;
	naxos::NsIntVarArray FP_flying_time;
	for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
		FP.push_back(naxos::NsIntVar(pm, 1, PILOTZ));
		FP_start.push_back(naxos::NsIntVar(pm, 1, flight_plans_vector->at(i)->start));
		FP_start[i].set(flight_plans_vector->at(i)->start);
		FP_end.push_back(naxos::NsIntVar(pm, 1, flight_plans_vector->at(i)->end));
		FP_end[i].set(flight_plans_vector->at(i)->end);
		FP_flying_time.push_back(naxos::NsIntVar(pm, 0, flight_plans_vector->at(i)->flying_time));
		FP_flying_time[i].set(flight_plans_vector->at(i)->flying_time);
	}
	///////////////////// DAYS OFF - START
	// Step 1:
	// Have a variable for every pilot * every flight plan. Like 26 * 3304.
	// 0/1 value, if the pilot has that plan or not.
	naxos::NsIntVarArray PilotAssignments; // 2D array in 1D format like in Parallel Systems
	for (int ip = 0; ip < PILOTZ; ++ip)
		for (int ifp = 0; ifp < (int) flight_plans_vector->size(); ++ifp)
			PilotAssignments.push_back(FP[ifp] == ip + 1); // True if flight plan ifp has pilot ip
	// Step 2:
	// This array contains, for every pilot, for every day of the global
	// schedule, how many FPs he has that touch that day even for a minute
	// (so that day isn't a day off). Usually from 0 to 2, could be up to 3.
	naxos::NsIntVarArray PilotHasWorkInThatDay; // Size PILOTZ * DAYS
	naxos::NsDeque<naxos::NsIntVarArray> temps(PILOTZ * DAYZ); // Intermediary array that helps to translate PilotAssignments into PilotHasWorkThatDay
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int d = 0; d < DAYZ; ++d) {
			for (int ifp = 0; ifp < (int) flight_plans_vector->size(); ++ifp) {
				if ((flight_plans_vector->at(ifp)->start >= d * 1440 && flight_plans_vector->at(ifp)->start < (d + 1) * 1440)
					|| (flight_plans_vector->at(ifp)->end >= d * 1440 && flight_plans_vector->at(ifp)->end < (d + 1) * 1440)) { // If ifp touches day d
					// printf("FP n. %d touches day %d (%d - %d)\n", ifp, d, flight_plans_vector->at(ifp)->start, flight_plans_vector->at(ifp)->end);
					temps[ip * DAYZ + d].push_back(PilotAssignments[ip * flight_plans_vector->size() + ifp]);
				}
				else {
					// printf("FP n. %d does not touch day %d (%d - %d)\n", ifp, d, flight_plans_vector->at(ifp)->start, flight_plans_vector->at(ifp)->end);
				}
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
	// IN STEP 2, MAYBE TRY NsCount() AND JUST DO BOOLEAN TYPE 0/1 FOR IF HE HAS ANY WORK THAT DAY, NEVER MIND 1, 2, OR 3 FPs.
	// THIS WILL HELP WITH STEP 3: USE NsSum(iw, plus_days_in_week). THIS NUMBER WILL BE 0-7 (SUM OF 7 0/1s), AND THAT'S IT.
	// Step 3:
	// This array has size (PILOTZ * ROLLING_WEEKS). It contains, for every pilot ip,
	// for every rolling week iw, how many days of that week are touched by a flight plan assigned to that pilot.
	naxos::NsIntVarArray PilotBusyDaysInRollingWeek;
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int iw = 0; iw < ROLLING_WEEKS; ++iw) {
			if (DAYZ >= 7)
				// The 2 lines below are equivalent, and require the corresponding line to be active in step 2
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * ROLLING_WEEKS + iw, 7));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 2] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 3] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 4] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 5] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 6] > 0));
			else if (DAYZ == 6)
				// The 2 lines below are equivalent, and require the corresponding line to be active in step 2
				// PilotBusyDaysInRollingWeek.push_back(naxos::NsSum(PilotHasWorkInThatDay, ip * ROLLING_WEEKS + iw, 6));
				PilotBusyDaysInRollingWeek.push_back((PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 1] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 2] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 3] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 4] > 0)
				                               + (PilotHasWorkInThatDay[ip * ROLLING_WEEKS + iw + 5] > 0));
			// This constraint only matters if there's 6 days or more.
			// Step 4:
			// Constraint: Every week must have <= 5 busy days.
			pm.add(PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS + iw] <= 5);
		}
	}
	///////////////////// DAYS OFF - END

	///////////////////////// FLYING TIME OPTIMIZATION - USES STEP 1 FROM ABOVE
	// Make another pseudo-2D array like PilotAssignments, with dimensions PILOTZ * FPs. Named PilotAssignmentsFlyingTime.
	// Each variable in that array will be PilotAssignments[ip][ifp] * FP_flying_time[ifp]
	// Then, the flying time of a pilot will be NsSum(PilotAssignmentsFlyingTime, ip*3304, 3304)
	naxos::NsIntVarArray PilotAssignmentsTimesFlyingTime; // 2D array in 1D format like in Parallel Systems
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int ifp = 0; ifp < (int) flight_plans_vector->size(); ++ifp) {
			PilotAssignmentsTimesFlyingTime.push_back(PilotAssignments[ip * flight_plans_vector->size() + ifp] * FP_flying_time[ifp]);
		}
	}
	naxos::NsIntVarArray PilotFlyingTime;
	for (int ip = 0; ip < PILOTZ; ++ip) {
		PilotFlyingTime.push_back(naxos::NsSum(PilotAssignmentsTimesFlyingTime, ip * flight_plans_vector->size(), flight_plans_vector->size()));
	}
	// Now calculate V = SUM( (PilotFlyingTime(i) - IFT) ^ 2 )
	naxos::NsIntVarArray PilotFlyingTimeMinusIFTSquared;
	for (int ip = 0; ip < PILOTZ; ++ip) {
		PilotFlyingTimeMinusIFTSquared.push_back((PilotFlyingTime[ip] - IFT) * (PilotFlyingTime[ip] - IFT));
	}
	naxos::NsIntVar V = naxos::NsSum(PilotFlyingTimeMinusIFTSquared, 0, PILOTZ);
	pm.minimize(V);
	///////////////////////// END OF FLYING TIME OPTIMIZATION
	pm.addGoal(new naxos::NsgLabeling(FP));

	// If 2 FPs have the same pilot, they cannot overlap.
	for (int i = 0; i < (int) flight_plans_vector->size(); ++i) { // With only this constraint, 100 FPs need 11 pilots.
		for (int j = 0; j < (int) flight_plans_vector->size(); ++j) {
			if (i == j)
				continue;
			else if (/* i starts before j ends */ FP_start[i].value() <= FP_end[j].value() && /* i starts after j starts*/ FP_start[i].value() >= FP_start[j].value())
				pm.add(FP[i]!=FP[j]);
			else if (/* i ends after j starts */ FP_end[i].value() >= FP_start[j].value() && /* i ends before j ends */ FP_end[i].value() <= FP_end[j].value())
				pm.add(FP[i]!=FP[j]);
		}
	}

	// If 2 FPs have the same pilot, they must be 660+ minutes apart.
	for (int i = 0; i < (int) flight_plans_vector->size(); ++i) { // With this constraint too, 100 FPs need 24 pilots.
		for (int j = 0; j < (int) flight_plans_vector->size(); ++j) {
			if (i == j)
				continue;
			else if (FP_start[i].value() >= FP_end[j].value() && FP_start[i].value() - FP_end[j].value() <= 660) // If i starts after j ends, and within 11 hours
				pm.add(FP[i]!=FP[j]);
			else if (FP_start[j].value() >= FP_end[i].value() && FP_start[j].value() - FP_end[i].value() <= 660) // If j starts after i ends, and within 11 hours
				pm.add(FP[i]!=FP[j]);
		}
	}

	int iterationz = 0;
	// pm.timeLimit(10);
	while (pm.nextSolution() != false) {
		printf("  --  SOLUTION FOUND, V = %ld  --\n", V.value());
		// for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
		// 		printf("Flight plan %d -> pilot %ld\n", i+1, FP[i].value());
		// }
		if (++iterationz >= 9)
			break;
	}
	// else
	// 	printf("\n  --  NO SOLUTION FOUND  --\n\n");

	printf("\n  --  PilotFlyingTime  --\n");
	for (int ip = 0; ip < PILOTZ; ++ip) {
		printf("Pilot %d has flying time %ld and FPs:", ip + 1, PilotFlyingTime[ip].value());
		for (int ifp = 0; ifp < (int) flight_plans_vector->size(); ++ifp) {
			if (PilotAssignments[ip * flight_plans_vector->size() + ifp].value() == 1)
				printf(" %d", ifp + 1);
			// DIAGNOSTIC FOR STEP 1
			if (PilotAssignments[ip * flight_plans_vector->size() + ifp].value() == true && FP[ifp].value() != ip+1)
				printf("MISTAKE\n"); // Boolean == 1, but FP doesn't have this pilot, so MISTAKE
			if (PilotAssignments[ip * flight_plans_vector->size() + ifp].value() == false && FP[ifp].value() == ip+1)
				printf("MISTAKE\n");
			// END OF DIAGNOSTIC FOR STEP 1
		}
		printf("\n");
	}
	for (int ip = 0; ip < PILOTZ; ++ip) {
		printf("Pilot %2d:", ip+1);
		for (int d = 0; d < DAYZ; ++d) {
			printf(" %ld", PilotHasWorkInThatDay[ip * DAYZ + d].value());
		}
		printf("\n");
	}
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int iw = 0; iw < ROLLING_WEEKS; ++iw) {
			printf("Pilot %d busy days in week %d: %ld\n", ip+1, iw+1, PilotBusyDaysInRollingWeek[ip * ROLLING_WEEKS + iw].value());
		}
	}
	printf("DAYZ: %d, rolling weeks: %d, V: %ld\n", DAYZ, ROLLING_WEEKS, V.value());
	/////////////////////////////////////////////////////////////////

	for (int i = 0; i < (int) flight_plans_vector->size(); ++i)
		delete (*flight_plans_vector)[i];
	delete flight_plans_vector;
	delete[] pairings_file;
	return 0;
}