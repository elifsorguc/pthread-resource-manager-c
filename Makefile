all: libreman.a app

libreman.a: reman.c
	gcc -Wall -c reman.c
	ar -cvq libreman.a reman.o
	ranlib libreman.a

app: app.c
	gcc -Wall -o app app.c -L. -lreman -lpthread
clean:
	rm -fr *.o *.a *~ a.out app reman.o reman.a libreman.a