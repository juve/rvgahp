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

#include "condor_config.h"

char *argv0 = NULL;

void usage() {
    fprintf(stderr, "Usage: %s CE_NAME GAHP_NAME\n", argv0);
}

int main(int argc, char **argv) {
    argv0 = basename(strdup(argv[0]));

    if (argc != 3) {
        fprintf(stderr, "Wrong number of arguments: expected 3, got %d\n", argc);
        usage();
        exit(1);
    }

    char port_str[10];
    if (condor_config_val("RVGAHP_BROKER_PORT", port_str, 10, DEFAULT_BROKER_PORT) != 0) {
        fprintf(stderr, "ERROR reading RVGAHP_BROKER_PORT from config\n");
        exit(1);
    }
    unsigned short port;
    if (sscanf(port_str, "%hu", &port) != 1) {
        fprintf(stderr, "ERROR Invalid port: %s\n", port_str);
        exit(1);
    }

    char *ce = argv[1];
    char *gahp = argv[2];

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

    if (listen(sck, 0) < 0) {
        fprintf(stderr, "ERROR listening on socket: %s\n", strerror(errno));
        return 1;
    }

    /* Set an alarm to timeout the accept just in case there is no gahp_daemon */
    alarm(30);

    int client = accept(sck, (struct sockaddr *)&peer_addr, &addrlen);

    /* Turn off the timer */
    alarm(0);

    /* Close the server socket before connecting I/O */
    close(sck);

    if (client <= 0) {
        fprintf(stderr, "ERROR accepting connection: %s\n", strerror(errno));
        exit(1);
    }

    /* Tell the remote site our CE and GAHP */
    char message[BUFSIZ];
    snprintf(message, BUFSIZ, "%s %s", ce, gahp);
    if (send(client, message, strlen(message), 0) <= 0) {
        fprintf(stderr, "ERROR sending message: %s\n", strerror(errno));
        close(client);
        exit(1);
    }

    /* Handle all the I/O between client and server */
    int bytes_read = 0;
    char buf[BUFSIZ];
    struct pollfd ufds[2];

    while (1) {
        ufds[0].fd = client;
        ufds[0].events = POLLIN;
        ufds[0].revents = 0;
        ufds[1].fd = STDIN_FILENO;
        ufds[1].events = POLLIN;
        ufds[1].revents = 0;

        int rv = poll(ufds, 2, -1);
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
                bytes_read = recv(client, buf, BUFSIZ, 0);
                if (bytes_read == 0) {
                    break;
                } else {
                    write(STDOUT_FILENO, buf, bytes_read);
                }
            }

            if (ufds[1].revents != 0) {
                if (ufds[1].revents != POLLIN) {
                    fprintf(stderr, "ERROR on stdin!\n");
                    break;
                }
                bytes_read = read(STDIN_FILENO, buf, BUFSIZ);
                if (bytes_read == 0) {
                    break;
                } else {
                    send(client, buf, bytes_read, 0);
                }
            }
        }
    }

    close(client);

    return 0;
}

