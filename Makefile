TASK1_SRC	:= schedsim_arrival.c util.c
EXE		:= schedsim_arr_ref

all: $(EXE)

schedsim: $(TASK1_SRC)
	gcc -Wall  -std=c99 -std=gnu99 -Werror -pedantic -g $^ -o $@

clean:
	rm -f $(EXE)