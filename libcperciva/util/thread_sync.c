#define IPC_SYNC_ONLY 0

#define shared_sync thread_sync
#define shared_sync_init thread_sync_init
#define shared_sync_wait thread_sync_wait
#define shared_sync_signal thread_sync_signal
#define shared_sync_done thread_sync_done

#include "shared_sync.c"
