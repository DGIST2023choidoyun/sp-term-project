# rpi-rgb-led-matrix가 이미 설치 및 빌드까지 끝났다고 가정.

# 압축파일 내에 있는 파일들은 반드시 rpi-rgb-led-matrix 안에 있어야 함.

# 빌드 (릴리즈)

sudo make release

# 빌드 (디버그: 로그 확인)

sudo make debug

# 빌드 (디버그: 로그 & 메모리 체크)

sudo make test

# Client 코드만 빌드 및 TA 서버로 실행 (LED 제외, 대결 테스트용)

sudo make client
sudo make run

# LED Matrix Standalone

# Standalone시 입력 포맷은 8자\n8자\n...8자 총 8줄을 입력 받음. (포맷 안 지키면 오류)

sudo make board
sudo make run_board
