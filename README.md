# Papers3 JP Learner

M5Paper S3 기반 일본어 학습 전용 기기

## 특징

- **SRS 학습**: SM-2 알고리즘 기반 간격 반복 학습 (Anki 스타일)
- **단어/문형**: N1-N2 레벨 단어 및 문형 학습
- **필사**: 365일 일일 명언 필사 콘텐츠
- **epub 리더**: epub 파일 읽기 지원
- **오프라인 사전**: SD 카드 기반 오프라인 일본어 사전

## 하드웨어

- M5Paper S3 (ESP32-S3)
- 960 x 540 E-ink 디스플레이
- 16GB SD 카드

## 개발 환경

- PlatformIO
- Arduino Framework

## 빌드

```bash
# PlatformIO CLI
pio run

# 업로드
pio run --target upload

# 파일시스템 업로드
pio run --target uploadfs
```

## 디렉토리 구조

```
papers3-jplearner/
├── src/                # 소스 코드
├── include/            # 헤더 파일
├── lib/                # 라이브러리
├── data/               # 파일시스템 데이터 (폰트 등)
├── docs/               # 설계 문서
│   └── DESIGN.md       # 상세 설계 문서
└── platformio.ini      # PlatformIO 설정
```

## SD 카드 구조

```
SD 카드/
├── books/              # epub 파일
│   └── 365.epub        # 기본 필사 콘텐츠
├── dict/               # 사전 데이터
├── fonts/              # 추가 폰트
└── userdata/           # 사용자 데이터 (SRS, 진행률)
```

## 라이선스

MIT License

## 참고

- [M5Paper S3 공식 문서](https://docs.m5stack.com/en/core/papers3)
- [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader)
