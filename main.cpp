#include <iostream>
#include <cstring>
#include "naxos-2.0.5/core/naxos.h"
#include "flight_plan.hpp"
#define FLIGHT_PLANS_TO_READ 20 // -1 means ALL
#define PILOTZ 10 // Pilots count from 1, not 0

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
	int rolling_weeks = get_number_of_rolling_weeks(flight_plans_vector);

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
	// (so that day isn't a day off). Usually from 0 to 3.
	/*naxos::NsIntVarArray PilotHasWorkInThatDay; // Size PILOTZ * DAYS
	// naxos::NsIntVarArray temp;
	int previous_size = 0, current_size = 0;
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int d = 0; d < DAYZ; ++d) {
			// PilotHasWorkInThatDay.push_back(naxos::NsIntVar(pm, 0, 4)); // Can't be more than 3, that would mean there's a mistake.
			PilotHasWorkInThatDay.push_back(naxos::NsIntVar(pm, 0, 1));
			// PilotHasWorkInThatDay[ip * DAYZ + d].set(1);
			// previous_size = temp.size();
			for (int ifp = 0; ifp < (int) flight_plans_vector->size(); ++ifp) {
				// Maybe use a temp NsIntVarArray temp, and when this "if" is true,
				// append PilotAssignments[ip * flight_plans_vector->size() + ifp].
				// Then do PilotHasWorkInThatDay[ip * DAYZ + d] = NsSum(temp, 0, temp->size())
				// Cannot delete "temp" array: just save an index to where we left it in the previous iteration
				// and continue from there: NsSum(temp, previous_iteration_stopped_here-size_of_array_at_that_time, temp->size())
				// OTHER IDEA: when this "if" is true, add NsIfThen(PilotAssignments[ip * flight_plans_vector->size() + ifp] == 1, PilotHasWorkInThatDay[ip * DAYZ + d] == 1)
				// The array PilotHasWorkInThatDay must be instantiated before, with PilotHasWorkInThatDay.push_back(naxos::NsIntVar(pm, 0, 1));
				// This idea will make sure that PilotHasWorkInThatDay is 1 when it's supposed to be, but it might be 1 when it should be 0.
				// To fix that, minimize the whole array as a pm goal, so it'll be 0 when no constraint NsIfThen forces 1.
				// Also test without the minimize goal, to see if the unconstrained variables are 0 or 1.
				if ((flight_plans_vector->at(ifp)->start > d * 1440 && flight_plans_vector->at(ifp)->start < (d + 1) * 1440)
					|| (flight_plans_vector->at(ifp)->end > d * 1440 && flight_plans_vector->at(ifp)->end < (d + 1) * 1440)) { // If ifp touches day d
					// PilotHasWorkInThatDay[ip * DAYZ + d] += PilotAssignments[ip * flight_plans_vector->size() + ifp];
					// temp.push_back(PilotAssignments[ip * flight_plans_vector->size() + ifp]);
					pm.add(naxos::NsIfThen(PilotAssignments[ip * flight_plans_vector->size() + ifp] == 1, PilotHasWorkInThatDay[ip * DAYZ + d] == 1));
				}
			}
			// current_size = temp.size();
			// printf("previous_size: %d, current_size: %d\n", previous_size, current_size);
			// if (current_size > previous_size)
			// 	PilotHasWorkInThatDay.push_back(naxos::NsSum(temp, previous_size, current_size - previous_size));
			// else { // No flight touches this day, whether pilot has it or not
			// 	PilotHasWorkInThatDay.push_back(naxos::NsIntVar(pm, 0, 0));
			// 	PilotHasWorkInThatDay[ip * DAYZ + d].set(0);
			// }
		}
	}
	naxos::NsIntVar PilotHasWorkInThatDay_minimize = naxos::NsSum(PilotHasWorkInThatDay, 0, PilotHasWorkInThatDay.size());*/
	// IN STEP 2, MAYBE TRY NsCount() AND JUST DO BOOLEAN TYPE 0/1 FOR IF HE HAS ANY WORK THAT DAY, NEVER MIND 1, 2, OR 3 FPs.
	// THIS WILL HELP WITH STEP 3: USE NsSum(iw, plus_days_in_week). THIS NUMBER WILL BE 0-7 (SUM OF 7 0/1s), AND THAT'S IT.
	// Step 3:
	// This array (size PILOTZ * rolling_weeks) contains, for every pilot ip,
	// for every rolling week iw, how many days of that week are touched by a flight plan assigned to that pilot.
	/*naxos::NsIntVarArray PilotBusyDaysInRollingWeek;
	int plus_days_in_week = 
	for (int ip = 0; ip < PILOTZ; ++ip) {
		for (int iw = 0; iw < rolling_weeks; ++iw) {
			PilotBusyDaysInRollingWeek.push_back(naxos::NsIntVar(pm, 0, 7));
			PilotBusyDaysInRollingWeek = (PilotHasWorkInThatDay[ip * rolling_weeks + iw] > 0)
			                           + (PilotHasWorkInThatDay[ip * rolling_weeks + iw + 1] > 0)
			                           + (PilotHasWorkInThatDay[ip * rolling_weeks + iw + 2] > 0)
			                           + (PilotHasWorkInThatDay[ip * rolling_weeks + iw + 3] > 0)
			                           + (PilotHasWorkInThatDay[ip * rolling_weeks + iw + 4] > 0)
			                           + (PilotHasWorkInThatDay[ip * rolling_weeks + iw + 5] > 0)
			                           + (PilotHasWorkInThatDay[ip * rolling_weeks + iw + 6] > 0);
			// Step 4:
			// Constraints: Every week must have <= 5 busy days.
			pm.add(PilotBusyDaysInRollingWeek[ip * rolling_weeks + iw] <= 5);
		}
	}*/
	// Pilot_busy_days_in_rolling_week[pi*rolling_weeks + i_week] = sum of PilotHasWorkInThatDay's
	//                                 that have a value of >= 1
	//                   Like: Pilot_busy_days_in_rolling_week = (PilotHasWorkInThatDay>=1) + (PilotHasWorkInThatDay>=1) +... 7 times, cause it's a week.
	// Step 4:
	// Constraint: All Pilot_busy_days_in_rolling_week should be <= 5
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
	// naxos::NsIntVar minimaize = V + PilotHasWorkInThatDay_minimize;
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
	// int boundd = 0;
	// int unboundd = 0;
	// for (int ip = 0; ip < PILOTZ; ++ip) {
	// 	for (int d = 0; d < DAYZ; ++d) {
	// 		if (PilotHasWorkInThatDay[ip * DAYZ + d].isBound())
	// 			boundd++;
	// 		else
	// 			unboundd++;
	// 	}
	// }
	// printf("bound: %d, unbound: %d\n", boundd, unboundd);
	// printf("DAYZ: %d, rolling weeks: %d, V: %ld, minimaize: %ld\n", DAYZ, rolling_weeks, V.value(), minimaize.value());
	/////////////////////////////////////////////////////////////////

	for (int i = 0; i < (int) flight_plans_vector->size(); ++i)
		delete (*flight_plans_vector)[i];
	delete flight_plans_vector;
	delete[] pairings_file;
	return 0;
}