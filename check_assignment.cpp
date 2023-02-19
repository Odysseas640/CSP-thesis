#include "check_assignment.hpp"

bool sort_by_startDateTime(const flightPlan* fp1, const flightPlan* fp2) {
	if (compare_dates(fp1->startDateTime, fp2->startDateTime) > 0)
		return true;
	else
		return false;
}

int check_assignment(const std::vector<flightPlan*>* FPvector, const DateTime& starttime_dt, const DateTime& endtime_dt) {
	printf("-----  CHECKING ASSIGNMENT\n");
	// A vector of maps. Every map contains the flight plan IDs that have been assigned to one pilot.
	// That's the key. The value will be a pointer to a flightPlan struct, found from FPvector vector.
	std::vector<std::map<int, flightPlan*>*> vector_of_maps;
	int lines_read = 0;
	char* line = NULL;
	size_t len = 0;
	while ((getline(&line, &len, stdin)) != -1) {
		vector_of_maps.push_back(new std::map<int, flightPlan*>);
		if (line[0] == '\n') { // No flight plans in this line, pilot has none.
			lines_read++;
			continue;
		}
		for (int i = 0; i < (int) strlen(line); i=i+5) { // Flight plan IDs are 5 characters apart.
			int FP_ID = atoi(line + i);
			// Put this FP_ID in vector_of_maps.at(i) as a key, and find it in FPvector to put it as a value.
			// Find the flight plan with FP_ID in FPvector
			for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
				if (FP_ID == FPvector->at(ifp)->ID) {
					(*vector_of_maps.at(lines_read))[FP_ID] = FPvector->at(ifp);
					break;
				}
				if (ifp == (int) FPvector->size() - 1) {
					printf("FP not found: %d\n", FP_ID);
					printf("ERROR: A flight plan in the assignments doesn't exist in the pairings file.\n");
					exit(1);
				}
			}
		}
		lines_read++;
	}
	free(line);

	// For every flight plan in FPvector that's between starttime and endtime, find it in vector_of_maps. It should exist.
	// The other way around has been done while parsing the assignment from stdin.
	for (int ifp = 0; ifp < (int) FPvector->size(); ++ifp) {
		int ifp_found = 0;
		if (FP_in_range(FPvector->at(ifp)->startDateTime, starttime_dt, endtime_dt) == 1) { // Look for this FP in assignments map
			std::map<int, flightPlan*>::iterator i_found;
			for (int im = 0; im < (int) vector_of_maps.size(); ++im) { // If ifp exists in map im, continue.
				i_found = vector_of_maps.at(im)->find(FPvector->at(ifp)->ID);
				if (i_found != vector_of_maps.at(im)->end()) {
					ifp_found = 1;
					break;;
				}
			}
			if (ifp_found == 1)
				continue;
			printf("ERROR: flight plan %d in [starttime,endtime] has not been assigned.\n", FPvector->at(ifp)->ID);
			exit(1);
		}
	}

	// Calculate V for assignments
	// float IFT_float = calculate_IFT(FPvector, lines_read);
	// printf("IFT_float: %f\n", IFT_float);
	// The correct type for IFT would be "float", but Naxos Solver can only use "int" when calculating assignments,
	// so if I used "float" here, the number would be different and it would look like there's a bug.
	int IFT = calculate_IFT(FPvector, lines_read);
	float V = 0;
	for (int im = 0; im < (int) vector_of_maps.size(); ++im) {
		// Calculate flying time for this pilot
		// Subtract it from IFT and square it
		int pilot_flying_time = 0;
		std::map<int, flightPlan*>::iterator im2;
		for (im2 = vector_of_maps.at(im)->begin(); im2 != vector_of_maps.at(im)->end(); ++im2) {
			pilot_flying_time += im2->second->flying_time;
		}
		int PilotFlyingTimeMinusIFTSquared = (IFT - pilot_flying_time) * (IFT - pilot_flying_time);
		V += PilotFlyingTimeMinusIFTSquared;
	}

	// Make a vector_of_vectors and copy maps to vectors, and then sort the vectors by start_date with a custom comparison function.
	std::vector<std::vector<flightPlan*>*>* vector_of_vectors = new std::vector<std::vector<flightPlan*>*>;
	for (int i = 0; i < (int) vector_of_maps.size(); ++i) {
		vector_of_vectors->push_back(new std::vector<flightPlan*>);
		std::map<int, flightPlan*>::iterator j;
		for (j = vector_of_maps.at(i)->begin(); j != vector_of_maps.at(i)->end(); ++j) {
			vector_of_vectors->at(i)->push_back(j->second);
		}
		sort(vector_of_vectors->at(i)->begin(), vector_of_vectors->at(i)->end(), sort_by_startDateTime); // Sort this pilot's vector by startDateTime
		// Now FPs in each vector should be sorted by startDateTime.
	}

	check_assignments_11h_gap(vector_of_vectors);
	check_assignments_rolling_weeks(vector_of_vectors);

	printf("-----  ASSIGNMENT OK, %ld pilots, V: %f\n", vector_of_vectors->size(), V);
	/////////// Free stuff
	for (int i = 0; i < (int) vector_of_maps.size(); ++i)
		delete vector_of_maps.at(i);
	for (int i = 0; i < (int) vector_of_vectors->size(); ++i)
		delete vector_of_vectors->at(i);
	delete vector_of_vectors;
	return 0;
}

int check_assignments_11h_gap(const std::vector<std::vector<flightPlan*>*>* vector_of_vectors) {
	for (int i = 0; i < (int) vector_of_vectors->size(); ++i) {
		// Check if this pilot's flight plans overlap
		for (int j = 0; j < (int) vector_of_vectors->at(i)->size() - 1; ++j) {
			// Check if this FP and the next FP overlap
			if (compare_dates(vector_of_vectors->at(i)->at(j)->endDateTime, vector_of_vectors->at(i)->at(j + 1)->startDateTime) <= 0) {
				printf("ERROR: 2 assignments overlap.\n");
				exit(1);
			}
			// Check if this FP and the next FP are >=660 minutes apart
			if (vector_of_vectors->at(i)->at(j + 1)->start - vector_of_vectors->at(i)->at(j)->end < 660) {
				printf("ERROR: 2 assignments don't have a gap of 11 hours.\n");
				exit(1);
			}
		}
	}
	return 0;
}

int check_assignments_rolling_weeks(const std::vector<std::vector<flightPlan*>*>* vector_of_vectors) {
	char* busy_days_in_rolling_week = new char[7];
	for (int ip = 0; ip < (int) vector_of_vectors->size(); ++ip) {
		if (vector_of_vectors->at(ip)->size() == 0) // Some pilots may not have any FPs
			continue;
		// Look at this pilot's first and last FP->start,end (minutes), and get first and last day.
		// Convert that into rolling weeks. For every rolling week, look at all FPs to see if >5 days are touched.
		int first_flight_start_time = vector_of_vectors->at(ip)->front()->start;
		int last_flight_end_time = vector_of_vectors->at(ip)->back()->end;
		int first_day = first_flight_start_time / 1440;
		int last_day = last_flight_end_time / 1440;
		if (last_day - first_day <= 5) // "Days off" constraint doesn't matter if there's only 5 days.
			continue;
		int days_6_or_7 = 7; // This pilot might have 6 days of work, so then we have one rolling week with 6 days.
		if (last_day - first_day == 6)
			days_6_or_7 = 6;
		for (int d = first_day; d + days_6_or_7 <= last_day; ++d) { // For every rolling week
			// Rolling week number d starts at day d.
			for (int i7 = 0; i7 < days_6_or_7; ++i7)
				busy_days_in_rolling_week[i7] = 0;
			for (int dw = 0; dw < days_6_or_7; ++dw) { // For every day in that rolling week
				for (int j = 0; j < (int) vector_of_vectors->at(ip)->size(); ++j) {
					if (FP_touches_day(vector_of_vectors->at(ip)->at(j), d + dw) == 1)
						busy_days_in_rolling_week[dw] = 1;
				}
			}
			int busy_days = 0;
			for (int i7 = 0; i7 < days_6_or_7; ++i7)
				busy_days += busy_days_in_rolling_week[i7];
			if (busy_days > 5) {
				printf("ERROR: Pilot %d has >5 busy days.\n", ip);
				delete[] busy_days_in_rolling_week;
				exit(1);
			}
		}
	}
	delete[] busy_days_in_rolling_week;
	return 0;
}