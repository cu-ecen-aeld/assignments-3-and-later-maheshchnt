INCLUDE_DIRS = 
LIB_DIRS = 
CC = gcc

CDEFS=
CFLAGS= -O0 -Wall -Werror -O -g $(INCLUDE_DIRS) $(CDEFS)
#LIBS=-lpthread

PRODUCT=writer

HFILES= 
CFILES= writer.c

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

