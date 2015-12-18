#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <libgen.h>

#include "common.h"

/* TODO Construct and listen on UNIX domain socket */

#define SECONDS 1000
#define MINUTES (60*SECONDS)
#define TIMEOUT (30*MINUTES)

char *argv0 = NULL;

void usage() {
    fprintf(stderr, "Usage: %s SOCKPATH\n\n", argv0);
    fprintf(stderr, "Where SOCKPATH is the path to the unix domain socket "
                    "that should be created\n");
}

int main(int argc, char **argv) {
    argv0 = basename(strdup(argv[0]));

    if (argc != 2) {
        fprintf(stderr, "ERROR Invalid argument\n");
        usage();
        exit(1);
    }

    char *sockpath = argv[1];

    fprintf(stderr, "%s starting...\n", argv0);
    fprintf(stderr, "UNIX socket: %s\n", sockpath);

    /* TODO Check sockpath */
    unlink(sockpath);

    int sck = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sck < 0) {
        fprintf(stderr, "ERROR creating socket: %s\n", strerror(errno));
        return 1;
    }

    struct sockaddr_un local;
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, sockpath);
    socklen_t addrlen = strlen(local.sun_path) + sizeof(local.sun_family);

    if (bind(sck, (struct sockaddr *)&local, addrlen) < 0) {
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
            /* TODO Timeout more frequently and make sure our socket didn't get deleted */
            fprintf(stderr, "ERROR timeout occurred\r\n");
            exit(1);
        } else {
            if (ufds[0].revents & POLLIN) {
                struct sockaddr_un remote;
                client = accept(sck, (struct sockaddr *)&remote, &addrlen);
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

    /* Remove the socket here */
    unlink(sockpath);

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
                if (bytes_read == 0) {
                    fprintf(stderr, "Client disconnected\n");
                    exit(1);
                } else if (bytes_read < 0) {
                    fprintf(stderr, "ERROR reading data from client: %s\n", strerror(errno));
                    exit(1);
                } else {
                    write(STDOUT_FILENO, buf, bytes_read);
                }
            }
            if (ufds[0].revents != 0 && ufds[0].revents != POLLIN) {
                fprintf(stderr, "ERROR on client socket!\n");
                exit(1);
            }
            if (ufds[1].revents & POLLIN) {
                bytes_read = read(STDIN_FILENO, buf, BUFSIZ);
                if (bytes_read == 0) {
                    fprintf(stderr, "STDIN EOF\n");
                    exit(1);
                } else if (bytes_read < 0) {
                    fprintf(stderr, "ERROR reading from STDIN: %s\n", strerror(errno));
                    exit(1);
                } else {
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

