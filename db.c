
#include "main.h"

int addProg(Prog *item, ProgList *list) {
    if (list->length >= INT_MAX) {
#ifdef MODE_DEBUG
        fprintf(stderr, "addProg: ERROR: can not load prog with id=%d - list length exceeded\n", item->id);
#endif
        return 0;
    }
    if (list->top == NULL) {
        lockProgList();
        list->top = item;
        unlockProgList();
    } else {
        lockMutex(&list->last->mutex);
        list->last->next = item;
        unlockMutex(&list->last->mutex);
    }
    list->last = item;
    list->length++;
#ifdef MODE_DEBUG
    printf("%s(): prog with id=%d loaded\n", F, item->id);
#endif
    return 1;
}

int addProgById(int prog_id, ProgList *list, PeerList *peer_list, sqlite3 *db_data, const char *db_data_path) {
    Prog *rprog = getProgById(prog_id, list);
    if (rprog != NULL) {//program is already running
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): program with id = %d is being controlled by program\n", F, rprog->id);
#endif
        return 0;
    }

    Prog *item = malloc(sizeof *(item));
    if (item == NULL) {
        fprintf(stderr, "%s(): failed to allocate memory\n", F);
        return 0;
    }
    memset(item, 0, sizeof *item);
    item->id = prog_id;
    item->next = NULL;

    item->cycle_duration = cycle_duration;

    if (!initMutex(&item->mutex)) {
        free(item);
        return 0;
    }
    if (!initMutex(&item->control_data.result_mutex)) {
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!initClient(&item->sock_fd, WAIT_RESP_TIMEOUT)) {
        freeMutex(&item->control_data.result_mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!getProgByIdFDB(item->id, item, peer_list, db_data, db_data_path)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->control_data.result_mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }

    FORLISTN(item->control_data.peer_list, i) {
        item->control_data.peer_list.item[i].fd = &item->sock_fd;
    }

    if (!checkProg(item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->control_data.result_mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!addProg(item, list)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->control_data.result_mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!createMThread(&item->thread, &threadFunction, item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->control_data.result_mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    return 1;
}

int deleteProgById(int id, ProgList *list, char *db_data_path) {
#ifdef MODE_DEBUG
    printf("prog to delete: %d\n", id);
#endif
    Prog *prev = NULL, *curr;
    int done = 0;
    curr = list->top;
    while (curr != NULL) {
        if (curr->id == id) {
            if (prev != NULL) {
                lockMutex(&prev->mutex);
                prev->next = curr->next;
                unlockMutex(&prev->mutex);
            } else {//curr=top
                lockProgList();
                list->top = curr->next;
                unlockProgList();
            }
            if (curr == list->last) {
                list->last = prev;
            }
            list->length--;
            stopProgThread(curr);
            db_saveTableFieldInt("prog", "load", curr->id, 0, NULL, db_data_path);
            freeProg(curr);
#ifdef MODE_DEBUG
            printf("prog with id: %d deleted from prog_list\n", id);
#endif
            done = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    return done;
}

int loadActiveProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData *data = d;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            int id = atoi(DB_COLUMN_VALUE);
            addProgById(id, data->prog_list, data->peer_list, data->db_data, NULL);
        } else {
            fprintf(stderr, "%s(): unknown column: %s\n", F, DB_COLUMN_NAME);
        }
    }
    return EXIT_SUCCESS;
}

int loadActiveProg(ProgList *list, PeerList *peer_list, char *db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    ProgData data = {.prog_list = list, .peer_list = peer_list, .db_data = db};
    char *q = "select id from prog where load=1";
    if (!db_exec(db, q, loadActiveProg_callback, &data)) {
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}

int getProgPeerList_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData * data = d;
    Prog *item = data->prog;
    int c = 0;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("peer_id")) {
            Peer *peer = getPeerById(DB_COLUMN_VALUE, data->peer_list);
            if (peer == NULL) {
#ifdef MODE_DEBUG
                fprintf(stderr, "%s(): peer %s not found\n", F, DB_COLUMN_VALUE);
#endif
                return EXIT_FAILURE;
            }
            item->control_data.peer_list.item[item->control_data.peer_list.length] = *peer;
            c++;
        } else {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): unknown column (we will skip it): %s\n", F, DB_COLUMN_NAME);
#endif
        }
    }
#define N 1
    if (c != N) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): required %d columns but %d found\n", F, N, c);
#endif
        return EXIT_FAILURE;
    }
#undef N
    item->control_data.peer_list.length++;
    return EXIT_SUCCESS;
}

static int getProgPeerList(ProgData *data) {
    PeerList *list = &data->prog->control_data.peer_list;
    RESET_LIST(list);
    char q[LINE_SIZE];
    int n = 0;
    snprintf(q, sizeof q, "select count(*) from prog_peer where prog_id=%d", data->prog->id);
    db_getInt(&n, data->db_data, q);
    if (n <= 0) {
        return 1;
    }
    RESIZE_M_LIST(list, n);
    if (list->max_length != n) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed to resize prog peer_list (%d!=%d) where prog_id=%d\n", F, list->max_length, n, data->prog->id);
#endif
        return 0;
    }
    snprintf(q, sizeof q, "select peer_id from prog_peer where prog_id=%d", data->prog->id);
    if (!db_exec(data->db_data, q, getProgPeerList_callback, data)) {
        return 0;
    }
    if (list->max_length != list->length) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed to fill prog peer_list (%d!=%d) where prog_id=%d\n", F, list->max_length, list->length, data->prog->id);
#endif
        return 0;
    }
    return 1;
}

static int getProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData * data = d;
    Prog *item = data->prog;
    int load = 0, enable = 0;
    int c = 0;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            item->id = atoi(DB_COLUMN_VALUE);
            c++;
        } else if (DB_COLUMN_IS("check_interval_sec")) {
            item->control_data.check_interval.tv_nsec = 0;
            item->control_data.check_interval.tv_sec = atoi(DB_COLUMN_VALUE);
            c++;
        } else if (DB_COLUMN_IS("enable")) {
            enable = atoi(DB_COLUMN_VALUE);
            c++;
        } else if (DB_COLUMN_IS("load")) {
            load = atoi(DB_COLUMN_VALUE);
            c++;
        } else {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): unknown column (we will skip it): %s\n", F, DB_COLUMN_NAME);
#endif
        }
    }
#define N 4
    if (c != N) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): required %d columns but %d found\n", F, N, c);
#endif
        return EXIT_FAILURE;
    }
#undef N
    if (!getProgPeerList(data)) {
        return EXIT_FAILURE;
    }
    if (enable) {
        item->control_data.state = INIT;
    } else {
        item->control_data.state = DISABLE;
    }
    if (!load) {
        db_saveTableFieldInt("prog", "load", item->id, 1, data->db_data, NULL);
    }
    return EXIT_SUCCESS;
}

int getProgByIdFDB(int prog_id, Prog *item, PeerList *peer_list, sqlite3 *dbl, const char *db_path) {
    if (dbl != NULL && db_path != NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): dbl xor db_path expected\n", F);
#endif
        return 0;
    }
    sqlite3 *db;
    int close = 0;
    if (db_path != NULL) {
        if (!db_open(db_path, &db)) {
            return 0;
        }
        close = 1;
    } else {
        db = dbl;
    }
    char q[LINE_SIZE];
    ProgData data = {.peer_list = peer_list, .prog = item, .db_data = db};
    snprintf(q, sizeof q, "select * from prog where id=%d", prog_id);
    if (!db_exec(db, q, getProg_callback, &data)) {
        if (close)sqlite3_close(db);
        return 0;
    }
    if (close)sqlite3_close(db);
    return 1;
}
