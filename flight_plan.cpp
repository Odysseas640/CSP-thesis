#include "flight_plan.hpp"

void print(flightPlan* f) {
	printf("%d %04d-%02d-%02d %02d:%02d %04d-%02d-%02d %02d:%02d\n",
		f->ID,
		f->startDateTime.year, f->startDateTime.month,
		f->startDateTime.day, f->startDateTime.hour, f->startDateTime.minute,
		f->endDateTime.year, f->endDateTime.month, f->endDateTime.day,
		f->endDateTime.hour, f->endDateTime.minute);
}

void read_file_into_vector(std::vector<flightPlan*>* flight_plans_vector, char* pairings_file, int FPs_to_read) {
	FILE* fp = fopen(pairings_file, "r");
	if (fp == NULL)
		exit(EXIT_FAILURE);

	char* line = NULL;
	size_t len = 0;
	// int linez = 0;

	int previous_line_ID = 0;
	flightPlan* FP_struct;
	int FPs_read = 0;
	while ((getline(&line, &len, fp)) != -1) {
		int ID = atoi(line);
		if (ID != previous_line_ID) { // New flight plan starts in this line
			FPs_read++;
			// printf("LINE\n");
			if (FPs_to_read > 0 && FPs_read > FPs_to_read) {
				// printf("break\n");
				break;
			}
			FP_struct = new flightPlan;
			flight_plans_vector->push_back(FP_struct);
			FP_struct->ID = ID;
			FP_struct->startDateTime.year = atoi(line + 17);
			FP_struct->startDateTime.month = atoi(line + 22);
			FP_struct->startDateTime.day = atoi(line + 25);
			FP_struct->startDateTime.hour = atoi(line + 28);
			FP_struct->startDateTime.minute = atoi(line + 31);
			previous_line_ID = ID;
			// Every flight plan has 2, 3 or 4 flights.
			// They all start and end in ATH.
		}
		else { // Same flight plan in this line. It may be the last flight, so save the data as if it was.
			// If it's not really the last flight, it will be overwritten by the next.
			FP_struct->endDateTime.year = atoi(line + 34);
			FP_struct->endDateTime.month = atoi(line + 39);
			FP_struct->endDateTime.day = atoi(line + 42);
			FP_struct->endDateTime.hour = atoi(line + 45);
			FP_struct->endDateTime.minute = atoi(line + 48);
		}
		// linez++;
		// if (linez >= 8)
		// 	break;
	}
	fclose(fp);
	if (line)
		free(line);
}

int get_minutes(const DateTime &FP_datetime, int day0, int month0, int year0) {
	int days = 0;
	int day = day0, month = month0, year = year0;
	while (day != FP_datetime.day || month != FP_datetime.month || year != FP_datetime.year) {
		days++;
		day++;
		if ((day > 29 && month == 2)
			|| (day > 30 && month == 11)
			|| (day > 31 && (month == 1 || month == 12))) {
			day = 1;
			month++;
		}
		if (month > 12) {
			month = 1;
			year++;
		}
	}
	int minutes_final = 1440 * days + 60 * FP_datetime.hour + FP_datetime.minute;
	printf("%d/%d/%d to %2d/%2d/%d %02d:%02d -- (%d days) %d minutes\n", day0, month0, year0, FP_datetime.day, FP_datetime.month, FP_datetime.year, FP_datetime.hour, FP_datetime.minute, days, minutes_final);
	return(minutes_final);
}

void set_minutes_from_global_start_date(std::vector<flightPlan*>* flight_plans_vector, int day0, int month0, int year0) {
	for (int i = 0; i < (int) flight_plans_vector->size(); ++i) {
		flight_plans_vector->at(i)->start = get_minutes(flight_plans_vector->at(i)->startDateTime, day0, month0, year0);
		flight_plans_vector->at(i)->end = get_minutes(flight_plans_vector->at(i)->endDateTime, day0, month0, year0);
	}
}