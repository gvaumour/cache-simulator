
EXEC=trace-gen
FLAGS= -O0 -Wall -std=c++11 -lz

make: $(EXEC)
$(EXEC): Makefile main.cpp
	g++ main.cpp  $(FLAGS) -o $(EXEC)
clean:
	rm $(EXEC) 
