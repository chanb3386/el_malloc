CFLAGS = -Wall -g
CC     = gcc $(CFLAGS)

PROGRAMS = \
	el_malloc.o \
	el_demo \
	showsym \

all : $(PROGRAMS)

clean :
	rm -f $(PROGRAMS) *.o vgcore.* 

el_malloc.o : el_malloc.c el_malloc.h
	$(CC) -c $<

el_demo : el_demo.c el_malloc.o
	$(CC) -o $@ $^

showsym.o : showsym.c
	$(CC) -c $<

showsym : showsym.o
	$(CC) -o $@ $^

# TESTING TARGETS
test-p1: el_malloc.o
	chmod u+x ./test_el_malloc.sh
	./test_el_malloc.sh

test-p2: showsym
	chmod u+x ./test_showsym.sh
	./test_showsym.sh

