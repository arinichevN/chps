
#ifndef CHPS_H
#define CHPS_H


#include "lib/util.h"
#include "lib/crc.h"
#include "lib/app.h"
#include "lib/timef.h"
#include "lib/udp.h"
#include "lib/tsv.h"
#include "lib/configl.h"
#include "lib/dbl.h"
#include "lib/acp/main.h"
#include "lib/acp/prog.h"



#define APP_NAME chps
#define APP_NAME_STR TOSTRING(APP_NAME)


#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./"
#endif

#define CONFIG_FILE "" CONF_DIR "config.tsv"

#define WAIT_RESP_TIMEOUT 3

#define PROG_LIST_LOOP_ST {Prog *item = prog_list.top; while (item != NULL) {
#define PROG_LIST_LOOP_SP item = item->next; } item = prog_list.top;}

enum {
    INIT,
    RUN,
    STOP,
    RESET,
    OFF,
    FAILURE,
    DISABLE
} StateAPP;

typedef struct{
    PeerList peer_list;
    struct timespec check_interval;
    int state;
    Ton_ts tmr_check;
    FTS result;
    Mutex result_mutex;
}ControlData;

struct prog_st {
    int id;
    ControlData control_data;
    struct timespec cycle_duration;
    int sock_fd;
    Mutex mutex;
    pthread_t thread;
    struct prog_st *next;
};
typedef struct prog_st Prog;

DEC_LLIST(Prog)

typedef struct {
    ProgList *prog_list;
    PeerList *peer_list;
    Prog * prog;
    sqlite3 *db_data;
} ProgData;

extern int readSettings();

extern int initData();

extern void initApp();

extern void serverRun(int *state, int init_state);

extern void progControl(ControlData *item);

extern void *threadFunction(void *arg);

extern void freeData();

extern void freeApp();

extern void exit_nicely();

extern void exit_nicely_e(char *s);
#endif 

