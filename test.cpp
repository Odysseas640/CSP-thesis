#include <iostream>

// The entered year must be divisible by 4
// The entered year must be divisible by 400 but not by 100

int main() {
	for (int year = 1580; year < 2410; ++year) {
		if (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)) {
			printf("%d\n", year);
		}
		// if (year % 400 != 0 && (year % 4 != 0 || year % 100 == 0)) {
		// 	printf("%d\n", year);
		// }
		// else {
		// 	printf("MISTAKE\n");
		// }
	}
}

// int main() {
// 	int start_Year = 1580, end_Year = 2410;
// 	for (int i= start_Year; i< end_Year; i++){
// 		if (((i % 4 == 0) && (i % 100!= 0)) || (i % 400 == 0)){
// 			printf("%d\n", i);
// 		}
// 		else {
// 			// printf("%d\n", i);
// 		}
// 	}	
// 	return 0;
// }