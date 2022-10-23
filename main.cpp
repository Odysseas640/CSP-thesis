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
	read_file_into_vector(flight_plans_vector, pairings_file, 200);

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
	int pilots = 200;
	naxos::NsProblemManager pm;
	naxos::NsIntVarArray FP_var_array;
	naxos::NsIntVarArray FP_start_time_array, FP_end_time_array;
	for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
		FP_var_array.push_back(naxos::NsIntVar(pm, 1, pilots));
		FP_start_time_array.push_back(naxos::NsIntVar(pm, 1, 123456));
		FP_start_time_array[i].set(flight_plans_vector->at(i)->start);
		FP_end_time_array.push_back(naxos::NsIntVar(pm, 1, 123456));
		FP_end_time_array[i].set(flight_plans_vector->at(i)->end);
	}
	pm.addGoal(new naxos::NsgLabeling(FP_var_array));
	// If 2 FPs have the same pilot, they cannot overlap. HOW TO DO THIS MORE EFFICIENTLY
	for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
		for (int j = 0; j < (int) flight_plans_vector->size(); ++j) {
			if (i == j)
				continue;
			pm.add(naxos::NsIfThen((/* i starts before j ends */ FP_start_time_array[i] <= FP_end_time_array[j] && /* i starts after j starts*/ FP_start_time_array[i] >= FP_start_time_array[j])
			|| (/* i ends after j starts */ FP_end_time_array[i] >= FP_start_time_array[j] && /* i ends before j ends */ FP_end_time_array[i] <= FP_end_time_array[j]),
			/*CANT HAVE SAME PILOT*/ FP_var_array[i]!=FP_var_array[j]));
		}
	}

	// If 2 FPs have the same pilot, they must be 660+ minutes apart.
	naxos::NsIntVar minutes660(pm, 660, 660);
	minutes660.set(660);
	for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
		for (int j = 0; j < (int) flight_plans_vector->size(); ++j) {
			if (i == j)
				continue;
			pm.add(naxos::NsIfThen(/* i ends within 11 hours of j starting */ FP_start_time_array[j] - FP_end_time_array[i] < 660
			|| /* i starts within 11 hours of j ending */ FP_end_time_array[i] - FP_end_time_array[j] < 660,
			/*CANT HAVE SAME PILOT*/ FP_var_array[i]!=FP_var_array[j]));
		}
	}

	// int solution_n = 0;
	if (pm.nextSolution() != false) {
		printf("\n  --  SOLUTION  --\n");
		// solution_n++;
		for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
			// if (solution_n == 1)
				printf("Flight plan %d -> pilot %ld\n", i+1, FP_var_array[i].value());
		}
	}
	else
		printf("\n  --  NO SOLUTION  --\n\n");
	// printf("Iterations (legal solutions): %d\n", solution_n);
	/////////////////////////////////////////////////////////////////

	for (int i = 0; i < (int) flight_plans_vector->size(); ++i)
		delete (*flight_plans_vector)[i];
	delete flight_plans_vector;
	delete[] pairings_file;
	return 0;
}
