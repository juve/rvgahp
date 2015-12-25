/**
 * Copyright 2015 University of Southern California
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef RVGAHP_CONDOR_CONFIG_H
#define RVGAHP_CONDOR_CONFIG_H

#define DEFAULT_BROKER_HOST "127.0.0.1"
#define DEFAULT_BROKER_PORT "41000"
#define DEFAULT_INTERVAL "60"

#include <unistd.h>

#define log(stream, fmt, ...) \
    fprintf(stream, "%s %s[%d]: " fmt, timestamp(), argv0, \
            getpid(), ##__VA_ARGS__); fflush(stream)

char *timestamp();
int condor_config_val(char *var, char *val, size_t valsize, const char *default_val);

#endif
