#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <netdb.h>
#include <unistd.h>
#include <jansson.h>

#include "board.h"
#include "led-matrix-c.h"

#pragma region MACRO_UTIL
#define SELECT_ARITIES(_1,_2,_3,_4,FUNC,...)    FUNC
#define AMP1(a)                                 &a
#define AMP2(a, ...)                            &a,AMP1(__VA_ARGS__)
#define AMP3(a, ...)                            &a,AMP2(__VA_ARGS__)
#define AMP4(a, ...)                            &a,AMP3(__VA_ARGS__)
#define AMPS(...)                               SELECT_ARITIES(__VA_ARGS__, AMP4, AMP3, AMP2, AMP1)(__VA_ARGS__)

#define CNT_ARGS_IMPL(_1,_2,_3,_4,N,...)        N
#define CNT_ARGS(...)                           CNT_ARGS_IMPL(__VA_ARGS__, 4,3,2,1,0)

// compile with -DDEBUG
#ifdef DEBUG
#define LOG(...)                                LOGG(__VA_ARGS__)
#else
#define LOG(...)                    
#endif
#define LOGG(...)                               printf(__VA_ARGS__)
#define ERR(...)                                fprintf(stderr, __VA_ARGS__)

#define SAME_STR(x, y)                          (x != NULL && y != NULL && !strcmp((x), (y)))
#define IN_BOARD(x, y)   ((x) >= 0 && (x) < SIZE && (y) >= 0 && (y) < SIZE)

#define __ALLOC_JSON_NEW(name, ...)             json name; __ALLOC_JSON(name, __VA_ARGS__)
#define __ALLOC_JSON(name, ...)                 name.data = json_pack(__VA_ARGS__); name.ref = TRUE; if (name.data)
#define __ALLOC_FAIL                            else
#define __FREE_JSON(name)                       free_json(&name);
#define __FREE_JSONS(...)                       free_jsons(CNT_ARGS(__VA_ARGS__), AMPS(__VA_ARGS__))
#define __FREES(...)                            frees(CNT_ARGS(__VA_ARGS__), __VA_ARGS__)
#define __UNPACK_JSON(target, ...)              if (json_unpack(target.data, __VA_ARGS__) >= 0)
#define __UNPACK_FAIL                           else

#define GET_TYPE(payload)                       json_string_value(json_object_get(payload.data, "type"))
#define IS_NULL(name)                           json_is_null(name)

#define FOREACH_BOARD(i, j)                     int i, j; for (i = 0; i < SIZE; i++) for (j = 0; j < SIZE; j++)
#define FOREACH_COL(j)                          FOREACH_ROW(j)
#define FOREACH_ROW(i)                          int i; for (i = 0; i < SIZE; i++)
#define COPY_BOARD(dst, srcs)                   FOREACH_ROW(i) { memcpy(dst[i], srcs[i], 8); }
#pragma endregion
#pragma region CONSTS
#define TRUE                                    1
#define FALSE                                   0
#define true                                    TRUE
#define false                                   FALSE
#define PAYLOAD_BUFFER                          2048
#define SIZE                                    8
#pragma endregion
#pragma region TYPEDEF
typedef enum { READY, REGISTERED, WAIT_TURN, WAIT_MOVE_ACK } STATUS;
typedef enum { REGISTER, MOVE }                       PAYLOAD;
typedef struct pollfd                           s_pollfd;
typedef struct addrinfo                         s_addrinfo;
typedef char                                    BOOL;
typedef char                                    tile;
typedef struct {
    json_t*             data; // real json object
    BOOL                ref; // referred or not
} json;
typedef struct { int r1, c1, r2, c2; } Move;
#pragma endregion

Move valid_moves_player[1000];
int valid_moves_cnt_player = 0;

Move valid_moves_opp[1000];
int valid_moves_cnt_opp = 0;

void frees(int, ...);
void free_json(json*);
void free_jsons(int, ...);
BOOL execution_check(int argc, char* argv[], const char** pip, const char** pport, const char** pname);
void print_board(const tile board[SIZE][SIZE]);

BOOL set_network(const char* ip, const char* port, int* server, STATUS* status);
BOOL register_player(const int server, const char* username, STATUS* status);
BOOL game_start(const int server, const char* username, BOOL* out_is_first, STATUS* status);
BOOL process(int sockfd, const char* username, STATUS* status);
Move move_generate(const tile board[SIZE][SIZE], const tile player, const tile oppo);
Move* available_move_player(const tile board[SIZE][SIZE], const tile player);
Move* available_move_opp(const tile board[SIZE][SIZE], const tile player);

int send_stream(const int dst, char* data);
json recv_stream(const int src);

char* build_payload(PAYLOAD, ...);
char* stringfy(json*);
json json_undefined();

void handle_signal(int signum) {
    deinit_led_matrix();
    exit(0);
}

#pragma region MAIN
int main(int argc, char* argv[]) {
    const char* ip;
    const char* port;
    const char* username;

    if (!execution_check(argc, argv, &ip, &port, &username)) {
        ERR("[invalid execution format]\n");
        return 1;
    }

    int socket;
    STATUS status;

    signal(SIGINT, handle_signal);   // Ctrl+C
    signal(SIGTERM, handle_signal);  // kill 명령 등

    if (!set_network(ip, port, &socket, &status)) {
        ERR("[netset error]\n");
        return 1;
    } else if (!register_player(socket, username, &status)) {
        ERR("[register failed]\n");
        close(socket);
        return 1;
    } else if (!process(socket, username, &status)) {
        close(socket);
        return 1;
    }
    
    close(socket);
    return 0;
}
#pragma endregion

BOOL set_network(const char* ip, const char* port, int* server, STATUS* game_status) {
    s_addrinfo hints, *res;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(ip, port, &hints, &res);
    if (status != 0) {
        return FALSE;
    }

    *server = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (*server == -1) {
        return FALSE;
    }

    status = connect(*server, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        return FALSE;
    }

    *game_status = READY;
    freeaddrinfo(res);
    return TRUE;
}
BOOL register_player(const int server, const char* username, STATUS* status) {
    if (*status != READY)
        return FALSE;

    BOOL ret = FALSE;
    char* payload = build_payload(REGISTER, username);

    int len = send_stream(server, payload);
    if (len) {
        // recv register_ack
        char* type = NULL;
        json resp = recv_stream(server);
        
        __UNPACK_JSON(resp, "{s:s}", "type", &type) {
            ret = TRUE;
            LOGG("[registered]\n");
            *status = REGISTERED;
        }
        __FREE_JSON(resp);
    }

    // err or register_nack
    *status = REGISTERED;
    free(payload);
    return ret;
}
BOOL game_start(const int server, const char* username, BOOL* out_is_first, STATUS* status) {
    if (*status != REGISTERED)
        return FALSE;

    BOOL ret = FALSE;
    char *type = NULL, *first_player = NULL;
    char* players[2];
    json resp = recv_stream(server);
    __UNPACK_JSON(resp, "{s:s, s:s, s:[s, s]}", "type", &type, "first_player", &first_player, "players", &players[0], &players[1]) {
        LOGG("[game start]\n");
        ret = TRUE;
        if (SAME_STR(players[0], username))
            *out_is_first = TRUE;
        else
            *out_is_first = FALSE;
    }

    *status = WAIT_TURN;
    __FREE_JSON(resp);
    return ret;
}
BOOL process(int sockfd, const char* username, STATUS* status) {
    BOOL is_first;
    s_pollfd server = { .fd = sockfd, .events = POLLIN };

    if (game_start(sockfd, username, &is_first, status)) {
        tile board[SIZE][SIZE];
        const tile cur = is_first ? 'R' : 'B';
        const tile oppo = is_first ? 'B' : 'R';
        while (1) {
            int ready = poll(&server, 1, -1);
            if (ready < 0) {
                return FALSE;
            }

            json resp = recv_stream(server.fd);
            const char* type = GET_TYPE(resp);

            if (!type) {
                ERR("[network error]\n");
                return FALSE;
            } else if (SAME_STR(type, "your_turn")) {
                LOGG("[my turn: %s]\n", username);
                char* row[8];
                double timeout;

                __UNPACK_JSON(resp, "{s:s, s:[s,s,s,s,s,s,s,s], s:f}", "type", &type, "board", &row[0], &row[1], &row[2], &row[3], &row[4], &row[5], &row[6], &row[7], "timeout", &timeout) {
                    COPY_BOARD(board, row);
                    clock_t st = clock();
                    Move new_move = move_generate(board, cur, oppo);
                    clock_t en = clock();

                    char* move_payload = build_payload(MOVE, username, new_move.c1, new_move.r1, new_move.c2, new_move.r2);
                    if ((double)(en - st) / CLOCKS_PER_SEC <= timeout && move_payload) {
                        LOGG("[move from %d %d to %d %d]\n", new_move.c1, new_move.r1, new_move.c2, new_move.r2);
                        send_stream(server.fd, move_payload);
                    }
                    free(move_payload);
                }
                *status = WAIT_MOVE_ACK;
            }else {
                if (SAME_STR(type, "move_ok")) {
                    char* row[8];
                    char* dummy;
                    LOG("move accepted\n");

                    __UNPACK_JSON(resp, "{s:s, s:[s,s,s,s,s,s,s,s], s:s}", "type", &type, "board", &row[0], &row[1], &row[2], &row[3], &row[4], &row[5], &row[6], &row[7], "next_player", &dummy) {
                        COPY_BOARD(board, row);
                        print_board(board);
                    }
                } else if (SAME_STR(type, "invalid_move")) {
                    char* row[8];
                    char* dummy;
                    LOG("move rejected\n");

                    __UNPACK_JSON(resp, "{s:s, s:[s,s,s,s,s,s,s,s], s:s}", "type", &type, "board", &row[0], &row[1], &row[2], &row[3], &row[4], &row[5], &row[6], &row[7], "next_player", &dummy) {
                        COPY_BOARD(board, row);
                        print_board(board);
                    } __UNPACK_FAIL {
                        __UNPACK_JSON(resp, "{s:s, s:s}", "type", &type, "reason", &dummy);
                    }
                } else if (SAME_STR(type, "pass")) {
                    char* dummy;
                    LOG("passed\n");

                    __UNPACK_JSON(resp, "{s:s, s:s}", "type", &type, "next_player", &dummy) {
                        LOGG("[turn passed]\n");
                    }
                }
                *status = WAIT_TURN;
            }
            if (SAME_STR(type, "game_over")) {
                json_t* user = json_object_get(resp.data, "scores");
                if (IS_NULL(user) || !user) {
                    ERR("cannot find scores\n");
                    break;
                }
                int score = json_integer_value(json_object_get(user, username));

                LOGG("==========Game Over===========\n");
                print_board(board);
                LOGG("%s's score: %d\n", username, score);
                __FREE_JSON(resp);
                break; // unpack 실패해도 일단 종료
            }

            __FREE_JSON(resp);
        }
    } else {
        ERR("[game error]\n");
        return FALSE; // abnormal exit
    }

    return TRUE; // normal exit
}
Move move_generate(const tile board[SIZE][SIZE], const tile player, const tile oppo) {
    Move new_move = { .r1 = 0, .c1 = 0, .r2 = 0, .c2 = 0 };

    // return new_move;
    Move* player_valid_moves = available_move_player(board, player);
    Move* opp_valid_moves = available_move_opp(board, oppo);
    if (player_valid_moves == NULL) {
        return new_move;
    }


    // 후보 중 최대의 효율을 가지는 수 찾기
    // 전략들 (추가 제거 가능)
    // 현재 전략 : 1. 상대의 말을 많이 뒤집을 수 있는 수
    //         2. 내 말을 알박기할 수 있고 상대의 말을 알박기하게 두지 않는 칸. (더 이상 뒤집힐 수 없는 칸) --(나중에 구현)
    //         3. 상대가 행동할 때, 내가 둔 곳을 다시 차지할 위협이 적은 칸. (이동한 곳 주변에 내 말이 많고 상대가 그 곳에 말을 두었을 때, 내 말이 많이 뺏기는 칸) 설령 내 말을 뺏기더라도 다시 가져올 수 있으면 ㄱㅊ(주변 칸의 개수가 짝수일 경우)
    //         4. 내 차례에 마지막 수를 둘 수 있는지(보드 전체의 남은 칸이 짝수이면 jump, 홀수이면 clone) -- 일단 배제
    //         5. 추가적인 전략을 추가할 수 있고, 제거할 수도 있게 설계한다. (위 전략을 합쳐서 최적의 수를 도출하는 구조.)
    Move best_move = {-1, -1, -1, -1};
    int max_score = -1000;
    int score[1000];
    for (int i = 0; i < valid_moves_cnt_player; i++) {
        score[i] = 0; // 초기화
    }
    for (int i = 0; i < valid_moves_cnt_player; i++) {
    Move* m = &player_valid_moves[i];
        int r1 = m->r1, c1 = m->c1, r2 = m->r2, c2 = m->c2;

        // 이번 행동의 이동
        int distance_r = abs(r2 - r1);
        int distance_c = abs(c2 - c1);
        int pres_move = -1; // 현재 이동 방식 (0 = clone, 1 = jump)

        // 중앙집권체제
        int center_score = 0;
        center_score += 3.5 - abs(r2 - 3.5) + 3.5 - abs(c2 - 3.5); // 0~7점


        if ((distance_r == 2 || distance_r == 0) && (distance_c == 2 || distance_c == 0)){
            pres_move = 1; // jump 전략
        }
        else if (distance_r <= 1 && distance_c <= 1) {
            pres_move = 0; // clone 전략
        } else {
            continue; // 유효하지 않은 이동은 무시
        }

        // 상대의 말을 많이 뒤집을 수 있는 수
        int flips = 0;
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = r2 + dr, nc = c2 + dc; // 이동한 칸 주변
                if (!IN_BOARD(nr, nc)) continue;
                if (board[nr][nc] == oppo) {
                    flips++;
                }
            }
        }
    
        int threats = 0; // 상대의 위협이 있는 수
        // 상대가 행동할 때, 내가 둔 곳을 차지할 위협이 적은 칸인지 확인
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = r2 + dr, nc = c2 + dc; // 이동한 칸 주변
                // 이동한 칸 주변에 내 말이 많고 상대가 그 곳에 말을 두었을 때, 내 말이 많이 뺏기는 칸인지 확인
                if (!IN_BOARD(nr, nc)) continue;
                BOOL found = false;
                for (int k = 0; k < valid_moves_cnt_opp; k++) {
                    Move* opp_move = &opp_valid_moves[k];
                    if (opp_move->r2 == nr && opp_move->c2 == nc) {
                        found = true;
                        break;
                    }
                }
                if (!found) continue; // 상대의 유효한 이동에 포함되지 않는지 확인
                else threats ++;
                for (int ddr = -1; ddr <= 1; ddr++) {
                    for (int ddc = -1; ddc <= 1; ddc++) {
                        if (ddr == 0 && ddc == 0) continue;
                        int nnr = nr + ddr, nnc = nc + ddc; // 주변 칸
                        if (!IN_BOARD(nnr, nnc)) continue;
                        if (nnr == r2 && nnc == c2) continue; // 이동한 칸은 제외
                        if (board[nnr][nnc] == player) {
                            threats ++; // 내 말을 뒤집을 수 있는 칸이 있다면 점수 감소
                        }
                    }
                }
            }
        }
        // 내가 마지막 수를 둘 수 있는지 확인
        int remaining_moves = 0; // 남은 칸의 개수
        for (int r = 0; r < SIZE; r++) {
            for (int c = 0; c < SIZE; c++) {
                if (board[r][c] == '.') {
                    remaining_moves++;
                }
            }
        }
        if (remaining_moves <= 3){    
            if (remaining_moves % 2 == 0) {
                // 남은 칸이 짝수이면 jump 전략
                if (pres_move == 0) {
                    // 현재 수가 clone 전략이면 점수 감소
                    score[i] -= 0; // jump 전략으로 점수 감소
                }
                else if (pres_move == 1) {
                    // 현재 수가 jump 전략이면 점수 증가
                    score[i] += 10; // jump 전략으로 점수 증가
                }
            } else {
                // 남은 칸이 홀수이면 clone 전략
                if (pres_move == 1) {
                    // 현재 수가 jump 전략이면 점수 감소
                    score[i] -= 0; // clone 전략으로 점수 감소
                }
                else if (pres_move == 0) {
                    // 현재 수가 clone 전략이면 점수 증가
                    score[i] += 10; // clone 전략으로 점수 증가
                }
            }
        }
        // 점수 계산
        int a,b,c,d;
        a = 1;
        b = 2;
        c = 1;
        score[i] += a * center_score + b * flips - c * threats;
        // 최대의 효율을 가지는 수 찾기

        if (score[i] > max_score) {
            max_score = score[i];
            best_move = *m; // 현재 수가 가장 좋은 수로 업데이트
        }
    }
    
    new_move.c1 = best_move.r1 + 1;
    new_move.r1 = best_move.c1 + 1;
    new_move.c2 = best_move.r2 + 1;
    new_move.r2 = best_move.c2 + 1;

    return new_move;
}
Move* available_move_player(const tile board[SIZE][SIZE], const BOOL player) {
    // 초기화
    valid_moves_cnt_player = 0;

    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] != player) continue;
            
            // 가능한 수를 저장
            for (int dr = -2; dr <= 2; dr++) {
                for (int dc = -2; dc <= 2; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr, nc = c + dc;
                    if (!IN_BOARD(nr, nc)) continue;
                    if (board[nr][nc] != '.') continue;
                    int adx = abs(dr), ady = abs(dc);
                    if ((adx <= 1 && ady <= 1 && (adx != 0 || ady != 0)) ||
                        (adx == 2 && ady == 0) ||
                        (adx == 0 && ady == 2) ||
                        (adx == 2 && ady == 2)) {
                        valid_moves_player[valid_moves_cnt_player++] = (Move){r, c, nr, nc};
                    }
                }
            }
        }
    }
    // printf("%d\n", valid_moves_cnt_player);
    return valid_moves_player;
}
Move* available_move_opp(const tile board[SIZE][SIZE], const BOOL player) {
    // 초기화
    valid_moves_cnt_opp = 0;

    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] != player) continue;
            
            // 가능한 수를 저장
            for (int dr = -2; dr <= 2; dr++) {
                for (int dc = -2; dc <= 2; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr, nc = c + dc;
                    if (!IN_BOARD(nr, nc)) continue;
                    if (board[nr][nc] != '.') continue;
                    int adx = abs(dr), ady = abs(dc);
                    if ((adx <= 1 && ady <= 1 && (adx != 0 || ady != 0)) ||
                        (adx == 2 && ady == 0) ||
                        (adx == 0 && ady == 2) ||
                        (adx == 2 && ady == 2)) {
                        valid_moves_opp[valid_moves_cnt_opp++] = (Move){r, c, nr, nc};
                    }
                }
            }
        }
    }
    // printf("%d\n", valid_moves_cnt_opp);
    return valid_moves_opp;
}
#pragma region FUNC_SOCKET
int send_stream(const int dst, char* data) {
    size_t total = strlen(data); 
    ssize_t remain = (ssize_t)total;
    ssize_t sent = 0;

    // 아무것도 안 가는 것도 오류로 봄
    while (remain > 0) {
        ssize_t n = send(dst, data + sent, remain, 0);
        if (n <= 0) { // err
            send(dst, "\n", 2, 0); // 오류 전파를 막기 위해 개행문자 삽입
            return 0;
        }
        sent += n;
        remain -= n;
    }

    return sent;
}
json recv_stream(const int src) // string -> json
{
    char buffer[PAYLOAD_BUFFER];

    char *nl;
    int n = recv(src, buffer, PAYLOAD_BUFFER, MSG_PEEK);
    if (n <= 0)
        return json_undefined();
    LOG("n = %d\n", n);
    buffer[n] = '\0';
    
    nl = strchr(buffer, '\n');
    if (!nl)
        return json_undefined();

    n = recv(src, buffer, nl - buffer + 1, 0);
    if (n <= 0) 
        return json_undefined();

    *nl = '\0';

    json_error_t error;
    json payload_json = { .data = json_loads(buffer, 0, &error), .ref = TRUE };
    if (!payload_json.data) {
        ERR("%s (line %d)\n", error.text, error.line);
        return json_undefined();
    }
    
    return payload_json;
}
#pragma endregion

#pragma region FUNC_JSON
char* build_payload(PAYLOAD code, ...){
    va_list ap;
    va_start(ap, code);

    char* payload_string;

    json payload;
    switch (code) {
        case REGISTER:
            __ALLOC_JSON(payload, "{s:s, s:s}", "type", "register", "username", va_arg(ap, const char*));
            __ALLOC_FAIL {
                va_end(ap);
                return NULL;
            }
        break;
        case MOVE:
            {
                const char* username = va_arg(ap, const char*);
                int sx = va_arg(ap, int);
                int sy = va_arg(ap, int);
                int tx = va_arg(ap, int);
                int ty = va_arg(ap, int);
                __ALLOC_JSON(payload, "{s:s, s:s, s:i, s:i, s:i, s:i}", "type", "move", "username", username, "sx", sx, "sy", sy, "tx", tx, "ty", ty);
                __ALLOC_FAIL {
                    va_end(ap);
                    return NULL;
                }
            }
        break;
    }
    payload_string = stringfy(&payload);

    va_end(ap);
    __FREE_JSON(payload);
    return payload_string;
}
char* stringfy(json* payload) {
    if (!payload->ref || IS_NULL(payload->data))
        return NULL;
        
    char* str = json_dumps(payload->data, JSON_COMPACT); // NULL on error

    if (!str) {
        return NULL;
    }
    size_t len = strlen(str);
    char* with_newline = (char*)malloc(len + 2);
    if (with_newline) {
        memcpy(with_newline, str, len);
        with_newline[len] = '\n';
        with_newline[len + 1] = '\0';
    }

    free(str);
    return with_newline;
}
json json_undefined() {
    json null_res = { .data = json_null(), .ref = FALSE };
    return null_res;
}
#pragma endregion

#pragma region FUNC_UTIL
void frees(int num, ...) {
    va_list ap;
    va_start(ap, num);

    int i;
    for (i = 0; i < num; i++) {
        void* ptr = va_arg(ap, void*);
        free(ptr); // no needed nullptr check
    }
    va_end(ap);
}
void free_json(json* ptr) {
    if (ptr->ref && !IS_NULL(ptr->data)) {
        json_decref(ptr->data); // no needed nullptr check
    }
}
void free_jsons(int num, ...) {
    va_list ap;
    va_start(ap, num);

    int i;
    for (i = 0; i < num; i++) {
        json* ptr = va_arg(ap, json*);
        free_json(ptr);
    }
    va_end(ap);
}
BOOL execution_check(int argc, char* argv[], const char** pip, const char** pport, const char** pname) {
    // argv[0]: program name
    if (argc != 7)
        return FALSE;
    
    int i;
    for (i = 1; i < 7; i++) {
        const char* param = argv[i];
        if (SAME_STR(param, "-ip")) {
            *pip = argv[++i];
        } else if (SAME_STR(param, "-port")) {
            *pport = argv[++i];
        } else if (SAME_STR(param, "-username")) {
            *pname = argv[++i];
        }
    }
    
    return TRUE;
}
void print_board(const tile board[SIZE][SIZE]) {
    FOREACH_ROW(i) {
        FOREACH_COL(j) {
            putchar(board[i][j]);
        }
        printf("\n");
    }

    render(board);
}
#pragma endregion