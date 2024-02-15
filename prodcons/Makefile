
PROGRAMS = main test_rate_limiter test_tcp_utils

LDLIBS = -lpthread

# CFLAGS = '-w'

all: $(PROGRAMS)

main: main.c rate_limiter.o tcp_utils.o
	${CC} $< -o $@

test_rate_limiter: test_rate_limiter.c rate_limiter.o
	${CC} $< -o $@

test_tcp_utils: test_tcp_utils.c tcp_utils.o
	${CC} $< -o $@

# rate_limiter.o: rate_limiter.c
# tcp_utils.o: rate_limiter.c

clean:
	rm $(PROGRAMS) rate_limiter.o tcp_utils.o

