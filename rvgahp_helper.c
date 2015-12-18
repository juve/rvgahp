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

/* TODO Determine CE name from arg1 */
/* TODO Construct and listen on UNIX domain socket */

#define SECONDS 1000
#define TIMEOUT (300*SECONDS)

char *argv0 = NULL;

void usage() {
    fprintf(stderr, "Usage: %s\n", argv0);
    fprintf(stderr, "This command takes no arguments\n");
}

int main(int argc, char **argv) {
    argv0 = basename(strdup(argv[0]));

    if (argc != 1) {
        fprintf(stderr, "Invalid argument\n");
        usage();
        exit(1);
    }

    unsigned short port = 41000;

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

    /* Initally poll to accept and check stdin */
    struct pollfd ufds[2];
    int client = -1;
    while (1) {
        ufds[0].fd = sck;
        ufds[0].events = POLLIN;
        ufds[0].revents = 0;
        ufds[1].fd = STDIN_FILENO;
        ufds[1].events = POLLHUP|POLLERR;
        ufds[1].revents = 0;

        int rv = poll(ufds, 2, TIMEOUT);
        if (rv == -1) {
            fprintf(stderr, "ERROR polling for connection/stdin close: %s\n", strerror(errno));
            exit(1);
        } else if (rv == 0) {
            fprintf(stderr, "ERROR timeout occurred\r\n");
            exit(1);
        } else {
            if (ufds[0].revents & POLLIN) {
                client = accept(sck, (struct sockaddr *)&peer_addr, &addrlen);
                if (client < 0) {
                    fprintf(stderr, "ERROR accepting connection: %s\n", strerror(errno));
                    exit(1);
                }
                break;
            }
            if (ufds[0].revents != 0 && ufds[0].revents != POLLHUP) {
                fprintf(stderr, "ERROR on listening socket!\n");
                exit(1);
            }
            if (ufds[1].revents != 0) {
                fprintf(stderr, "ERROR stdin closed\n");
                exit(1);
            }
        }
    }

    /* Close the server socket before connecting I/O so the next proxy process
     * can start listening. */
    close(sck);

    /* Handle all the I/O between client and server */
    int bytes_read = 0;
    char buf[BUFSIZ];
    while (1) {
        ufds[0].fd = client;
        ufds[0].events = POLLIN;
        ufds[0].revents = 0;
        ufds[1].fd = STDIN_FILENO;
        ufds[1].events = POLLIN;
        ufds[1].revents = 0;

        int rv = poll(ufds, 2, -1);
        if (rv == -1) {
            fprintf(stderr, "ERROR polling socket/stdin: %s\n", strerror(errno));
            exit(1);
        } else {
            if (ufds[0].revents & POLLIN) {
                bytes_read = recv(client, buf, BUFSIZ, 0);
                if (bytes_read > 0) {
                    write(STDOUT_FILENO, buf, bytes_read);
                }
            }
            if (ufds[0].revents != 0 && ufds[0].revents != POLLIN) {
                fprintf(stderr, "ERROR on client socket!\n");
                exit(1);
            }
            if (ufds[1].revents & POLLIN) {
                bytes_read = read(STDIN_FILENO, buf, BUFSIZ);
                if (bytes_read > 0) {
                    send(client, buf, bytes_read, 0);
                }
            }
            if (ufds[1].revents & POLLHUP) {
                fprintf(stderr, "STDIN closed\n");
                exit(1);
            }
        }
    }

    close(client);

    return 0;
}

