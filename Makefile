
TARGETS=rvgahp_ce rvgahp_proxy
CFLAGS=-Wall --std=gnu99

all: $(TARGETS)

rvgahp_ce: rvgahp_ce.o condor_config.o
rvgahp_proxy: rvgahp_proxy.o condor_config.o

clean:
	rm -f *.o $(TARGETS)
