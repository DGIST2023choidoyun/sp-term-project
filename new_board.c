#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "board.h"         // extern char board[8][8];
#include "led-matrix-c.h"  // LED 매트릭스 제어 함수들

#define BOARD_H
#define BOARD_SIZE 8
extern char board[BOARD_SIZE][BOARD_SIZE];



int main(void) {
    // 1) 표준 입력에서 8줄을 읽어서 board[8][8]에 저장
    //    각 줄은 반드시 8글자(문자열)로 이루어져야 하며, 뒤에 '\n'이 따라올 수 있음.
    char line[16];  // 최대 8글자 + 개행 + 널문자 여유 두고 선언
    for (int row = 0; row < BOARD_SIZE; ++row) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            fprintf(stderr, "입력 줄을 읽는 데 실패했습니다. (row=%d)\n", row);
            return EXIT_FAILURE;
        }
        // 첫 8글자만 사용 ('.','R','B' 중 하나)
        for (int col = 0; col < BOARD_SIZE; ++col) {
            board[row][col] = line[col];
        }
    }

    // 2) 8×8 LED 매트릭스 초기화
    struct LedMatrix* mtx = led_matrix_create(BOARD_SIZE, BOARD_SIZE);
    if (mtx == NULL) {
        fprintf(stderr, "LED 매트릭스 초기화 실패\n");
        return EXIT_FAILURE;
    }

    // 3) board[][] 값에 따라 색상을 지정해서 픽셀 세팅
    //    board[row][col] == '.': 검정(0,0,0)
    //                    'R': 빨강(255,0,0)
    //                    'B': 파랑(0,0,255)
    for (int row = 0; row < BOARD_SIZE; ++row) {
        for (int col = 0; col < BOARD_SIZE; ++col) {
            char ch = board[row][col];
            switch (ch) {
                case 'R':
                    // 빨간색
                    led_matrix_set_pixel(mtx, col, row, 255, 0, 0);
                    break;
                case 'B':
                    // 파란색
                    led_matrix_set_pixel(mtx, col, row, 0, 0, 255);
                    break;
                case '.':
                default:
                    // 검정(꺼짐)
                    led_matrix_set_pixel(mtx, col, row, 0, 0, 0);
                    break;
            }
        }
    }

    // 4) 버퍼에 채운 픽셀 정보를 LED 매트릭스에 한 번 전송
    led_matrix_update(mtx);

    // (선택) 화면을 잠시 유지
    sleep(1);

    // 5) 매트릭스 리소스 해제
    led_matrix_destroy(mtx);

    return EXIT_SUCCESS;
}
