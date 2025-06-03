// simple_board_display.c
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "led-matrix-c.h"  // Use led-matrix-c.h as requested

// Board and matrix dimensions
static const int BOARD_SIZE  = 8;
static const int MATRIX_SIZE = 64;             // Changed to 64×64
static const int CELL_SIZE   = MATRIX_SIZE / BOARD_SIZE;  // 64/8 = 8

int main(void) {
    // 1) Initialize LED matrix
    struct RGBLedMatrixOptions options;
    struct RGBLedRuntimeOptions r_options;
    memset(&options,  0, sizeof(options));
    memset(&r_options, 0, sizeof(r_options));

    // Example settings: 64×64 matrix, brightness 50, GPIO init enabled
    options.rows         = MATRIX_SIZE;
    options.cols         = MATRIX_SIZE;
    options.chain_length = 1;
    options.parallel     = 1;
    options.brightness   = 50;
    options.hardware_mapping = "adafruit-hat";

    r_options.do_gpio_init = true;
    r_options.gpio_slowdown = 2;

    struct RGBLedMatrix *matrix =
        led_matrix_create_from_options_and_rt_options(&options, &r_options);
    if (!matrix) {
        fprintf(stderr, "ERROR: LED matrix initialization failed\n");
        return 1;
    }
    struct LedCanvas *canvas = led_matrix_get_canvas(matrix);
    if (!canvas) {
        fprintf(stderr, "ERROR: Failed to get canvas from LED matrix\n");
        led_matrix_delete(matrix);
        return 1;
    }

    // 2) Input buffers and board array
    char line[16];
    char board[BOARD_SIZE][BOARD_SIZE];

    // 3) Read 8 lines from stdin into board[][]
    for (int row = 0; row < BOARD_SIZE; ++row) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            fprintf(stderr, "ERROR: Failed to read input line %d\n", row);
            led_matrix_delete(matrix);
            return 1;
        }
        // Check at least 8 characters
        if ((int)strlen(line) < BOARD_SIZE) {
            fprintf(stderr, "ERROR: Line %d contains fewer than 8 characters\n", row);
            led_matrix_delete(matrix);
            return 1;
        }
        // Copy first 8 characters into board[row][]
        memcpy(board[row], line, BOARD_SIZE);
    }

    // 4) Draw board onto the LED matrix
    //    First clear the entire canvas to black
    led_canvas_clear(canvas);

    //    For each of the 8×8 cells, fill an 8×8 pixel block
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            unsigned char r_col = 0, g_col = 0, b_col = 0;
            if (board[r][c] == 'R') {
                r_col = 255;  // red
            } else if (board[r][c] == 'B') {
                b_col = 255;  // blue
            }
            // '.' or any other character remains black (0,0,0)

            // Top-left pixel of this cell's block
            int x0 = c * CELL_SIZE;
            int y0 = r * CELL_SIZE;

            // Fill an 8×8 pixel block with the chosen color
            for (int dy = 0; dy < CELL_SIZE; ++dy) {
                for (int dx = 0; dx < CELL_SIZE; ++dx) {
                    led_canvas_set_pixel(canvas,
                                         x0 + dx,
                                         y0 + dy,
                                         r_col, g_col, b_col);
                }
            }
        }
    }

    // 5) Render the frame to the LED matrix
    led_matrix_render_frame(canvas);

    // Print the board to terminal (debug)
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            putchar(board[r][c]);
        }
        putchar('\n');
    }

    // 6) Keep the result visible for 1 second (optional)
    sleep(1);

    // 7) Clean up and exit
    led_matrix_delete(matrix);
    return 0;
}
