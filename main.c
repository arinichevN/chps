#include "main.h"

int app_state = APP_INIT;

TSVresult config_tsv = TSVRESULT_INITIALIZER;
char * db_data_path;

int sock_port = -1;
int sock_fd = -1;

Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};
Mutex progl_mutex = MUTEX_INITIALIZER;
Mutex db_data_mutex = MUTEX_INITIALIZER;

PeerList peer_list = LIST_INITIALIZER;
ProgList prog_list = LLIST_INITIALIZER;

#include "util.c"
#include "db.c"

int readSettings(TSVresult* r, const char *data_path, int *port, struct timespec *cd, char **db_data_path) {
    if (!TSVinit(r, data_path)) {
        return 0;
    }
    int _port = TSVgetis(r, 0, "port");
    int _cd_sec = TSVgetis(r, 0, "cd_sec");
    int _cd_nsec = TSVgetis(r, 0, "cd_nsec");
    char *_db_data_path = TSVgetvalues(r, 0, "db_data_path");
    if (TSVnullreturned(r)) {
        return 0;
    }
    *port = _port;
    cd->tv_sec = _cd_sec;
    cd->tv_nsec = _cd_nsec;
    *db_data_path = _db_data_path;
    return 1;
}

int initData() {
    if (!config_getPeerList(&peer_list, NULL, db_data_path)) {
        freePeerList(&peer_list);
        return 0;
    }
    if (!loadActiveProg(&prog_list, &peer_list, db_data_path)) {
        freeProgList(&prog_list);
        freePeerList(&peer_list);
        return 0;
    }
    return 1;
}

void initApp() {
    if (!readSettings(&config_tsv, CONFIG_FILE, &sock_port, &cycle_duration, &db_data_path)) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
#ifdef MODE_DEBUG
    printf("%s(): \n\tsock_port: %d, \n\tcycle_duration: %ld sec %ld nsec, \n\tdb_data_path: %s\n", F, sock_port, cycle_duration.tv_sec, cycle_duration.tv_nsec, db_data_path);
#endif
    if (!initMutex(&progl_mutex)) {
        exit_nicely_e("initApp: failed to initialize progl_mutex\n");
    }
    if (!initMutex(&db_data_mutex)) {
        exit_nicely_e("initApp: failed to initialize db_data_mutex\n");
    }
    if (!initServer(&sock_fd, sock_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }
}

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    DEF_SERVER_I1LIST
    if (
            ACP_CMD_IS(ACP_CMD_PROG_STOP) ||
            ACP_CMD_IS(ACP_CMD_PROG_START) ||
            ACP_CMD_IS(ACP_CMD_PROG_RESET) ||
            ACP_CMD_IS(ACP_CMD_PROG_ENABLE) ||
            ACP_CMD_IS(ACP_CMD_PROG_DISABLE) ||
            ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT) ||
            ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME) ||
            ACP_CMD_IS(ACP_CMD_GET_FTS)
            ) {
        acp_requestDataToI1List(&request, &i1l);
        if (i1l.length <= 0) {
            return;
        }

    } else {
        return;
    }
    if (ACP_CMD_IS(ACP_CMD_PROG_STOP)) {

        if (lockMutex(&db_data_mutex)) {
            for (int i = 0; i < i1l.length; i++) {
                Prog *item = getProgById(i1l.item[i], &prog_list);
                if (item != NULL) {
                    deleteProgById(i1l.item[i], &prog_list, db_data_path);
                }
            }
            unlockMutex(&db_data_mutex);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_START)) {

        if (lockMutex(&db_data_mutex)) {
            for (int i = 0; i < i1l.length; i++) {
                addProgById(i1l.item[i], &prog_list, &peer_list, NULL, db_data_path);
            }
            unlockMutex(&db_data_mutex);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_RESET)) {

        if (lockMutex(&db_data_mutex)) {
            for (int i = 0; i < i1l.length; i++) {
                Prog *item = getProgById(i1l.item[i], &prog_list);
                if (item != NULL) {
                    deleteProgById(i1l.item[i], &prog_list, db_data_path);
                }
            }

            for (int i = 0; i < i1l.length; i++) {
                addProgById(i1l.item[i], &prog_list, &peer_list, NULL, db_data_path);
            }
            unlockMutex(&db_data_mutex);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_ENABLE)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    if (item->control_data.state == OFF) {
                        item->control_data.state = INIT;

                        if (lockMutex(&db_data_mutex)) {
                            db_saveTableFieldInt("prog", "enable", item->id, 1, NULL, db_data_path);
                            unlockMutex(&db_data_mutex);
                        }
                    }
                    unlockMutex(&item->mutex);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_DISABLE)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    if (item->control_data.state != OFF) {
                        item->control_data.state = DISABLE;

                        if (lockMutex(&db_data_mutex)) {
                            db_saveTableFieldInt("prog", "enable", item->id, 0, NULL, db_data_path);
                            unlockMutex(&db_data_mutex);
                        }
                    }
                    unlockMutex(&item->mutex);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgInit(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgRuntime(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_GET_FTS)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgFTS(item, &response)) {
                    return;
                }
            }
        }
    }
    acp_responseSend(&response, &peer_client);

}

void progControl(ControlData *item) {
#ifdef MODE_DEBUG
    char *state = getStateStr(item->state);
    struct timespec tm1 = getTimeRestTmr(item->check_interval, item->tmr_check);
    printf(" state:%s result:%.2f %d %ld check_tm_rest:%ldsec\n", state, item->result.value, item->result.state, item->result.tm.tv_sec, tm1.tv_sec);
#endif
    switch (item->state) {
        case INIT:
            ton_ts_reset(&item->tmr_check);
            lockMutex(&item->result_mutex);
            item->result.state = 0;
            unlockMutex(&item->result_mutex);
            item->state = RUN;
            break;
        case RUN:
            if (!ton_ts(item->check_interval, &item->tmr_check)) {
                float value;
                if (acp_peerListIsActive(&item->peer_list)) {
                    value = GOOD_FLOAT;
                } else {
                    value = BAD_FLOAT;
                }
                if (lockMutex(&item->result_mutex)) {
                    item->result.value = value;
                    item->result.tm = getCurrentTime();
                    item->result.state = 1;
                    unlockMutex(&item->result_mutex);
                }
            }
            break;
        case DISABLE:
            item->result.state = 0;
            item->state = OFF;
            break;
        case OFF:
            break;
        default:
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): unknown state, switched to OFF where\n", F);
#endif
            item->state = OFF;
            break;
    }
}
#undef INTERVAL_CHECK

void cleanup_handler(void *arg) {
    Prog *item = arg;
    printf("cleaning up thread %d\n", item->id);
}

void *threadFunction(void *arg) {
    Prog *item = arg;
#ifdef MODE_DEBUG
    printf("thread for program with id=%d has been started\n", item->id);
#endif
#ifdef MODE_DEBUG
    pthread_cleanup_push(cleanup_handler, item);
#endif
    while (1) {
        struct timespec t1 = getCurrentTime();
        int old_state;
        if (threadCancelDisable(&old_state)) {
            if (lockMutex(&item->mutex)) {
#ifdef MODE_DEBUG
                printf("prog_id: %d", item->id);
#endif
                progControl(&item->control_data);
                unlockMutex(&item->mutex);
            }
            threadSetCancelState(old_state);
        }
        sleepRest(item->cycle_duration, t1);
    }
#ifdef MODE_DEBUG
    pthread_cleanup_pop(1);
#endif
}

void freeData() {
    stopAllProgThreads(&prog_list);
    freeProgList(&prog_list);
    freePeerList(&peer_list);
}

void freeApp() {
    freeData();
    freeSocketFd(&sock_fd);
    freeMutex(&progl_mutex);
    freeMutex(&db_data_mutex);
    TSVclear(&config_tsv);
}

void exit_nicely() {
    freeApp();
#ifdef MODE_DEBUG
    puts("\nBye...");
#endif
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {
    fprintf(stderr, "%s", s);
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
    int data_initialized = 0;
    while (1) {
#ifdef MODE_DEBUG
        printf("main(): %s %d\n", getAppState(app_state), data_initialized);
#endif
        switch (app_state) {
            case APP_INIT:
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
                data_initialized = initData();
                app_state = APP_RUN;
                delayUsIdle(1000000);
                break;
            case APP_RUN:
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
                freeApp();
                delayUsIdle(1000000);
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
                exit_nicely();
                break;
            default:
                exit_nicely_e("main: unknown application state");
                break;
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}
