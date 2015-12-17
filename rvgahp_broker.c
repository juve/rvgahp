#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <libgen.h>

#include "common.h"

#define BROKER_BANNER "rvgahp_broker server"

char *argv0;

void usage() {
    fprintf(stderr, "Usage: %s\n", argv0);
    fprintf(stderr, "This command has no options\n");
}

int main(int argc, char **argv) {
    argv0 = basename(strdup(argv[0]));

    if (argc > 1) {
        fprintf(stderr, "Invalid argument: %s\n", argv[1]);
        usage();
        exit(1);
    }

    log("Starting...\n");

    char port_str[10];
    if (condor_config_val("RVGAHP_BROKER_PORT", port_str, 10, DEFAULT_BROKER_PORT) != 0) {
        fprintf(stderr, "ERROR Unable to read RVGAHP_BROKER_PORT from config\n");
        exit(1);
    }
    unsigned short port;
    if (sscanf(port_str, "%hu", &port) != 1) {
        fprintf(stderr, "ERROR Invalid port: %s\n", port_str);
        exit(1);
    }
    log("Port: %hu\n", port);

    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in this_addr, peer_addr;
    memset(&this_addr, 0, addrlen);
    memset(&peer_addr, 0, addrlen);

    this_addr.sin_port = htons(port);
    this_addr.sin_family = AF_INET;
    this_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sck = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sck < 0) {
        fprintf(stderr, "ERROR creating socket: %s\n", strerror(errno));
        return 1;
    }

    int enable = 1;
    if (setsockopt(sck, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        fprintf(stderr, "ERROR setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
        return 1;
    }

    if (bind(sck, (struct sockaddr *)&this_addr, addrlen) < 0) {
        fprintf(stderr, "ERROR binding socket: %s\n", strerror(errno));
        return 1;
    }

    if (listen(sck, 5) < 0) {
        fprintf(stderr, "ERROR listening on socket: %s\n", strerror(errno));
        return 1;
    }

    struct pollfd ufds[1];
    while (1) {
        ufds[0].fd = sck;
        ufds[0].events = POLLIN;
        ufds[0].revents = 0;

        /* TODO Fill in the pollfd structure from the list of clients */

        int rv = poll(ufds, 1, -1);
        if (rv == -1) {
            perror("poll");
        } else if (rv == 0) {
            printf("ERROR Timeout occurred!\r\n");
            exit(1);
        } else {
            if (ufds[0].revents != 0) {
                if (ufds[0].revents != POLLIN) {
                    fprintf(stderr, "ERROR on socket!\n");
                    break;
                }
                int client = accept(sck, (struct sockaddr *)&peer_addr, &addrlen);
                if (client <= 0) {
                    fprintf(stderr, "ERROR accepting connection: %s\n", strerror(errno));
                    continue;
                }

                log("Got connection\n");

                dprintf(client, "%s\r\n", BROKER_BANNER);

                /* TODO Add client to data structure */
                close(client);
            }
        }
    }

    close(sck);

    exit(0);
}

