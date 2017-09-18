EXEC = ./obj-intel64/roeval
EXEC_RELEASE = ./obj-intel64/roeval_release

FLAGS_DEBUGS =
ifdef TEST
	FLAGS_DEBUGS = -DTEST -g
endif 

CPP   = g++
FLAGS = -Wall -O3	 -std=c++11 -I./src/ $(FLAGS_DEBUGS)
LDFLAGS= -lz -Wall -O0 $(FLAGS_DEBUGS)

SRC= $(wildcard src/*.cc)
OBJ= $(subst src/, obj-intel64/,  $(SRC:.cc=.o))

$(EXEC) : $(OBJ) Makefile 
	$(CPP) -o $(EXEC) $(OBJ) $(LDFLAGS)

obj-intel64/%.o :  src/%.cc src/%.hh Makefile
	$(CPP) $(FLAGS) -c $< -o $@
	
obj-intel64/testRAPPredictor.o :  src/testRAPPredictor.cc src/testRAPPredictor.hh Makefile RAP_config.hh
	$(CPP) $(FLAGS) -c $< -o $@

$(EXEC_RELEASE) : $(EXEC)	
	cp $(EXEC) $(EXEC_RELEASE)

release: $(EXEC_RELEASE)
	
clean:
	rm -f $(EXEC) *~ src/*~ obj-intel64/*.o 

all : $(EXEC)

