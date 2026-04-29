# JobPostCounterCpp

[TradingView 주봉 캔들차트 보기](https://ikboong.github.io/JobPostCounterCpp/weekly-candles.html)

C++로 잡코리아 지역별 채용정보의 일반 채용공고 수를 측정해 누적 CSV와 XLSX 파일을 만듭니다. XLSX에는 일별 추세 그래프와 주봉 캔들차트가 같이 생성됩니다.

## 데이터 방식

- 잡코리아: `https://www.jobkorea.co.kr/recruit/joblist?menucode=local&localorder=1` 페이지의 일반 채용정보 전체 수(`hdnGICnt`)를 읽습니다.
- 공개 API 키 없이 동작하지만, 잡코리아 페이지 구조가 바뀌면 파서 수정이 필요할 수 있습니다.

## XLSX 시트

- `Data`: 실행 시점별 원본 누적 데이터
- `Chart`: 실행별 공고 수 추세 그래프
- `WeeklyOHLC`: 주 단위 `Open`, `High`, `Low`, `Close` 집계
- `WeeklyCandles`: `WeeklyOHLC` 기반 주봉 캔들차트

## 빌드

```powershell
.\build.ps1
```

## 1회 실행

```powershell
.\run.ps1
```

기본 출력:

- `data\job_post_counts.csv`
- `data\job_post_counts.xlsx`

## 매일 자동 실행

이 repo의 `.github/workflows/daily-job-post-counter.yml`은 Cloudflare Worker가 매일 09:00 KST에 `workflow_dispatch`로 호출하도록 구성합니다.

- Windows runner에서 C++ 프로그램을 빌드합니다.
- 잡코리아 공고 수를 측정합니다.
- `data\job_post_counts.csv`, `data\job_post_counts.xlsx`가 바뀌면 자동 commit/push합니다.
- GitHub Pages 차트 데이터를 같은 workflow에서 다시 배포합니다.
- 한 번 GitHub에 push해두면 Codex 구독 여부와 무관하게 GitHub Actions가 계속 실행합니다.

GitHub repo의 Actions 권한에서 `Read and write permissions`가 허용되어 있어야 결과 파일을 push할 수 있습니다.

## 이상값 알림

`scripts/check_anomaly.ps1`은 매일 측정 직후 CSV의 최신 행을 검사합니다. 이상값이어도 CSV/XLSX를 먼저 commit/push한 뒤 workflow를 실패 처리하므로 GitHub 모바일 앱의 Actions 실패 푸시알림을 받을 수 있습니다.

현재 이상값 기준:

- 측정 실패: `JobKoreaStatus`가 `ok`가 아님
- 오늘 공고 수가 비어 있거나 숫자가 아님
- 직전 날짜의 정상 count 대비 20% 이상 급증/급감

푸시알림 테스트는 Actions 탭에서 `Test Failure Notification` workflow를 수동 실행하면 됩니다. 이 workflow는 데이터 파일을 건드리지 않고 의도적으로 실패합니다.

## TradingView 차트 페이지

GitHub README는 JavaScript와 iframe 실행을 제한하므로 TradingView 차트를 README 내부에 직접 렌더링할 수는 없습니다. 대신 GitHub Pages에서 `docs/weekly-candles.html`을 배포하고, README 상단에서 주봉 캔들차트 페이지로 연결합니다.

처음 한 번은 repo의 `Settings > Pages`에서 Source를 `GitHub Actions`로 설정한 뒤 `Publish Pages` workflow를 실행하면 됩니다.

## 로컬 Windows 작업 스케줄러 등록

예: 매일 오전 9시에 실행

```powershell
.\install_task.ps1 -Time "09:00"
```

## 직접 실행 옵션

```powershell
.\build\Release\JobPostCounter.exe --output-dir data
```

정상 측정에 실패해도 행은 누적됩니다. 실패 시 count는 비워두고 status/message 컬럼에 이유를 기록합니다.

웹 주봉 캔들차트와 XLSX의 `WeeklyOHLC`/`WeeklyCandles`는 `JobKoreaStatus`가 `ok`이고 count가 숫자인 행만 반영합니다.
