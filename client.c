#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <netdb.h>
#include <unistd.h>
#include <jansson.h>

#include "board.h"

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
#define IN_BOUNDS(x, y)   ((x) >= 0 && (x) < SIZE && (y) >= 0 && (y) < SIZE)

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
typedef struct Move {
    int r1, c1;   // 출발 좌표 (0-based)
    int r2, c2;   // 도착 좌표 (0-based)
    char type;    // 'C' = Clone, 'J' = Jump
} Move;
typedef struct {
    int idx;
    int score;
} ScoredMove;
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
void get_all_valid_moves(const tile board[SIZE][SIZE], char player, Move moves[], int *move_count);
int  count_flipped(const tile board[SIZE][SIZE], Move m, char player);
int is_corner_adjacent(int r, int c);
int score_piece_count(const tile board[SIZE][SIZE], char player);
void simulate_move(const tile src_board[SIZE][SIZE], Move m, char player, tile dest_board[SIZE][SIZE]);

int send_stream(const int dst, char* data);
json recv_stream(const int src);

char* build_payload(PAYLOAD, ...);
char* stringfy(json*);
json json_undefined();

#ifndef CLIENT_ONLY
void handle_signal(int signum) {
    deinit_led_matrix();
    exit(0);
}
#endif

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

    #ifndef CLIENT_ONLY
        signal(SIGINT, handle_signal);   // Ctrl+C
        signal(SIGTERM, handle_signal);  // kill 명령 등
    #endif

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
    #ifndef CLIENT_ONLY
        deinit_led_matrix();
    #endif
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

        #ifndef CLIENT_ONLY
            init_led_panel();
        #endif
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

    Move moves[1000];
    int move_cnt = 0;
    
    // 1) 합법 수 모두 수집
    get_all_valid_moves(board, player, moves, &move_cnt);

    // 2) 둘 수 없으면 PASS
    if (move_cnt == 0) {
        return new_move;
    }

    ScoredMove scored[1000];
    for (int i = 0; i < move_cnt; i++) {
        Move m = moves[i];
        int flips = count_flipped(board, m, player);
        int corner_penalty = is_corner_adjacent(m.r2, m.c2) ? 1 : 0;
        float dr = fabsf((float)m.r2 - 3.5f);
        float dc = fabsf((float)m.c2 - 3.5f);
        float dist_center = dr + dc;
        int score = flips * 1000 - corner_penalty * 500 - (int)(dist_center * 10.0f);
        scored[i].idx = i;
        scored[i].score = score;
    }

    // 4) 1차 점수로 내림차순 정렬하여 상위 N개만 선정
    int N = (move_cnt < 30 ? move_cnt : 30);
    // 간단히 선택 정렬로 상위 N 추출 (move_cnt 최대 1000이므로 비용 무시)
    for (int i = 0; i < N; i++) {
        int best_j = i;
        for (int j = i + 1; j < move_cnt; j++) {
            if (scored[j].score > scored[best_j].score) {
                best_j = j;
            }
        }
        // swap
        ScoredMove tmp = scored[i];
        scored[i] = scored[best_j];
        scored[best_j] = tmp;
    }

    // 5) 상위 N개 후보 각각에 대해 “단계 2” 시뮬레이션:
    //    (A) 나의 후보 수를 보드에 적용 → b1
    //    (B) 상대편의 그리디 수(move_generate 로직과 동일)를 b1에 적용 → b2
    //    (C) b2에서의 score_piece_count(player) 계산 → 최종 평가
    int best_idx = scored[0].idx;
    int best_eval = INT_MIN;

    for (int k = 0; k < N; k++) {
        int i = scored[k].idx;
        Move m = moves[i];

        // (A) 내 수 적용
        char board1[SIZE][SIZE];
        simulate_move(board, m, player, board1);

        // (B) 상대편 그리디 수 찾기
        Move opp_moves[1000];
        int opp_move_cnt = 0;
        get_all_valid_moves(board1, oppo, opp_moves, &opp_move_cnt);

        if (opp_move_cnt == 0) {
            // 상대가 둘 수 없으면 바로 평가
            int eval = score_piece_count(board1, player);
            if (eval > best_eval) {
                best_eval = eval;
                best_idx = i;
            }
            continue;
        }

        // 상대편 그리디: flips*1000 − corner_penalty*500 − dist_center*10
        int opp_best_j = 0;
        int opp_best_score = INT_MIN;
        for (int j = 0; j < opp_move_cnt; j++) {
            Move om = opp_moves[j];
            int flips2 = count_flipped(board1, om, oppo);
            int corner_pen2 = is_corner_adjacent(om.r2, om.c2) ? 1 : 0;
            float dr2 = fabsf((float)om.r2 - 3.5f);
            float dc2 = fabsf((float)om.c2 - 3.5f);
            float dist_center2 = dr2 + dc2;
            int score2 = flips2 * 1000 - corner_pen2 * 500 - (int)(dist_center2 * 10.0f);
            if (score2 > opp_best_score) {
                opp_best_score = score2;
                opp_best_j = j;
            }
        }

        // (C) 상대 수 적용
        char board2[SIZE][SIZE];
        simulate_move(board1, opp_moves[opp_best_j], oppo, board2);

        // (D) 최종 평가: 말 개수 차이
        int eval = score_piece_count(board2, player);
        if (eval > best_eval) {
            best_eval = eval;
            best_idx = i;
        }
    }
    
    new_move.c1 = moves[best_idx].r1 + 1;
    new_move.r1 = moves[best_idx].c1 + 1;
    new_move.c2 = moves[best_idx].r2 + 1;
    new_move.r2 = moves[best_idx].c2 + 1;

    return new_move;
}
// (2) get_all_valid_moves: 모든 유효 Move(C or J) 수집
void get_all_valid_moves(const tile board[SIZE][SIZE], char player,
                        Move moves[], int *move_count) {
    int cnt = 0;
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] != player) continue;

            // (A) Clone (거리 1)
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr;
                    int nc = c + dc;
                    if (!IN_BOUNDS(nr, nc)) continue;
                    if (board[nr][nc] != '.') continue;
                    if (cnt < 1000) {
                        moves[cnt].r1 = r;
                        moves[cnt].c1 = c;
                        moves[cnt].r2 = nr;
                        moves[cnt].c2 = nc;
                        moves[cnt].type = 'C';
                        cnt++;
                    }
                }
            }

            // (B) Jump (거리 2: 대각선 포함)
            for (int dr = -2; dr <= 2; dr++) {
                for (int dc = -2; dc <= 2; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int adr = abs(dr), adc = abs(dc);
                    if ((adr == 2 && adc == 0) ||
                        (adr == 0 && adc == 2) ||
                        (adr == 2 && adc == 2)) {
                        int nr = r + dr;
                        int nc = c + dc;
                        if (!IN_BOUNDS(nr, nc)) continue;
                        if (board[nr][nc] != '.') continue;
                        if (cnt < 1000) {
                            moves[cnt].r1 = r;
                            moves[cnt].c1 = c;
                            moves[cnt].r2 = nr;
                            moves[cnt].c2 = nc;
                            moves[cnt].type = 'J';
                            cnt++;
                        }
                    }
                }
            }
        }
    }
    *move_count = cnt;
}
int count_flipped(const tile board[SIZE][SIZE], Move m, char player) {
    int count = 0;
    char opp = (player == 'R' ? 'B' : 'R');
    int dr = m.r2, dc = m.c2;

    for (int ddr = -1; ddr <= 1; ddr++) {
        for (int ddc = -1; ddc <= 1; ddc++) {
            if (ddr == 0 && ddc == 0) continue;
            int nr = dr + ddr;
            int nc = dc + ddc;
            if (!IN_BOUNDS(nr, nc)) continue;
            if (board[nr][nc] == opp) count++;
        }
    }
    return count;
}
int is_corner_adjacent(int r, int c) {
    if ((r == 0 && c == 1) || (r == 1 && c == 0) || (r == 1 && c == 1)) return 1;
    if ((r == 0 && c == 6) || (r == 1 && c == 7) || (r == 1 && c == 6)) return 1;
    if ((r == 6 && c == 0) || (r == 7 && c == 1) || (r == 6 && c == 1)) return 1;
    if ((r == 6 && c == 7) || (r == 7 && c == 6) || (r == 6 && c == 6)) return 1;
    return 0;
}
int score_piece_count(const tile board[SIZE][SIZE], char player) {
    int mycnt = 0, oppcnt = 0;
    char opp = (player == 'R' ? 'B' : 'R');
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] == player)       mycnt++;
            else if (board[r][c] == opp)     oppcnt++;
        }
    }
    return mycnt - oppcnt;
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
void simulate_move(const tile src_board[SIZE][SIZE], Move m,
                    char player, tile dest_board[SIZE][SIZE]) {
    // 보드 복사
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            dest_board[i][j] = src_board[i][j];
        }
    }

    int sr = m.r1, sc = m.c1;
    int dr = m.r2, dc = m.c2;

    if (m.type == 'C') {
        // Clone: 출발 지점 남기고 도착 지점에 생성
        dest_board[dr][dc] = player;
    } else {
        // Jump: 출발 지점 말 제거, 도착 지점에 생성
        dest_board[sr][sc] = '.';
        dest_board[dr][dc] = player;
    }

    // Flip: 도착 지점 주변 8방향 상대 말을 뒤집음
    char opp = (player == 'R' ? 'B' : 'R');
    for (int ddr = -1; ddr <= 1; ddr++) {
        for (int ddc = -1; ddc <= 1; ddc++) {
            if (ddr == 0 && ddc == 0) continue;
            int nr = dr + ddr;
            int nc = dc + ddc;
            if (!IN_BOUNDS(nr, nc)) continue;
            if (dest_board[nr][nc] == opp) {
                dest_board[nr][nc] = player;
            }
        }
    }
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

    #ifndef CLIENT_ONLY
        render(board);
    #endif
}
#pragma endregion