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
#include <sys/un.h>

#include "common.h"

char *argv0 = NULL;

void usage() {
    fprintf(stderr, "Usage: %s SOCKPATH GAHP_NAME\n\n", argv0);
    fprintf(stderr, "Where SOCKPATH is the path to the unix domain socket\n"
                    "that should be created, and GAHP_NAME is 'batch_gahp'\n"
                    "or 'condor_ft-gahp'\n");
}

int main(int argc, char **argv) {
    argv0 = basename(strdup(argv[0]));

    if (argc != 3) {
        fprintf(stderr, "Wrong number of arguments: expected 3, got %d\n", argc);
        usage();
        exit(1);
    }

    char *sockpath = argv[1];
    char *gahp = argv[2];

    /* If the socket doesn't exist, give it 30 seconds to appear */
    int tries = 0;
    while (access(sockpath, R_OK|W_OK) != 0) {
        tries++;
        if (tries < 30) {
            sleep(1);
        } else {
            fprintf(stderr, "ERROR No UNIX socket\n");
            return 1;
        }
    }

    int sck = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sck < 0) {
        fprintf(stderr, "ERROR creating socket: %s\n", strerror(errno));
        return 1;
    }

    struct sockaddr_un remote;
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, sockpath);
    socklen_t addrlen = strlen(remote.sun_path) + sizeof(remote.sun_family);

    if (connect(sck, (struct sockaddr *)&remote, addrlen) < 0) {
        fprintf(stderr, "ERROR connecting to socket: %s\n", sockpath);
        exit(1);
    }

    /* Tell the remote site our GAHP */
    if (dprintf(sck, "%s\r\n", gahp) < 0) {
        fprintf(stderr, "ERROR sending message: %s\n", strerror(errno));
        close(sck);
        exit(1);
    }

    /* Handle all the I/O between GAHP and helper */
    int bytes_read = 0;
    char buf[BUFSIZ];
    struct pollfd ufds[2];

    while (1) {
        ufds[0].fd = sck;
        ufds[0].events = POLLIN;
        ufds[0].revents = 0;
        ufds[1].fd = STDIN_FILENO;
        ufds[1].events = POLLIN;
        ufds[1].revents = 0;

        int rv = poll(ufds, 2, -1);
        if (rv == -1) {
            fprintf(stderr, "ERROR polling socket and stdin: %s\n", strerror(errno));
            exit(1);
        } else {
            if (ufds[0].revents & POLLIN) {
                bytes_read = recv(sck, buf, BUFSIZ, 0);
                if (bytes_read == 0) {
                    /* Connection closed, should get POLLHUP */
                } else if (bytes_read > 0) {
                    write(STDOUT_FILENO, buf, bytes_read);
                } else {
                    fprintf(stderr, "ERROR reading from socket: %s\n", strerror(errno));
                    break;
                }
            }
            if (ufds[0].revents & POLLHUP) {
                fprintf(stderr, "Helper hung up\n");
                break;
            }

            if (ufds[1].revents & POLLIN) {
                bytes_read = read(STDIN_FILENO, buf, BUFSIZ);
                if (bytes_read == 0) {
                    /* GridManager closed stdin, should get POLLHUP */
                } else if (bytes_read > 0) {
                    send(sck, buf, bytes_read, 0);
                } else {
                    fprintf(stderr, "ERROR reading from stdin\n");
                    break;
                }
            }
            if (ufds[1].revents & POLLHUP) {
                fprintf(stderr, "GridManager hung up\n");
                break;
            }
        }
    }

    close(sck);

    return 0;
}

