#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <netdb.h>
#include <unistd.h>
#include <jansson.h>

#include "board.h"

#pragma region MACRO_UTIL
#define SELECT_ARITIES(_1,_2,_3,_4,FUNC,...)    FUNC
#define AMP1(a)                                 &a // conat ampersand
#define AMP2(a, args...)                        &a,AMP1(##args)
#define AMP3(a, args...)                        &a,AMP2(##args)
#define AMP4(a, args...)                        &a,AMP3(##args)
#define AMPS(args...)                           SELECT_ARITIES(##args, AMP4, AMP3, AMP2, AMP1)(##args)

#define CNT_ARGS_IMPL(_1,_2,_3,_4,N,...)        N
#define CNT_ARGS(...)                           CNT_ARGS_IMPL(##args, 4,3,2,1,0)

#ifdef DEBUG // compile with -DDEBUG
#define LOG(format, args...)                    LOGG(format, ##args)
#else
#define LOG(format, args...)                    
#endif
#define LOGG(format, args...)                   printf(format, ##args) // log globally

#define SAME_STR(x, y)                          (x != NULL && y != NULL && !strcmp((x), (y)))

#define __ALLOC_JSON(name, args...)             json name = { .data = json_pack(##args), .ref = TRUE }; if (name)
#define __ALLOC_FAIL                            else
#define __FREE_JSON(name)                       free_json(&name);
#define __FREE_JSONS(args...)                   free_jsons(CNT_ARGS(##args), AMPS(##args)) // max 4
#define IS_NULL(name)                           json_is_null(name)
#pragma endregion
#pragma region CONSTS
#define TRUE                                    1
#define FALSE                                   0
#pragma endregion
#pragma region TYPEDEF
typedef struct pollfd                           s_pollfd;
typedef struct addrinfo                         s_addrinfo;
typedef char                                    bool;
typedef struct {
    json_t*             data; // real json object
    bool                ref; // referred or not
} json;
#pragma endregion


void frees(int, ...);
void free_json(json*);
void free_jsons(int, ...);

#pragma region MAIN
int main(int argc, char* argv[]) {

    return 0;
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
#pragma endregion