#include <iostream>
#include <cstring>
#include "naxos-2.0.5/core/naxos.h"
#include "flight_plan.hpp"

int main(int argc, char const *argv[]) {
	char *pairings_file = new char[256];
	memset(pairings_file, '\0', 256);
	strcpy(pairings_file, "./Pairings.txt");
	// Read all flight plans and put them in a vector
	std::vector<flightPlan*> *flight_plans_vector = new std::vector<flightPlan*>;
	read_file_into_vector(flight_plans_vector, pairings_file, -1);

	// Store flight plan start/end datetimes as minutes from START_DATE (2011/11/1)
	set_minutes_from_global_start_date(flight_plans_vector, 1, 11, 2011);

	for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
		print((*flight_plans_vector)[i]);
	}

	/////////////////////////////////////////////////////////////////
	// Every flight plan has only one pilot.
	// So, one variable for every flight plan, and it gets an Int value
	// that says who's the pilot.
	// There are 3304 flight plans. Try 3304 pilots and decrease.
	// int pilots = flight_plans_vector->size();
	int pilots = 26;
	naxos::NsProblemManager pm;
	naxos::NsIntVarArray FP;
	naxos::NsIntVarArray FP_start, FP_end;
	for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
		FP.push_back(naxos::NsIntVar(pm, 1, pilots));
		FP_start.push_back(naxos::NsIntVar(pm, 1, 180000)); // 4 months = 172800 minutes + safe margin
		FP_start[i].set(flight_plans_vector->at(i)->start);
		FP_end.push_back(naxos::NsIntVar(pm, 1, 180000));
		FP_end[i].set(flight_plans_vector->at(i)->end);
	}
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

	if (pm.nextSolution() != false) {
		printf("\n  --  SOLUTION  --\n");
		for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
				printf("Flight plan %d -> pilot %ld\n", i+1, FP[i].value());
		}
	}
	else
		printf("\n  --  NO SOLUTION  --\n\n");
	/////////////////////////////////////////////////////////////////

	for (int i = 0; i < (int) flight_plans_vector->size(); ++i)
		delete (*flight_plans_vector)[i];
	delete flight_plans_vector;
	delete[] pairings_file;
	return 0;
}
