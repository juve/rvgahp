#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <libgen.h>

#include "condor_config.h"

#define DEFAULT_BROKER_HOST "127.0.0.1"
#define DEFAULT_BROKER_PORT "41000"

char *argv0 = NULL;

void usage() {
    fprintf(stderr, "Usage: %s\n", argv0);
    fprintf(stderr, "This command has no options\n");
}

int set_condor_config() {
    char *homedir = getenv("HOME");
    if (homedir == NULL) {
        fprintf(stderr, "ERROR HOME is not set in environment");
        return -1;
    }

    char config_file[BUFSIZ];
    snprintf(config_file, BUFSIZ, "%s/%s", homedir, ".rvgahp/condor_config.rvgahp");
    if (access(config_file, R_OK) < 0) {
        fprintf(stderr, "ERROR Cannot find config file %s", config_file);
        return -1;
    }

    setenv("CONDOR_CONFIG", config_file, 1);
    return 0;
}

int main(int argc, char** argv) {
    argv0 = basename(strdup(argv[0]));

    if (argc > 1) {
        fprintf(stderr, "Invalid argument: %s\n", argv[1]);
        usage();
        exit(1);
    }

    printf("%s starting...\n", argv0);

    /* Set up configuration */
    if (set_condor_config() < 0) {
        exit(1);
    }
    printf("config: %s\n", getenv("CONDOR_CONFIG"));

    /* Get host and port from config */
    char server[1024];
    if (condor_config_val("RVGAHP_BROKER_HOST", server, 1024, DEFAULT_BROKER_HOST) != 0) {
        fprintf(stderr, "ERROR Unable to read RVGAHP_BROKER_HOST from config file\n");
        exit(1);
    }

    char server_port_str[10];
    if (condor_config_val("RVGAHP_BROKER_PORT", server_port_str, 10, DEFAULT_BROKER_PORT) != 0) {
        fprintf(stderr, "ERROR Unable to read RVGAHP_BROKER_PORT from config file\n");
        exit(1);
    }
    unsigned short port;
    if (sscanf(server_port_str, "%hu", &port) != 1) {
        fprintf(stderr, "ERROR Invalid RVGAHP_BROKER_PORT: %s\n", server_port_str);
        exit(1);
    }
    printf("server: %s:%hu\n", server, port);

    while (1) {
        struct sockaddr_in this_addr, serv_addr;
        int addrlen = sizeof(struct sockaddr_in);
        memset(&this_addr, 0, addrlen);
        memset(&serv_addr, 0, addrlen);

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        inet_pton(AF_INET, server, &serv_addr.sin_addr);

        int sck = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

        if (connect(sck, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            /* If we didn't connect, then wait a few seconds and try again */
            sleep(10);
            continue;
        }

        printf("Connected to rvgahp_proxy\n");

        /* Get the name of the GAHP to launch from the reverse_gahp */
        char gahp[BUFSIZ];
        int sz = read(sck, gahp, BUFSIZ);
        if (sz <= 0) {
            fprintf(stderr, "ERROR reading gahp name from rvgahp_proxy\n");
            close(sck);
            continue;
        }
        gahp[sz] = '\0';

        printf("Launching GAHP: %s\n", gahp);

        /* Construct the actual GAHP command */
        char gahp_command[BUFSIZ];
        if (strncmp("batch_gahp", gahp, 10) == 0) {
            char batch_gahp[BUFSIZ]; 
            if (condor_config_val("BATCH_GAHP", batch_gahp, BUFSIZ, NULL) < 0) {
                goto next;
            }
            char glite_location[BUFSIZ];
            if (condor_config_val("GLITE_LOCATION", glite_location, BUFSIZ, NULL) < 0) {
                goto next;
            }
            snprintf(gahp_command, BUFSIZ, "GLITE_LOCATION=%s %s", glite_location, batch_gahp);
        } else if (strncmp("condor_ft-gahp", gahp, 14) == 0) {
            char ft_gahp[BUFSIZ];
            if (condor_config_val("FT_GAHP", ft_gahp, BUFSIZ, NULL) < 0) {
                goto next;
            }
            snprintf(gahp_command, BUFSIZ, "%s -f", ft_gahp);
        } else {
            fprintf(stderr, "ERROR: Unknown GAHP: %s\n", gahp);
            goto next;
        }
        printf("Actual GAHP command: %s\n", gahp_command);

        pid_t c1 = fork();
        if (c1 > 0) {
            /* Wait for the GAHP process' parent to exit */
            int status;
            waitpid(c1, &status, 0);
            if (status != 0) {
                fprintf(stderr, "ERROR starting GAHP");
            }
        } else if(c1 == 0) {
            /* Make this process the session leader */
            setsid();

            pid_t c2 = fork();
            if (c2 > 0) {
                exit(0);
            } else if (c2 == 0) {
                chdir("/");

                int original_error = dup(2);

                close(0); /* close standard input  */
                close(1); /* close standard output */
                close(2); /* close standard error  */

                if (dup(sck) != 0 || dup(sck) != 1 || dup(sck) != 2) {
                    dprintf(original_error, "ERROR duplicating socket for stdin/stdout/stderr: %s\n", strerror(errno));
                    exit(1);
                }

                execl("/bin/sh", "/bin/sh", "-c", gahp_command, NULL);
                dprintf(original_error, "ERROR launching GAHP: %s\n", strerror(errno));
                _exit(1);
            } else {
                fprintf(stderr, "ERROR forking GAHP process (2): %s\n", strerror(errno));
                exit(1);
            }
        } else {
            fprintf(stderr, "Error forking GAHP process (1): %s\n", strerror(errno));
        }

next:
        /* We no longer need the client socket */
        close(sck);
    }

    exit(0);
}

