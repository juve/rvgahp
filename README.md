Reverse GAHP
============

This is an implementation of the remote_gahp for Condor that doesn't require
ssh connections to the remote resource. Instead, it uses connection brokering
to establish bi-directional communication between the GridManager and GAHP
processes running on the remote resource.

Configuration
-------------

On your submit host:

1. In your condor_config set:

  REMOTE_GAHP = /path/to/rvgahp_proxy

On the remote resource:

1. Create $HOME/.condor/log
2. Create a $HOME/.condor/condor_config.ft-gahp
3. In condor_config.ft-gahp set:

  BOSCO_SANDBOX_DIR = $ENV(HOME)/.condor
  LOG = $ENV(HOME)/.condor/log
  FT_GAHP_LOG = $(LOG)/FTGahpLog
  LIBEXEC = /usr/libexec/condor

4. start the rvgahp_ce process.
