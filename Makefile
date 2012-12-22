# Finished at Oct. 20 2012

myhttpd: myhttpd.o main.o
	gcc -o myhttpd myhttpd.o main.o -lpthread

myhttpd.o: myhttpd.c myhttpd.h
	gcc -c -Wall myhttpd.c -lpthread

main.o: main.c myhttpd.h
	gcc -c -Wall main.c -lpthread
