
#include "main.h"

FUN_LLIST_GET_BY_ID(Prog)

extern int getProgByIdFDB(int prog_id, Prog *item, PeerList *peer_list, sqlite3 *dbl, const char *db_path);

void stopProgThread(Prog *item) {
#ifdef MODE_DEBUG
    printf("signaling thread %d to cancel...\n", item->id);
#endif
    if (pthread_cancel(item->thread) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_cancel()");
#endif
    }
    void * result;
#ifdef MODE_DEBUG
    printf("joining thread %d...\n", item->id);
#endif
    if (pthread_join(item->thread, &result) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_join()");
#endif
    }
    if (result != PTHREAD_CANCELED) {
#ifdef MODE_DEBUG
        printf("thread %d not canceled\n", item->id);
#endif
    }
}

void stopAllProgThreads(ProgList * list) {
    PROG_LIST_LOOP_ST
#ifdef MODE_DEBUG
            printf("signaling thread %d to cancel...\n", item->id);
#endif
    if (pthread_cancel(item->thread) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_cancel()");
#endif
    }
    PROG_LIST_LOOP_SP

    PROG_LIST_LOOP_ST
            void * result;
#ifdef MODE_DEBUG
    printf("joining thread %d...\n", item->id);
#endif
    if (pthread_join(item->thread, &result) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_join()");
#endif
    }
    if (result != PTHREAD_CANCELED) {
#ifdef MODE_DEBUG
        printf("thread %d not canceled\n", item->id);
#endif
    }
    PROG_LIST_LOOP_SP
}

void freeProg(Prog * item) {
    freeSocketFd(&item->sock_fd);
    freeMutex(&item->mutex);
    FREE_LIST(&item->control_data.peer_list);
    free(item);
}

void freeProgList(ProgList * list) {
    Prog *item = list->top, *temp;
    while (item != NULL) {
        temp = item;
        item = item->next;
        freeProg(temp);
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

int lockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_lock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("lockProgList: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_trylock(&(progl_mutex.self)) != 0) {
        return 0;
    }
    return 1;
}

int unlockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_unlock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("unlockProgList: error unlocking mutex (CMD_GET_ALL)");
#endif 
        return 0;
    }
    return 1;
}

int checkProg(const Prog * item) {
    if (item->control_data.check_interval.tv_sec <= 0 && item->control_data.check_interval.tv_nsec <= 0) {
        fprintf(stderr, "%s(): bad check_interval where prog id = %d\n", F, item->id);
        return 0;
    }

    return 1;
}

char * getStateStr(char state) {
    switch (state) {
        case OFF:
            return "OFF";
        case INIT:
            return "INIT";
        case RUN:
            return "RUN";
        case STOP:
            return "STOP";
        case FAILURE:
            return "FAILURE";
        case RESET:
            return "RESET";
        case DISABLE:
            return "DISABLE";
    }
    return "\0";
}

int bufCatProgRuntime(Prog *item, ACPResponse * response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        char *state = getStateStr(item->control_data.state);
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_ROW_STR,
                item->id,
                state
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

int bufCatProgInit(Prog *item, ACPResponse * response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_ROW_STR,
                item->id,
                item->control_data.check_interval.tv_sec
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

int bufCatProgFTS(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->control_data.result_mutex)) {
        int r = acp_responseFTSCat(item->id, item->control_data.result.value, item->control_data.result.tm, item->control_data.result.state, response);
        unlockMutex(&item->control_data.result_mutex);
        return r;
    }
    return 0;
}

void printData(ACPResponse * response) {
#define ICTD item->control_data
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "CONFIG_FILE: %s\n", CONFIG_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    SEND_STR(q)
    snprintf(q, sizeof q, "cycle_duration sec: %ld\n", cycle_duration.tv_sec);
    SEND_STR(q)
    snprintf(q, sizeof q, "cycle_duration nsec: %ld\n", cycle_duration.tv_nsec);
    SEND_STR(q)
    snprintf(q, sizeof q, "db_data_path: %s\n", db_data_path);
    SEND_STR(q)
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    SEND_STR(q)
    snprintf(q, sizeof q, "PID: %d\n", getpid());
    SEND_STR(q)
    snprintf(q, sizeof q, "sizeof prog: %u\n", sizeof (Prog));
    SEND_STR(q)
    snprintf(q, sizeof q, "prog_list length: %d\n", prog_list.length);
    SEND_STR(q)
    acp_sendPeerListInfo(&peer_list, response, &peer_client);
    SEND_STR("+-----------------------+\n");
    SEND_STR("|        Program        |\n");
    SEND_STR("+-----------+-----------+\n");
    SEND_STR("|    id     |check_int_s|\n");
    SEND_STR("+-----------+-----------+\n");
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%11ld|\n",
            item->id,
            ICTD.check_interval.tv_sec
            );
    SEND_STR(q);
    PROG_LIST_LOOP_SP
    SEND_STR("+-----------+-----------+\n");

    SEND_STR("+-----------------------+\n");
    SEND_STR("|       Program peer    |\n");
    SEND_STR("+-----------+-----------+\n");
    SEND_STR("|  prog_id  |  peer_id  |\n");
    SEND_STR("+-----------+-----------+\n");

    PROG_LIST_LOOP_ST
    FORLISTN(ICTD.peer_list, i) {
        snprintf(q, sizeof q, "|%11d|%11.11s|\n",
                item->id,
                ICTD.peer_list.item[i].id
                );
        SEND_STR(q);
    }
    PROG_LIST_LOOP_SP
    SEND_STR("+-----------+-----------+\n");

    SEND_STR("+-----------------------------------------------------------------------------------+\n")
    SEND_STR("|                             Program runtime                                       |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|    id     |   state   |check_rst_s|   r_val   |  r_state  |   r_sec   |   r_nsec  |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    PROG_LIST_LOOP_ST
            char *state = getStateStr(ICTD.state);
    struct timespec tm1 = getTimeRestTmr(ICTD.check_interval, ICTD.tmr_check);
    snprintf(q, sizeof q, "|%11d|%11.11s|%11ld|%11.3f|%11d|%11ld|%11ld|\n",
            item->id,
            state,
            tm1.tv_sec,
            ICTD.result.value,
            ICTD.result.state,
            ICTD.result.tm.tv_sec,
            ICTD.result.tm.tv_nsec
            );
    SEND_STR(q)
    PROG_LIST_LOOP_SP
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")

    SEND_STR_L("_\n")
#undef ICTD
}

void printHelp(ACPResponse * response) {
    char q[LINE_SIZE];
    SEND_STR("COMMAND LIST\n")
    snprintf(q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tload prog into RAM and start its execution; program id expected\n", ACP_CMD_PROG_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tunload program from RAM; program id expected\n", ACP_CMD_PROG_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tunload program from RAM, after that load it; program id expected\n", ACP_CMD_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tenable running program; program id expected\n", ACP_CMD_PROG_ENABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tdisable running program; program id expected\n", ACP_CMD_PROG_DISABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog runtime; program id expected\n", ACP_CMD_PROG_GET_DATA_RUNTIME);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog initial data; program id expected\n", ACP_CMD_PROG_GET_DATA_INIT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog result; program id expected\n", ACP_CMD_GET_FTS);
    SEND_STR_L(q)
}