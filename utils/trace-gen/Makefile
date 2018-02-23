EXEC = ./obj-intel64/trace-gen.so

PIN_ROOT=/home/gvaumour/Dev/apps/pin/pin-2.14-71313-gcc.4.4.7-linux/

FLAGS=-DBIGARRAY_MULTIPLIER=1 -std=c++11 -Wall -Werror -Wno-unknown-pragmas -fno-stack-protector -DTARGET_IA32E -DHOST_IA32E -fPIC -DTARGET_LINUX  -I$(PIN_ROOT)source/include/pin -I$(PIN_ROOT)source/include/pin/gen -I$(PIN_ROOT)extras/components/include -I$(PIN_ROOT)extras/xed-intel64/include -I$(PIN_ROOT)source/tools/InstLib -O3 -fomit-frame-pointer -fno-strict-aliasing

LDFLAGS= -shared -Wl,--hash-style=sysv -Wl,-Bsymbolic -Wl,--version-script=$(PIN_ROOT)source/include/pin/pintool.ver -L$(PIN_ROOT)intel64/lib -L$(PIN_ROOT)intel64/lib-ext -L$(PIN_ROOT)intel64/runtime/glibc -L$(PIN_ROOT)extras/xed-intel64/lib -lpin -lxed -lpindwarf -ldl -lz

CPP= g++

SRC= $(wildcard src/*.cc)
OBJ= $(subst src/, obj-intel64/,  $(SRC:.cc=.o))


$(EXEC) : $(OBJ) Makefile 
	$(CPP) -o $(EXEC) $(OBJ) $(LDFLAGS) $(FLAGS_DEBUGS)

$(EXEC_RELEASE) : $(EXEC)	
	cp $(EXEC) $(EXEC_RELEASE)

release: $(EXEC_RELEASE)


obj-intel64/%.o :  src/%.cc Makefile
	$(CPP) $(FLAGS) $(FLAGS_DEBUGS) -c $< -o $@

clean:
	rm -f $(EXEC) $(OUTPUT_FILES) *~ src/*~ obj-intel64/*.o 

all : $(EXEC)

