#ifndef RVGAHP_CONDOR_CONFIG_H
#define RVGAHP_CONDOR_CONFIG_H

#define DEFAULT_BROKER_HOST "127.0.0.1"
#define DEFAULT_BROKER_PORT "41000"
#define DEFAULT_INTERVAL "60"

#include <unistd.h>

#define log(fmt, ...) \
    fprintf(stdout, "%s %s[%d]: " fmt, timestamp(), argv0, \
            getpid(), ##__VA_ARGS__)

char *timestamp();
int condor_config_val(char *var, char *val, size_t valsize, const char *default_val);

#endif
