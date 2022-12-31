CC = g++ -std=c++11
CFLAGS = -Wall -ggdb3

crewas: main.o check_assignment.o check_assignment.hpp flight_plan.o flight_plan.hpp problemmanager.o array_constraints.o bitset_domain.o expressions.o intvar.o var_constraints.o non_mini_solver_expressions.o non_mini_solver_constraints.o
	$(CC) -o crewas main.o check_assignment.o flight_plan.o problemmanager.o array_constraints.o bitset_domain.o expressions.o intvar.o var_constraints.o non_mini_solver_expressions.o non_mini_solver_constraints.o

main.o: main.cpp flight_plan.cpp flight_plan.hpp
	$(CC) $(CFLAGS) -c main.cpp

flight_plan.o: flight_plan.cpp flight_plan.hpp
	$(CC) $(CFLAGS) -c flight_plan.cpp

check_assignment.o: check_assignment.cpp check_assignment.hpp
	$(CC) $(CFLAGS) -c check_assignment.cpp

array_constraints.o: naxos-2.0.5/core/array_constraints.cpp
	$(CC) $(CFLAGS) -c naxos-2.0.5/core/array_constraints.cpp

bitset_domain.o: naxos-2.0.5/core/bitset_domain.cpp
	$(CC) $(CFLAGS) -c naxos-2.0.5/core/bitset_domain.cpp

expressions.o: naxos-2.0.5/core/expressions.cpp
	$(CC) $(CFLAGS) -c naxos-2.0.5/core/expressions.cpp

intvar.o: naxos-2.0.5/core/intvar.cpp
	$(CC) $(CFLAGS) -c naxos-2.0.5/core/intvar.cpp

var_constraints.o: naxos-2.0.5/core/var_constraints.cpp
	$(CC) $(CFLAGS) -c naxos-2.0.5/core/var_constraints.cpp

non_mini_solver_expressions.o: naxos-2.0.5/core/non_mini_solver_expressions.cpp
	$(CC) $(CFLAGS) -c naxos-2.0.5/core/non_mini_solver_expressions.cpp

non_mini_solver_constraints.o: naxos-2.0.5/core/non_mini_solver_constraints.cpp
	$(CC) $(CFLAGS) -c naxos-2.0.5/core/non_mini_solver_constraints.cpp

problemmanager.o: naxos-2.0.5/core/problemmanager.cpp
	$(CC) $(CFLAGS) -c naxos-2.0.5/core/problemmanager.cpp

clean:
	rm -f crewas *.o

# curve.o: Fred/src/curve.cpp Fred/include/curve.hpp
# 	$(CC) $(CFLAGS) -c Fred/src/curve.cpp