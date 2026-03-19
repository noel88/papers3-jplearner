# Papers3 JP Learner

![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-M5Paper%20S3-green.svg)
![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)

M5Paper S3 기반 일본어 학습 전용 E-ink 리더

## 주요 기능

### 필사 모드 (Copy)
- 365일 일일 명언 필사 콘텐츠
- 오늘 날짜에 맞는 챕터 자동 선택
- 텍스트 선택 및 하이라이트
- 읽기 전용/필사 모드 전환

### 읽기 모드 (Read)
- EPUB 파일 읽기 지원
- 챕터 네비게이션
- 폰트 크기 조절
- 읽기 진행률 저장

### WiFi 파일 전송
- 웹 브라우저를 통한 EPUB 업로드
- SD 카드 직접 접근 없이 파일 관리

### 슬립 모드
- 설정 가능한 자동 슬립 (1-30분)
- 시계 및 배터리 표시 슬립 화면
- 터치로 깨우기
- E-ink 무전력 이미지 유지

### 시스템 설정
- 슬립 시간 조절
- 날짜/시간 수동 설정
- WiFi 설정

## 하드웨어

- **M5Paper S3** (ESP32-S3)
- 960 x 540 E-ink 디스플레이
- 16GB SD 카드 (권장)
- 터치스크린

## 사용법

### 최초 설정

1. SD 카드에 필요한 폴더 구조 생성
2. EPUB 파일을 `books/` 폴더에 복사
3. TTF 폰트를 `fonts/` 폴더에 복사 (선택)
4. 기기 전원 켜기

### 화면 네비게이션

- **하단 탭 바**: 필사 / 읽기 / 설정 화면 전환
- **좌/우 영역 터치**: 이전/다음 페이지
- **중앙 터치**: 텍스트 선택 (지원 화면에서)

### WiFi 파일 전송

1. 설정 > WiFi 설정 메뉴 진입
2. WiFi 연결 후 표시되는 IP 주소 확인
3. 웹 브라우저에서 `http://[IP주소]` 접속
4. EPUB 파일 업로드

### 슬립/웨이크

- 설정된 시간 동안 터치 없으면 자동 슬립
- 슬립 상태에서 화면 터치로 깨우기
- 슬립 화면에서 시간/날짜/배터리 확인 가능

## 개발 환경

- PlatformIO
- Arduino Framework
- ESP-IDF (부분)

## 빌드

```bash
# 빌드
pio run

# 업로드
pio run --target upload

# 시리얼 모니터
pio device monitor

# 파일시스템 업로드 (data 폴더)
pio run --target uploadfs
```

## 디렉토리 구조

```
paperS3-reader/
├── src/                  # 소스 코드
│   ├── main.cpp          # 메인 루프
│   ├── screens/          # 화면 구현
│   ├── utils/            # 유틸리티
│   └── ui/               # UI 컴포넌트
├── include/              # 헤더 파일
├── lib/                  # 커스텀 라이브러리
├── data/                 # 파일시스템 데이터
└── platformio.ini        # PlatformIO 설정
```

## SD 카드 구조

```
SD 카드/
├── books/                # EPUB 파일
│   └── 365.epub          # 기본 필사 콘텐츠
├── fonts/                # TTF 폰트 (선택)
│   └── NotoSansJP.ttf    # 일본어 폰트
└── userdata/             # 사용자 데이터
    └── progress.json     # 읽기 진행률
```

## 버전 히스토리

### v1.0.0 (2025-03-19)
- 최초 안정 릴리스
- 필사/읽기 모드 구현
- WiFi 파일 전송
- 슬립 모드 및 시계 화면
- 텍스트 선택 기능
- 시스템 설정 (슬립 시간, 날짜/시간)

## 라이선스

MIT License

## 참고

- [M5Paper S3 공식 문서](https://docs.m5stack.com/en/core/papers3)
- [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader)
