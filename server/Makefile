INCLUDE_DIRS = 
LIB_DIRS =

CROSS_COMPILE =

CC?=$(CROSS_COMPILE)gcc
CFLAGS?= -O0 -Wall -Werror -O -g $(INCLUDE_DIRS) $(LDFLAGS)
LDFLAGS?= -pthread -lrt

PRODUCT=aesdsocket

HFILES= queue.h 
CFILES= aesdsocket.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	${PRODUCT}

clean:
	-rm -f *.o *.NEW *~
	-rm -f ${PRODUCT} ${DERIVED} ${GARBAGE}

${PRODUCT}:	${OBJS}
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<

