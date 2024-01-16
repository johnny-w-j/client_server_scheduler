## Author: Woohyuk Jang
## Descrn: make clean tar commands for client-server simulation via threads
## Date: Nov. 7 2016

SRC = sim.c queue.c
OBJ = sim.o queue.o
HDR = check.h queue.h

sim: ${OBJ}
	gcc -Wall -pthread -o sim sim.o queue.o

sim.o: sim.c ${HDR}
	gcc -Wall -pthread -c sim.c

queue.o: queue.c ${HDR}
	gcc -Wall -pthread -c queue.c

clean:
	/bin/rm -f sim.o queue.o sim

tar:
	tar cf os_asn2_jwj11.tar ${SRC} ${HDR} makefile sim_manual readme.txt question1.txt

man:
	man ./sim_manual
