#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "condor_config.h"

char *condor_config_val(char *var) {
    char cmd[BUFSIZ];
    snprintf(cmd, BUFSIZ, "condor_config_val %s", var);

    FILE *proc = popen(cmd, "r");
    if (proc == NULL) {
        fprintf(stderr, "ERROR getting %s config var\n", var);
        return NULL;
    }

    char val[BUFSIZ];
    if (fgets(val, BUFSIZ, proc) == NULL) {
        fprintf(stderr, "ERROR reading output of condor_config_val\n");
        pclose(proc);
        return NULL;
    }

    int status = pclose(proc);
    if (status != 0) {
        fprintf(stderr, "ERROR reading condor_config_val %s:\n", var);
        fprintf(stderr, "%s", val);
        return NULL;
    }

    int sz = strlen(val) - 1;
    while (val[sz] == '\n' || val[sz] == '\r') {
        val[sz] = '\0';
        sz--;
    }

    return strdup(val);
}

