INCLUDE_DIRS = 
LIB_DIRS =
ifdef CROSS_COMPILE
	CC = $(CROSS_COMPILE)gcc
else
CC = gcc
endif

CDEFS=
CFLAGS= -O0 -Wall -Werror -O -g $(INCLUDE_DIRS) $(CDEFS)
#LIBS=-lpthread

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
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<

