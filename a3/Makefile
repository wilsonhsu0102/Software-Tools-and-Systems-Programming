FLAGS = -Wall -g -std=gnu99 
DEPENDENCIES = helper.h

psort: helper.o psort.o
	gcc ${FLAGS} -o $@ $^

%.o: %.c ${DEPENDENCIES}
	gcc ${FLAGS} -c $<

clean: 
	rm -f *.o psort 
