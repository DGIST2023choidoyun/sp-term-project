// ────────────────────────────────────────────────────────────────────────────
// board.h
// ────────────────────────────────────────────────────────────────────────────
//
// 8×8 텍스트 보드를 Raspberry Pi RGB LED 매트릭스(예: 32×32 패널)에
// 그리는 간단한 C API.
//
// 라이브러리 모드(BOARD_STANDALONE 미정의): init_led_matrix() → display → deinit.
// Standalone 모드(BOARD_STANDALONE 정의 시): stdin으로 8×8 텍스트를 읽어서 LED/터미널 출력.
// 
// 컴파일 시: 
//   (1) #include "rpi-rgb-led-matrix-c.h" 가 필요하므로 rpi-rgb-led-matrix 패키지가 
//       -I 옵션으로 포함되어 있어야 함.
//   (2) 최종 링킹 시에는 g++를 사용해서 -lrgbmatrix -lpthread -lrt 등을 붙여야 함.
// 
// 사용 예시(라이브러리 모드):
//   // client.c에서...
//   #include "board.h"
//
//   int main(...) {
//     char board[8][8] = { ... }; // 예: 'R','B','.' 으로 채워진 8×8 배열
//     if (!init_led_matrix()) { fprintf(stderr, "LED 초기화 실패\n"); }
//     display_board_on_led(board);
//     // ... 게임 중 board가 바뀔 때마다 display_board_on_led(board) 호출 ...
//     deinit_led_matrix();
//     return 0;
//   }
// 
// 사용 예시(Standalone 모드):
//   gcc -DBOARD_STANDALONE board.c -I/path/to/rpi-rgb-led-matrix/include \
//        -L/path/to/rpi-rgb-led-matrix/lib -lrgbmatrix -lpthread -lrt -o board_standalone
//   ./board_standalone < sample_board.txt
//   (sample_board.txt에 8줄 × 8문자 보드 텍스트가 들어 있어야 함)
// 
// ────────────────────────────────────────────────────────────────────────────

#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool init_led_panel(void);

void render(const char board[8][8]);

/**
 * deinit_led_matrix()
 *   LED 매트릭스를 정상 해제하고 모든 자원을 돌려줍니다.
 *   프로그램 종료 시 한 번만 호출하면 됩니다.
 */
void deinit_led_matrix(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // BOARD_H
