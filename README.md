Reverse GAHP
============

This is an implementation of the remote_gahp for Condor that doesn't require
ssh connections to the remote resource. Instead, it uses connection brokering
to establish bi-directional communication between the GridManager and GAHP
processes running on the remote resource. The key benefit of this approach is
that it enables remote job submission without requiring the remote resource
to run services that accept incoming network connections (either because of
security policy, or because of a firewall or something).

It works like this:

![rvgahp design](doc/rvgahp.png)

The rvgahp_ce process calls the rvgahp_broker to register itself by name and
maintains an open connection. If it gets disconnected, it tries to immediately
reconnect. When a remote GAHP job is submitted, the GridManager launches an
rvgahp_proxy process to communicate with the GAHP servers. The proxy binds to
an ephemeral port in Condor's LOWPORT HIGHPORT range specified in the
condor_config. After that, the proxy calls the broker and asks it to have the
CE call it back and connect it to a new GAHP process. The proxy tells the
broker which CE to contact (by name), which GAHP server to start (batch_gahp
for job submission or condor_ft-gahp for file transfer), and what its address
is (including the ephemeral port number). The CE connects to the proxy,
launches the GAHP server, and connects the stdio of the GAHP server to the
socket. The proxy copies its stdin from the GridManager to the socket, and data
from the socket to stdout and back to the GridManager. Once all connections are
established, the job execution proceeds. When the GridManager is done, the GAHP
servers exit and the connections are torn down.

Configuration
-------------

On your submit host:

1. In your condor_config set:

    ```
    LOWPORT = 50000
    HIGHPORT = 51000
    REMOTE_GAHP = /path/to/rvgahp_proxy
    ```

On the remote resource:

1. Make sure the batch_gahp/glite/blahp is installed and configured correctly
1. Create $HOME/.rvgahp
1. Create a $HOME/.rvgahp/condor_config.rvgahp
1. In condor_config.rvgahp set:

    ```
    LIBEXEC = /usr/libexec/condor
    BOSCO_SANDBOX_DIR = $ENV(HOME)/.rvgahp
    LOG = $ENV(HOME)/.rvgahp

    GLITE_LOCATION = $(LIBEXEC)/glite
    BATCH_GAHP = $(GLITE_LOCATION)/bin/batch_gahp

    FT_GAHP_LOG = $(LOG)/FTGahpLog
    FT_GAHP = /usr/sbin/condor_ft-gahp

    RVGAHP_BROKER_HOST = example.com
    RVGAHP_BROKER_PORT = 41000

    # Name of the CE (needs to match grid_resource from job)
    RVGAHP_CE_NAME = hpcc
    ```

1. Start the rvgahp_ce process.

Example Job
-----------
```
universe = grid

grid_resource = batch pbs hpcc
+remote_cerequirements = EXTRA_ARGUMENTS=="-N testjob -l walltime=00:01:00 -l nodes=1:ppn=1"

executable = /bin/date
transfer_executable = False

output = test_$(cluster).$(process).out
error = test_$(cluster).$(process).err
log = test_$(cluster).$(process).log

should_transfer_files = YES
when_to_transfer_output = ON_EXIT
copy_to_spool = false
notification = NEVER

queue 1
```
