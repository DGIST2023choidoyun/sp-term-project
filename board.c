// #include <stdio.h>
// #include <stdlib.h>
// #include "board.h"
// #include "led-matrix-c.h"

// static RGBLedMatrix *matrix = NULL;
// static LedCanvas *canvas = NULL;

// void init_display() {
//     if (matrix == NULL) {
//         RGBLedMatrixOptions options = {
//             .rows = 64,
//             .cols = 64,
//             .chain_length = 1,
//             .parallel = 1,
//             .hardware_mapping = "adafruit-hat"
//         };

//         matrix = led_matrix_create_from_options(&options, NULL);
//         if (matrix == NULL) {
//             fprintf(stderr, "LED 매트릭스 초기화 실패\n");
//             exit(1);
//         }

//         canvas = led_matrix_get_canvas(matrix);
//     }
// }

// static void draw_cell(int row, int col, char symbol) {
//     int x0 = col * 8;
//     int y0 = row * 8;

//     uint8_t r = 0, g = 0, b = 0;

//     if (symbol == 'R') r = 255;
//     else if (symbol == 'B') b = 255;
//     else if (symbol == '#') r = g = b = 50;

//     for (int y = 1; y <= 6; ++y) {
//         for (int x = 1; x <= 6; ++x) {
//             led_canvas_set_pixel(canvas, x0 + x, y0 + y, r, g, b);
//         }
//     }

//     for (int i = 0; i < 8; ++i) {
//         led_canvas_set_pixel(canvas, x0 + i, y0, 64, 64, 64);
//         led_canvas_set_pixel(canvas, x0 + i, y0 + 7, 64, 64, 64);
//         led_canvas_set_pixel(canvas, x0, y0 + i, 64, 64, 64);
//         led_canvas_set_pixel(canvas, x0 + 7, y0 + i, 64, 64, 64);
//     }
// }

// void draw_board(const char board[8][8]) {
//     init_display();
//     for (int r = 0; r < 8; ++r)
//         for (int c = 0; c < 8; ++c)
//             draw_cell(r, c, board[r][c]);
// }
