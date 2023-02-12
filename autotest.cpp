#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <omp.h>
// make && ./autotest
// 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31

int main(int argc, char const *argv[]) {
	int exit_code = 0;
	char *startdate = new char[17];
	char *enddate = new char[17];
	strcpy(startdate, "2011-10-05/00:00");
	strcpy(enddate, "2012-03-01/23:59");
	int start_day = 28, end_day = 1;
	int start_month = 10, end_month = 11;
	int start_year = 2011, end_year = 2011;
	int i;
#pragma omp parallel num_threads(6)
	for (i = 1; i <= 11; ) {
		i++;
		sprintf(startdate + 8, "%02d%s", start_day, startdate + 10);
		sprintf(startdate + 5, "%02d%s", start_month, startdate + 7);
		sprintf(startdate + 0, "%04d%s", start_year, startdate + 4);
		sprintf(enddate + 8, "%02d%s", end_day, enddate + 10);
		sprintf(enddate + 5, "%02d%s", end_month, enddate + 7);
		sprintf(enddate + 0, "%04d%s", end_year, enddate + 4);
		if (fork() == 0)
			execl("./crewas", "./crewas", "-p", "Pairings.txt", "-n", "51", "-s", startdate, "-e", enddate, NULL);
		wait(&exit_code);
		printf("%s - %s - Exit code: %d\n", startdate, enddate, exit_code);
		if (exit_code != 0) {
			printf("ERROR\n");
			exit(1);
		}
		// if (fork() == 0)
		// 	execl("./crewas", "./crewas", "-i", "-p", "Pairings.txt", "-n", "51", "-s", startdate, "-e", enddate, "<", "test_assignmentz.txt", NULL);
		// wait(&exit_code);
		// printf("%s - %s - Exit code: %d\n", startdate, enddate, exit_code);
		// if (exit_code != 0) {
		// 	printf("ERROR\n");
		// 	exit(1);
		// }
#pragma omp critical
{
		start_day++;
		if (((start_month == 1 || start_month == 3 || start_month == 5 || start_month == 7 || start_month == 8 || start_month == 10 || start_month == 12) && start_day > 31)
			|| ((start_month == 4 || start_month == 6 || start_month == 9 || start_month == 11) && start_day > 30)
			|| (start_month == 2 && start_day > 28)) {
			start_day = 1;
			start_month++;
		}
		if (start_month > 12) {
			start_month = 1;
			start_year++;
		}

		end_day++;
		if (((end_month == 1 || end_month == 3 || end_month == 5 || end_month == 7 || end_month == 8 || end_month == 10 || end_month == 12) && end_day > 31)
			|| ((end_month == 4 || end_month == 6 || end_month == 9 || end_month == 11) && end_day > 30)
			|| (end_month == 2 && end_day > 28)) {
			end_day = 1;
			end_month++;
		}
		if (end_month > 12) {
			end_month = 1;
			end_year++;
		}
}
	};
	delete[] startdate;
	delete[] enddate;
	return 0;
}