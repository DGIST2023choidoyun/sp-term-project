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

/**
 * init_led_matrix()
 *   Raspberry Pi에 연결된 RGB LED 매트릭스를 초기화합니다.
 *   연결된 하드웨어가 없거나 초기화 실패 시 0을 리턴, 성공 시 1을 리턴합니다.
 *
 *   반드시 프로그램 시작 시 한 번만 호출해야 합니다.
 */
int init_led_matrix(void);

/**
 * display_board_on_led(board)
 *   8×8 char 배열(board)을 받아서, 연결된 RGB LED 매트릭스에 그려줍니다.
 *   
 *   board[r][c] 값이:
 *     'R' → 빨간색 블록,
 *     'B' → 파란색 블록,
 *     '.' → 검은색(꺼짐) 블록
 *   으로 매핑되어, 예를 들어 32×32 물리 매트릭스라면
 *   각 셀을 4×4 LED 픽셀 블록으로 칠하여 총 8×8 → 32×32로 확대해 표시합니다.
 *
 *   init_led_matrix() 호출이 성공한 뒤에만 동작합니다. 
 */
void display_board_on_led(char board[8][8]);

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
