#include "print_and_copy_results.hpp"

void copy_to_final_assignments(std::vector< std::vector<int> * > *&PilotAssignments_Final_Result, const std::vector<flightPlan*>* FPvector, const naxos::NsIntVarArray& PilotAssignments, int pilotz) {
	// First time this function is called, allocate vectors
	if (PilotAssignments_Final_Result == NULL) {
		PilotAssignments_Final_Result = new std::vector< std::vector<int> * >;
		for (int i = 0; i < pilotz; ++i)
			PilotAssignments_Final_Result->push_back(new std::vector<int>);
	}
	else
		for (int i = 0; i < pilotz; ++i)
			PilotAssignments_Final_Result->at(i)->clear();

	for (int ip = 0; ip < pilotz; ++ip) {
		for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
			if (PilotAssignments[ip * ((int) FPvector->size()) + ifp].value() == 1)
				PilotAssignments_Final_Result->at(ip)->push_back(FPvector->at(ifp)->ID);
		}
	}
}

void copy_to_final_flying_time(std::vector<int> *&PilotFlyingTime_Final_Result, const naxos::NsIntVarArray& PilotFlyingTime, int pilotz) {
	if (PilotFlyingTime_Final_Result == NULL)
		PilotFlyingTime_Final_Result = new std::vector<int>;
	else
		PilotFlyingTime_Final_Result->clear();

	for (int ip = 0; ip < PilotFlyingTime.size(); ++ip) {
		PilotFlyingTime_Final_Result->push_back(PilotFlyingTime[ip].value());
	}
}

void copy_to_final_assignments(std::vector< std::vector<int> * > *&PilotHasWorkInThatDay_Final_Result, const naxos::NsIntVarArray& PilotHasWorkInThatDay, int pilotz, int days2) {
	if (PilotHasWorkInThatDay.size() < 1)
		return;
	// First time this function is called, allocate vectors
	if (PilotHasWorkInThatDay_Final_Result == NULL) {
		PilotHasWorkInThatDay_Final_Result = new std::vector< std::vector<int> * >;
		for (int ip = 0; ip < pilotz; ++ip) {
			PilotHasWorkInThatDay_Final_Result->push_back(new std::vector<int>);
			for (int id = 0; id < days2; ++id) {
				PilotHasWorkInThatDay_Final_Result->at(ip)->push_back(0);
			}
		}
	}

	for (int ip = 0; ip < pilotz; ++ip) {
		for (int id = 0; id < days2; ++id) {
			PilotHasWorkInThatDay_Final_Result->at(ip)->at(id) = PilotHasWorkInThatDay[ip * days2 + id].value();
		}
	}
}

void print_final_assignments(const std::vector< std::vector<int> * > *PilotAssignments_Final_Result) {
	if (PilotAssignments_Final_Result == NULL) {
		printf("No solution found within time limit.\n");
		return;
	}
	for (int ip = 0; ip < (int) PilotAssignments_Final_Result->size(); ++ip) {
		for (int ifp = 0; ifp < (int) PilotAssignments_Final_Result->at(ip)->size(); ++ifp) {
			if (ifp == 0)
				printf("%04d", PilotAssignments_Final_Result->at(ip)->at(ifp));
			else
				printf(" %04d", PilotAssignments_Final_Result->at(ip)->at(ifp));
		}
		printf("\n");
	}
}

void print_final_assignments_to_file(const std::vector< std::vector<int> * > *PilotAssignments_Final_Result) {
	if (PilotAssignments_Final_Result == NULL) {
		printf("No solution found within time limit.\n");
		return;
	}
	FILE* fp = fopen("test_assignmentz.txt", "w");
	if (fp == NULL)
		exit(EXIT_FAILURE);
	for (int ip = 0; ip < (int) PilotAssignments_Final_Result->size(); ++ip) {
		for (int ifp = 0; ifp < (int) PilotAssignments_Final_Result->at(ip)->size(); ++ifp) {
			if (ifp == 0)
				fprintf(fp, "%04d", PilotAssignments_Final_Result->at(ip)->at(ifp));
			else
				fprintf(fp, " %04d", PilotAssignments_Final_Result->at(ip)->at(ifp));
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
}

void print_pilot_FPs(std::vector< std::vector<int> * > *&PilotAssignments_Final_Result, std::vector<int> *PilotFlyingTime_Final_Result) {
	printf("\n  --  PilotFlyingTime and FPs  --\n");
	for (int ip = 0; ip < (int) PilotAssignments_Final_Result->size(); ++ip) {
		printf("Pilot %d has flying time %d and FPs:", ip + 1, PilotFlyingTime_Final_Result->at(ip));
		for (int ifp = 0; ifp < (int) PilotAssignments_Final_Result->at(ip)->size(); ++ifp)
			printf(" %d", PilotAssignments_Final_Result->at(ip)->at(ifp));
		printf("\n");
	}
}

void print_pilot_days(std::vector< std::vector<int> * > *&PilotHasWorkInThatDay_Final_Result, int nPilots, int days2, int extra_days) {
	printf("  --  PilotHasWorkInThatDay  --\n");
	for (int ip = 0; ip < nPilots; ++ip) {
		printf("Pilot %2d days:", ip+1);
		for (int d = 0; d < days2; ++d) {
			if (extra_days > 0 && d == days2 - extra_days)
				printf(" |");
			printf(" %d", PilotHasWorkInThatDay_Final_Result->at(ip)->at(d));
		}
		printf("\n");
	}
}

void print_pilot_weeks(std::vector< std::vector<int> * > *&PilotBusyDaysInRollingWeek_Final_Result, int nPilots, int rolling_weeks, int extra_days) {
	printf("  --  PilotBusyDaysInRollingWeek  --\n");
	for (int ip = 0; ip < nPilots; ++ip) {
		printf("Pilot %d rolling weeks:", ip+1);
		for (int iw = 0; iw < rolling_weeks; ++iw) {
			if (extra_days > 0 && iw == rolling_weeks - extra_days)
				printf(" |");
			printf(" %d", PilotBusyDaysInRollingWeek_Final_Result->at(ip)->at(iw));
		}
		printf("\n");
	}
}

int read_arguments(char argc, const char* argv[], int& nPilots, char*& pairings_file, char*& starttime, char*& endtime, int& check_assignment_i, int& naxos_time_limit_seconds, int& iterations_limit, int& debug_print) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i],"-n") == 0) {
			for (int j = 0; j < (int) strlen(argv[i+1]); ++j) {
				if (argv[i+1][j] > '9' || argv[i+1][j] < '0') {
					printf("Number of pilots is not a number. Terminating.\n");
					exit(1);
				}
			}
			nPilots = atoi(argv[i+1]);
		}
		else if (strcmp(argv[i],"-p") == 0) {
			pairings_file = new char[strlen(argv[i+1]) + 1];
			strcpy(pairings_file, argv[i+1]);
		}
		else if (strcmp(argv[i],"-s") == 0) {
			starttime = new char[17];
			strcpy(starttime, argv[i+1]);
		}
		else if (strcmp(argv[i],"-e") == 0) {
			endtime = new char[17];
			strcpy(endtime, argv[i+1]);
		}
		else if (strcmp(argv[i],"-i") == 0)
			check_assignment_i = 1;
		else if (strcmp(argv[i],"-t") == 0)
			naxos_time_limit_seconds = atoi(argv[i+1]);
		else if (strcmp(argv[i],"-it") == 0)
			iterations_limit = atoi(argv[i+1]);
		else if (strcmp(argv[i],"-d") == 0)
			debug_print = 1;
	}
	if (nPilots < 0) {
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