#include "flight_plan.hpp"

void fill_time_struct(DateTime& DT, const char* line) {
	DT.year = atoi(line);
	DT.month = atoi(line + 5);
	DT.day = atoi(line + 8);
	DT.hour = atoi(line + 11);
	DT.minute = atoi(line + 14);
}

void print(const flightPlan* f) {
	printf("ID %d - ", f->ID);
	print(f->startDateTime);
	printf(" - ");
	print(f->endDateTime);
	printf(" - ft %d\n", f->flying_time);
}

int read_file_into_vector(std::vector<flightPlan*>* FPvector, char* pairings_file, DateTime& starttime_dt, DateTime& endtime_dt) {
	// Every flight plan has 2, 3 or 4 flights.
	// They all start and end in ATH.
	FILE* fp = fopen(pairings_file, "r");
	if (fp == NULL)
		exit(EXIT_FAILURE);

	char* line = NULL;
	size_t len = 0;

	int previous_line_ID = 0;
	int previous_out_of_range_ID = 0;
	flightPlan* FP_struct;
	int FPs_read = 0;
	int in_range = 0, out_of_range = 0;
	DateTime current_line_dt;
	while ((getline(&line, &len, fp)) != -1) {
		int ID = atoi(line);
		if (ID != previous_line_ID && ID != previous_out_of_range_ID) { // New flight plan starts in this line
			fill_time_struct(current_line_dt, line + 17);
			if (FP_in_range(current_line_dt, starttime_dt, endtime_dt) == 0) {
				previous_out_of_range_ID = ID;
				out_of_range++;
				// printf("out of range: ID %d\n", ID);
				continue; // Skip this FP if it's not in range
			}
			in_range++;
			FP_struct = new flightPlan;
			FPvector->push_back(FP_struct);
			FP_struct->ID = ID;
			fill_time_struct(FP_struct->startDateTime, line + 17);
			previous_line_ID = ID;
			FP_struct->flying_time = get_flight_duration(line); // Get duration of the individual flight in this line
			FPs_read++;
		}
		else if (ID != previous_out_of_range_ID) { // Same flight plan in this line. It may be the last flight, so save the data as if it was.
			// If it's not really the last flight, it will be overwritten by the next.
			fill_time_struct(FP_struct->endDateTime, line + 34);
			FP_struct->flying_time += get_flight_duration(line); // Get duration of the individual flight in this line
			// printf("ID %d - %d+++++++++++++++++++++++++++\n", ID, FP_struct->flying_time);
		}
	}
	fclose(fp);
	if (line)
		free(line);
	printf("in range: %d, out of range: %d\n", in_range, out_of_range);
	return FPs_read;
}

int get_flight_duration(char* line) { // Get duration for an individual flight
	int minutes = 0;
	int day0 = atoi(line + 25);
	int hour0 = atoi(line + 28);
	int minute0 = atoi(line + 31);
	int day1 = atoi(line + 42);
	int hour1 = atoi(line + 45);
	int minute1 = atoi(line + 48);
	if (day0 != day1) {
		minutes += hour1 * 60 + minute1;
		minutes += (23 - hour0) * 60 + (60 - minute0);
	}
	else {
		minutes += (hour1 - hour0) * 60;
		minutes += (minute1 - minute0);
	}
	return minutes;
}

int get_minutes(char* starttime, const DateTime &FP_datetime) {
	// Count minutes from 0000/00/00 00:00 for both dates, convert to minutes, and subtract.
	int year0 = atoi(starttime);
	int month0 = atoi(starttime + 5);
	int day0 = atoi(starttime + 8);
	int year1 = FP_datetime.year;
	int month1 = FP_datetime.month;
	int day1 = FP_datetime.day;
	int monthDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	// initialize count using years and day
	long int n0 = year0 * 365 + day0;
	long int n1 = year1 * 365 + day1;

	// Add days for months in given date
	for (int i = 0; i < month0 - 1; i++)
		n0 += monthDays[i];
	for (int i = 0; i < month1 - 1; i++)
		n1 += monthDays[i];

	if (month0 <= 2)
		year0--;
	n0 += (year0 / 4 - year0 / 100 + year0 / 400);
	if (month1 <= 2)
		year1--;
	n1 += (year1 / 4 - year1 / 100 + year1 / 400);

	// return difference between two counts
	int dayzz = n1 - n0;
	int minutes_final = 1440 * dayzz + 60 * FP_datetime.hour + FP_datetime.minute;
	return (minutes_final);
}

void set_minutes_from_global_start_date(std::vector<flightPlan*>* FPvector, char* starttime) {
	for (int i = 0; i < (int) FPvector->size(); ++i) {
		FPvector->at(i)->start = get_minutes(starttime, FPvector->at(i)->startDateTime);
		FPvector->at(i)->end = get_minutes(starttime, FPvector->at(i)->endDateTime);
		if (FPvector->at(i)->start + 5 >= FPvector->at(i)->end) {
			printf("MISTAKE: FP start time and end time are wrong\n");
			exit(2);
		}
	}
}

float calculate_IFT(const std::vector<flightPlan*>* FPvector, int pilots) {
	// Ideal Flight Time is (the total actual flying time in all the flight plans), divided by (pilots)
	int total_flying_time = 0;
	for (int i = 0; i < (int) FPvector->size(); ++i) {
		total_flying_time += FPvector->at(i)->flying_time;
	}
	return((float) total_flying_time / (float) pilots);
}

void get_number_of_rolling_weeks(int& weeks, int& weeks_2, int days, int days_2) {
	// If it's 7 days or less, return 1 rolling week.
	// If it's 8 days or more, return (DAYS - 6)
	if (days >= 8)
		weeks = days - 6;
	else
		weeks = 1;
	if (days_2 >= 8)
		weeks_2 = days_2 - 6;
	else
		weeks_2 = 1;
}
int get_number_of_days(int& days_to_enddate, int& days_to_latest_flight_end, int& extra_days, std::vector<flightPlan*>* FPvector, char* starttime, char* endtime) {
	if (FPvector->size() == 0) {
		printf("ERROR: No flight plans have been read.\n");
		return -1;
	}
	////////////////////////////////////
	// New way: Number of days is from starttime until the last day there's a flight.
	DateTime latest_FP_end;
	fill_time_struct(latest_FP_end, "0001-01-01/01:01");
	for (int i = 0; i < (int) FPvector->size(); ++i) { // Find the latest date at which a flight plan ends.
		if (compare_dates(latest_FP_end, FPvector->at(i)->endDateTime) == 1) {
			latest_FP_end.year = FPvector->at(i)->endDateTime.year;
			latest_FP_end.month = FPvector->at(i)->endDateTime.month;
			latest_FP_end.day = FPvector->at(i)->endDateTime.day;
			latest_FP_end.hour = FPvector->at(i)->endDateTime.hour;
			latest_FP_end.minute = FPvector->at(i)->endDateTime.minute;
		}
	}
	int minutes_to_latest_flight_end = get_minutes(starttime, latest_FP_end);
	printf("Minutes from start date 00:00 to latest flight end: %d\n", minutes_to_latest_flight_end);
	days_to_latest_flight_end = minutes_to_latest_flight_end / 1440;
	if (minutes_to_latest_flight_end % 1440 > 0)
		days_to_latest_flight_end++;
	////////////////////////////////////
	// Create a DateTime struct from endtime, and use get_minutes function.
	DateTime endtime_dt;
	fill_time_struct(endtime_dt, endtime);
	int minutes_to_enddate = get_minutes(starttime, endtime_dt);
	printf("Minutes from start date 00:00 to enddate: %d\n", minutes_to_enddate);
	days_to_enddate = minutes_to_enddate / 1440;
	if (minutes_to_enddate % 1440 > 0)
		days_to_enddate++;
	extra_days = days_to_latest_flight_end - days_to_enddate;
	return 0;
}

int FP_touches_day(flightPlan* FP, int d) {
	if ((FP->start >= d * 1440 && FP->end <= (d+1) * 1440)
	|| (FP->start <= d * 1440 && FP->end > d * 1440)
	|| (FP->start < (d+1) * 1440 && FP->end >= (d+1) * 1440)
	|| (FP->start <= d * 1440 && FP->end >= (d+1) * 1440))
		return(1);
	else
		return(0);
}

int compare_dates(const DateTime &d1, const DateTime &d2) {
	// -1 means d1 is later
	// 0 means equal
	// 1 means d2 is later
	if (d1.year > d2.year)
		return(-1);
	else if (d1.year < d2.year)
		return(1);
	else /*if (d1.year == d2.year)*/ {
		if (d1.month > d2.month)
			return(-1);
		else if (d1.month < d2.month)
			return(1);
		else /*if (d1.month == d2.month)*/ {
			if (d1.day > d2.day)
				return(-1);
			else if (d1.day < d2.day)
				return(1);
			else /*if (d1.day == d2.day)*/ {
				if (d1.hour > d2.hour)
					return(-1);
				else if (d1.hour < d2.hour)
					return(1);
				else /*if (d1.hour == d2.hour)*/ {
					if (d1.minute > d2.minute)
						return(-1);
					else if (d1.minute < d2.minute)
						return(1);
					else /*if (d1.minute == d2.minute)*/ {
						return(0);
					}
				}
			}
		}
	}
}

int FP_in_range(const DateTime& dt, const DateTime& starttime_dt, const DateTime& endtime_dt) {
	if (compare_dates(dt, starttime_dt) <= 0 && compare_dates(dt, endtime_dt) >= 0)
		return 1;
	else
		return 0;
}

void print(const DateTime& DT) {
	printf("%04d-%02d-%02d %02d:%02d", DT.year, DT.month, DT.day, DT.hour, DT.minute);
}