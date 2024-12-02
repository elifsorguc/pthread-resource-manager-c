all: libreman.a app

libreman.a: reman.c
	gcc -Wall -c reman.c
	ar -cvq libreman.a reman.o
	ranlib libreman.a

app: exp.c
	gcc -Wall -o app exp.c -L. -lreman -lpthread

clean:
	rm -f *.o *.a app
