#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "led-matrix.h"
#include "graphics.h"

#define CELL_SIZE 8

using namespace rgb_matrix;

int main(int argc, char *argv[]) {
    RGBMatrix::Options defaults;
    defaults.rows = 64;
    defaults.cols = 64;
    defaults.chain_length = 1;
    defaults.parallel = 1;

    RuntimeOptions runtime_opt;
    RGBMatrix *matrix = CreateMatrixFromFlags(&argc, &argv, &defaults, &runtime_opt);
    if (matrix == NULL) {
        fprintf(stderr, "Could not create matrix.\n");
        return 1;
    }

    FrameCanvas *canvas = matrix->CreateFrameCanvas();

    char board[8][9];  // 8줄 입력 받을 공간

    // stdin에서 8줄 입력
    for (int i = 0; i < 8; ++i) {
        if (fgets(board[i], sizeof(board[i]), stdin) == NULL) {
            fprintf(stderr, "Failed to read board line %d\n", i);
            return 1;
        }
        // 줄바꿈 제거
        size_t len = strlen(board[i]);
        if (len > 0 && board[i][len - 1] == '\n') {
            board[i][len - 1] = '\0';
        }
    }

    // 보드를 캔버스에 출력
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            int start_x = col * CELL_SIZE;
            int start_y = row * CELL_SIZE;
            uint8_t r = 0, g = 0, b = 0;

            if (board[row][col] == 'R') {
                r = 255;
            } else if (board[row][col] == 'B') {
                b = 255;
            }

            for (int dy = 0; dy < CELL_SIZE; ++dy) {
                for (int dx = 0; dx < CELL_SIZE; ++dx) {
                    canvas->SetPixel(start_x + dx, start_y + dy, r, g, b);
                }
            }
        }
    }

    canvas = matrix->SwapOnVSync(canvas);  // 출력

    sleep(5);  // 5초간 유지 후 종료
    delete matrix;
    return 0;
}
