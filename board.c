// board.c
#define _POSIX_C_SOURCE 200809L   // usleep 등의 POSIX 함수 사용을 위해

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>            // usleep()
#include "board.h"
#include "rpi-rgb-led-matrix-c.h"   // Henner Zeller의 C 바인딩 헤더

// ─────────────────────────────────────────────────────────────────────────────
// 하드웨어 환경에 맞게 조정할 수 있는 상수들
// ─────────────────────────────────────────────────────────────────────────────

// OctaFlip 보드 크기
#define BOARD_SIZE    8

// 실제 연결된 LED 매트릭스 패널 해상도 및 체인 길이
// 예시: 32×32 크기 패널 한 장만 연결된 경우
#define MATRIX_ROWS      64
#define MATRIX_COLS      64
#define CHAIN_LENGTH      1
#define PARALLEL          1

// 8×8 보드를 32×32 LED에 표시하기 위해 각 셀당 차지하는 픽셀 블록 크기
#define CELL_PIXEL_W   (MATRIX_COLS / BOARD_SIZE)  // 32/8 = 4
#define CELL_PIXEL_H   (MATRIX_ROWS / BOARD_SIZE)  // 32/8 = 4

// 셀 색상 정의 (24-bit RGB)
static const struct Color COLOR_RED   = { .r = 0xFF, .g = 0x00, .b = 0x00 };
static const struct Color COLOR_BLUE  = { .r = 0x00, .g = 0x00, .b = 0xFF };
static const struct Color COLOR_BLACK = { .r = 0x00, .g = 0x00, .b = 0x00 };

// 전역으로 관리할 LED 매트릭스 객체와 캔버스
static struct RGBLedMatrix *matrix = NULL;
static struct LedCanvas   *canvas = NULL;

/**
 * init_led_matrix()
 *   LED 매트릭스를 초기화하여 전역 매트릭스/캔버스를 설정합니다.
 *   하드웨어 연결이 없거나 오류 시 0 리턴, 성공 시 1 리턴.
 */
int init_led_matrix(void) {
  struct RGBLedMatrixOptions opts;
  struct RGBLedRuntimeOptions rt_opts;
  memset(&opts,  0, sizeof(opts));
  memset(&rt_opts, 0, sizeof(rt_opts));

  // 매트릭스 옵션 설정 (예: 32×32 패널 1개)
  opts.rows         = MATRIX_ROWS;       
  opts.cols         = MATRIX_COLS;       
  opts.chain_length = CHAIN_LENGTH;     
  opts.parallel     = PARALLEL;         
  opts.brightness   = 50;               // 초기 밝기(1..100)
  opts.hardware_mapping = "adafruit-hat"; // Pi HAT 매핑 예시

  // 런타임 옵션 (GPIO 초기화 및 데몬 X, 권한 하락 X)
  rt_opts.gpio_slowdown    = 2;  
  rt_opts.do_gpio_init     = true;
  rt_opts.daemon           = 0;  
  rt_opts.drop_privileges  = 0;

  // 매트릭스 생성
  matrix = led_matrix_create_from_options_and_rt_options(&opts, &rt_opts);
  if (matrix == NULL) {
    fprintf(stderr, "ERROR: LED 매트릭스 생성 실패\n");
    return 0;
  }

  canvas = led_matrix_get_canvas(matrix);
  if (canvas == NULL) {
    fprintf(stderr, "ERROR: LED 캔버스 가져오기 실패\n");
    led_matrix_delete(matrix);
    matrix = NULL;
    return 0;
  }

  return 1;
}

/**
 * display_board_on_led()
 *   8×8 board를 받아서 32×32 LED 매트릭스 상에서 4×4 픽셀 블록으로 매핑해 그립니다.
 *   반드시 init_led_matrix()가 성공적으로 호출된 뒤에만 사용해야 합니다.
 */
void display_board_on_led(char board[8][8]) {
  if (matrix == NULL || canvas == NULL) {
    // 아직 초기화되지 않았으면 아무 작업도 하지 않음
    return;
  }

  // 1) 캔버스를 검은색(꺼짐)으로 클리어
  led_canvas_clear(canvas);

  // 2) 8×8 보드 순회하며 각각 빨강/파랑/검은색 블록으로 채움
  for (int r = 0; r < BOARD_SIZE; ++r) {
    for (int c = 0; c < BOARD_SIZE; ++c) {
      char cell = board[r][c];
      const struct Color *col;
      if (cell == 'R')      col = &COLOR_RED;
      else if (cell == 'B') col = &COLOR_BLUE;
      else                  col = &COLOR_BLACK;

      // 이 칸이 차지할 LED 매트릭스 픽셀 영역 계산
      int x0 = c * CELL_PIXEL_W;
      int y0 = r * CELL_PIXEL_H;

      // 4×4 블록(예시) 만큼 채우기
      for (int dy = 0; dy < CELL_PIXEL_H; ++dy) {
        for (int dx = 0; dx < CELL_PIXEL_W; ++dx) {
          led_canvas_set_pixel(canvas, x0 + dx, y0 + dy,
                               col->r, col->g, col->b);
        }
      }
    }
  }

  // 이 예제는 단일 버퍼만 사용하므로 별도 swap_on_vsync 호출 없이 즉시 표시됩니다.
  // (만약 더블버퍼링을 원하면, 
  //   canvas = led_matrix_swap_on_vsync(matrix, canvas);
  //  처리를 해 주시면 됩니다.)
}

/**
 * deinit_led_matrix()
 *   LED 매트릭스를 정리(자원 해제)합니다. 
 *   프로그램 종료 시 한 번만 호출해야 합니다.
 */
void deinit_led_matrix(void) {
  if (matrix) {
    led_matrix_delete(matrix);
    matrix = NULL;
    canvas = NULL;
  }
}


#ifdef BOARD_STANDALONE

/**
 * Standalone 모드용 main()
 *
 * 표준 입력으로 8줄×8문자 형식의 보드 텍스트를 계속해서 읽어들인 뒤,
 *  - (가능하면) LED 매트릭스에도 출력
 *  - 표준 출력(터미널)에도 동일한 8×8 텍스트를 echo
 *
 * 입력 예시 (board.txt):
 *   RR..BB..   ← 8문자
 *   RR..BB..
 *   ........
 *   ...중간 생략...
 *   ........
 *   BB..RR..
 *   BB..RR..
 *
 * 위와 같이 첫 8줄을 읽으면 한 차례 보드. 그다음 EOF거나
 * 더 데이터가 있으면 다음 8줄을 또 읽음.
 */
int main(void) {
  char board[8][8];
  char linebuf[64];
  int initialized = 0;

  // LED 매트릭스 초기화 시도 (실패해도 계속 진행)
  if (init_led_matrix()) {
    initialized = 1;
  } else {
    fprintf(stderr, "Warning: LED 매트릭스 초기화 실패, stdout으로만 출력합니다.\n");
  }

  // “8줄을 한 묶음”으로 반복해서 읽는다
  while (1) {
    // 1) 8줄을 읽어서 board 배열에 채운다.
    for (int i = 0; i < 8; ++i) {
      if (fgets(linebuf, sizeof(linebuf), stdin) == NULL) {
        // 입력이 더 이상 없으면 종료
        if (i == 0) {
          // 아무것도 안 읽고 EOF → 정상 종료
          if (initialized) deinit_led_matrix();
          return 0;
        } else {
          // 중간에 EOF가 오면 잘못된 입력
          fprintf(stderr, "Error: 8줄 보드 입력 중 %d줄만 읽고 EOF 발견\n", i);
          if (initialized) deinit_led_matrix();
          return 1;
        }
      }
      // 줄 끝의 개행 제거
      size_t len = strlen(linebuf);
      if (len > 0 && (linebuf[len-1] == '\n' || linebuf[len-1] == '\r')) {
        linebuf[len-1] = '\0';
        --len;
      }
      // 8문자가 아니라면 에러
      if (len < 8) {
        fprintf(stderr, "Error: 각 줄은 최소 8문자여야 합니다 (줄 %d: \"%s\")\n", i+1, linebuf);
        if (initialized) deinit_led_matrix();
        return 1;
      }
      // board 배열에 복사
      memcpy(board[i], linebuf, 8);
    }

    // 2) LED에 그리기 (가능하면)
    if (initialized) {
      display_board_on_led(board);
      // LED가 없는 환경이라면 약간의 지연을 줘야 깜빡임을 볼 수 있음
      usleep(200000); // 0.2초
    }

    // 3) stdout에도 동일한 8×8 보드 텍스트를 echo
    for (int i = 0; i < 8; ++i) {
      // 끝에 개행 붙여서 출력
      printf("%.*s\n", 8, board[i]);
    }
    fflush(stdout);

    // 4) (선택) 빈 줄 하나 읽어들여서 다음 블록과 구분할 수 있도록 할 수도 있음
    //    여기서는 “바로 다음 8줄”을 곧바로 읽는 형식으로 했습니다.
  }

  // 여기까지 도달하지 않음
  if (initialized) deinit_led_matrix();
  return 0;
}

#endif  // BOARD_STANDALONE
