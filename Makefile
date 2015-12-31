
TARGETS=rvgahp_server rvgahp_proxy rvgahp_client
CFLAGS=-Wall --std=gnu99
LDFLAGS=-lm

all: $(TARGETS)

rvgahp_server: rvgahp_server.o common.o
rvgahp_proxy: rvgahp_proxy.o common.o
rvgahp_client: rvgahp_client.o common.o

clean:
	rm -f *.o $(TARGETS)
