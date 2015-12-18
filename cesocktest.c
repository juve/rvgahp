#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

void loop() {
    int socks[2];

    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
        fprintf(stderr, "ERROR socketpair failed: %s\n", strerror(errno));
        exit(1);
    }

    int gahp_sock = socks[0];
    int ssh_sock = socks[1];

    printf("Starting SSH connection\n");
    pid_t ssh_pid = fork();
    if (ssh_pid == 0) {
        int orig_err = dup(STDERR_FILENO);
        close(gahp_sock);
        close(0);
        close(1);
        dup(ssh_sock);
        dup(ssh_sock);
        execl("/bin/sh", "/bin/sh", "-c", "~/.rvgahp/rvgahp_ssh", NULL);
        dprintf(orig_err, "ERROR execing ssh script\n");
        _exit(1);
    } else if (ssh_pid < 0) {
        fprintf(stderr, "ERROR forking ssh script\n");
        exit(1);
    }

    /* Close here so that if the remote process dies, our read returns */
    close(ssh_sock);

    /* Print welcome message */
    printf("Printing welcome\n");
    if (dprintf(gahp_sock, "Reverse GAHP 2.0\r\n") < 0) {
        fprintf(stderr, "ERROR writing welcome message\n");
        goto again;
    }

    /* Get name of GAHP to launch */
    printf("Waiting for request\n");
    char buf[BUFSIZ];
    ssize_t b = read(gahp_sock, buf, BUFSIZ);
    if (b < 0) {
        fprintf(stderr, "ERROR read from SSH failed: %s\n", strerror(errno));
        /* This probably happened because the SSH process died */
        /* TODO Check to see if ssh is running */
        exit(1);
    }
    if (b == 0) {
        fprintf(stderr, "ERROR SSH socket closed\n");
        goto again;
    }

    /* Trim the message */
    buf[b] = '\0';
    char c = buf[--b];
    while (c == '\r' || c == '\n') {
        buf[b] = '\0';
        c = buf[--b];
    }

    if (strncasecmp(buf, "gahp", 4) == 0) {
        printf("Starting batch GAHP\n");
        pid_t gahp_pid = fork();
        if (gahp_pid == 0) {
            int orig_err = dup(STDERR_FILENO);
            close(0);
            close(1);
            close(2);
            dup(gahp_sock);
            dup(gahp_sock);
            dup(gahp_sock);
            execl("/bin/sh", "/bin/sh", "-c", "GLITE_LOCATION=/usr/local/BLAH /usr/local/BLAH/bin/blahpd", NULL);
            dprintf(orig_err, "ERROR execing GAHP\n");
            _exit(1);
        } else if (gahp_pid < 0) {
            fprintf(stderr, "ERROR launching GAHP\n");
            exit(1);
        }
    } else {
        fprintf(stderr, "ERROR Unknown request: %s\n", buf);
        dprintf(gahp_sock, "E Invalid request\r\n");
    }

again:
    close(gahp_sock);
}

int main(int argc, char **argv) {
    signal(SIGCHLD, SIG_IGN);
    while(1) {
        loop();
    }
}

