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

typedef struct {
	int ID;
	// char startAirport[4]; // They're all ATH
	// char endAirport[4];
	DateTime startDateTime;
	DateTime endDateTime;
	int start; // Minutes from GLOBAL_START_DATE
	int end; // Minutes from GLOBAL_START_DATE
	int flying_time; // Minutes of actual flying time, sum from all individual flights
} flightPlan;

void print(flightPlan*);
void read_file_into_vector(std::vector<flightPlan*>*, char*, int);
void set_minutes_from_global_start_date(std::vector<flightPlan*>*, int, int, int);
float calculate_IFT(std::vector<flightPlan*>*, int);
int get_flight_duration(char*);
#endif