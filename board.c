#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // sleep()

#include "led-matrix-c.h"

#define BOARD_SIZE 8
#define MATRIX_SIZE 64
#define CELL_PIXEL_W (MATRIX_SIZE / BOARD_SIZE)
#define CELL_PIXEL_H (MATRIX_SIZE / BOARD_SIZE)


static const struct Color COLOR_RED   = { 255, 0, 0 };
static const struct Color COLOR_BLUE  = { 0, 0, 255 };
static const struct Color COLOR_BLACK = { 0, 0, 0 };

int main(void) {
    struct RGBLedMatrixOptions options;
    struct RGBLedMatrix *matrix;
    struct LedCanvas *canvas;

    char board[BOARD_SIZE][BOARD_SIZE];
    char line[64];

    int i, j;

    // 옵션 구조체 초기화
    memset(&options, 0, sizeof(options));

    options.rows = MATRIX_SIZE;
    options.cols = MATRIX_SIZE;
    options.chain_length = 1;

    options.no_hardware_pulse = true;


    // 매트릭스 초기화
    matrix = led_matrix_create_from_options(&options, NULL, NULL);
    if (matrix == NULL) {
        fprintf(stderr, "ERROR: Failed to create matrix.\n");
        return 1;
    }

    canvas = led_matrix_create_offscreen_canvas(matrix);
    if (canvas == NULL) {
        fprintf(stderr, "ERROR: Failed to get canvas.\n");
        led_matrix_delete(matrix);
        return 1;
    }

    // 8줄 입력 받기
    for (i = 0; i < BOARD_SIZE; ++i) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            fprintf(stderr, "ERROR: Failed to read line %d\n", i + 1);
            led_matrix_delete(matrix);
            return 1;
        }

        size_t len = strlen(line);
        if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';  // 줄바꿈 제거
        }

        if (len < BOARD_SIZE) {
            fprintf(stderr, "ERROR: Line %d is too short (got %zu chars)\n", i + 1, len);
            led_matrix_delete(matrix);
            return 1;
        }

        memcpy(board[i], line, BOARD_SIZE);
    }

    // 보드 출력
    led_canvas_clear(canvas);

    for (i = 0; i < BOARD_SIZE; ++i) {
        for (j = 0; j < BOARD_SIZE; ++j) {
            const struct Color *color;
            int x0 = j * CELL_PIXEL_W;
            int y0 = i * CELL_PIXEL_H;
            int dx, dy;

            char ch = board[i][j];
            if (ch == 'R') {
                color = &COLOR_RED;
            } else if (ch == 'B') {
                color = &COLOR_BLUE;
            } else {
                color = &COLOR_BLACK;
            }

            for (dy = 0; dy < CELL_PIXEL_H; ++dy) {
                for (dx = 0; dx < CELL_PIXEL_W; ++dx) {
                    led_canvas_set_pixel(canvas,
                                         x0 + dx, y0 + dy,
                                         color->r, color->g, color->b);
                }
            }
        }
    }

    led_matrix_swap_on_vsync(matrix, canvas);

    sleep(5);
    led_matrix_delete(matrix);
    return 0;
}
