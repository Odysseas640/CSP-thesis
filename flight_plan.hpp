#ifndef __ODYS_FLIGHT_PLAN__
#define __ODYS_FLIGHT_PLAN__
#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include <vector>
#include <time.h>

typedef struct {
	int year;
	char month;
	char day;
	char hour;
	char minute;
} DateTime;

// typedef struct {
// 	int sequence; // 1st flight, 2nd, 3rd, 4th.
// 	DateTime startDateTime;
// 	DateTime endDateTime;
// } Flight;

typedef struct {
	int ID;
	// std::vector<Flight*>* flights;
	DateTime startDateTime;
	DateTime endDateTime;
	int start; // Minutes from date of STARTTIME, discounting the time of day
	int end; // Minutes from date of ENDTIME, discounting the time of day
	int flying_time; // Minutes of actual flying time, sum from all individual flights
} flightPlan;

void fill_time_struct(DateTime&, const char*);
void print(const flightPlan*);
int read_file_into_vector(std::vector<flightPlan*>*, char*, DateTime&, DateTime&);
void set_minutes_from_global_start_date(std::vector<flightPlan*>*, char*);
float calculate_IFT(const std::vector<flightPlan*>*, int);
int get_flight_duration(char*);
void get_number_of_rolling_weeks(int&, int&, int, int);
int get_number_of_days(int&, int&, int&, std::vector<flightPlan*>*, char*, char*);
int FP_touches_day(flightPlan*, int);
int compare_dates(const DateTime&, const DateTime&);
// int compare_dates(const char*, const char*);
int FP_in_range(const DateTime&, const DateTime&, const DateTime&);
void print(const DateTime&);
#endif