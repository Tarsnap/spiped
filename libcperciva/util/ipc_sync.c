#define IPC_SYNC_ONLY 1

#define shared_sync ipc_sync
#define shared_sync_init ipc_sync_init
#define shared_sync_wait ipc_sync_wait
#define shared_sync_signal ipc_sync_signal
#define shared_sync_wait_prep ipc_sync_wait_prep
#define shared_sync_signal_prep ipc_sync_signal_prep
#define shared_sync_done ipc_sync_done

#include "shared_sync.c"
