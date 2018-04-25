EXEC = ./obj-intel64/roeval
EXEC_RELEASE = ./obj-intel64/roeval_release

FLAGS = -std=c++11 -I./src/
LDFLAGS= -lz 

ifdef TEST
	FLAGS += -DTEST -g -O0
	LDFLAGS += -g -O0
else 
	LDFLAGS += -Wall -O3
	FLAGS += -Wall -O3
endif 

CPP   = g++

SRC= $(wildcard src/*.cc)
OBJ= $(subst src/, obj-intel64/,  $(SRC:.cc=.o))

$(EXEC) : $(OBJ) Makefile 
	$(CPP) -o $(EXEC) $(OBJ) $(LDFLAGS)

obj-intel64/%.o :  src/%.cc src/%.hh Makefile src/common.hh src/common.cc src/Cache.hh
	$(CPP) $(FLAGS) -c $< -o $@
	
$(EXEC_RELEASE) : $(EXEC)	
	cp $(EXEC) $(EXEC_RELEASE)

release: $(EXEC_RELEASE)
	
clean:
	rm -f $(EXEC) *~ src/*~ obj-intel64/*.o 

all : $(EXEC)

