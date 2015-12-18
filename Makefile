
TARGETS=rvgahp_ce rvgahp_proxy cesocktest
CFLAGS=-Wall --std=gnu99

all: $(TARGETS)

rvgahp_ce: rvgahp_ce.o common.o
rvgahp_proxy: rvgahp_proxy.o common.o
cesocktest: cesocktest.o

clean:
	rm -f *.o $(TARGETS)
