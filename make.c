CC = gcc 

LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a -lpthread 

FLAGS = -g -O2

CFLAGS = ${FLAGS} -I /home/courses/cse533/Stevens/unpv13e/lib

all: client server odr


server: server.o
	${CC} ${FLAGS} -o server_smandadapu server.o  get_hw_addrs.o ${LIBS}
server.o: server.c
	${CC} ${CFLAGS} -c server.c

client: client.o get_hw_addrs.o
	${CC} ${FLAGS} -o client_smandadapu client.o get_hw_addrs.o ${LIBS}
client.o: client.c
	${CC} ${CFLAGS} -c client.c

odr: odr.o get_hw_addrs.o
	${CC} ${CFLAGS} -o ODR_smandadapu odr.o get_hw_addrs.o ${LIBS}
odr.o: odr.c
	${CC} ${CFLAGS} -c odr.c 


get_hw_addrs.o: /users/cse533/Asgn3_code/get_hw_addrs.c
	${CC} ${CFLAGS} -c /users/cse533/Asgn3_code/get_hw_addrs.c 

 

clean:
	rm ODR_smandadapu odr.o client_smandadapu client.o server.o server_smandadapu get_hw_addrs.o
	
	
	
	

