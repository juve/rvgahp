
TARGETS=rvgahp_ce rvgahp_proxy rvgahp_helper
CFLAGS=-Wall --std=gnu99

all: $(TARGETS)

rvgahp_ce: rvgahp_ce.o common.o
rvgahp_proxy: rvgahp_proxy.o common.o
rvgahp_helper: rvgahp_helper.o common.o

clean:
	rm -f *.o $(TARGETS)
