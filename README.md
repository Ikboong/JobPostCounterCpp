# JobPostCounterCpp

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

이 repo의 `.github/workflows/daily-job-post-counter.yml`은 매일 09:00 KST에 GitHub Actions에서 실행됩니다.

- Windows runner에서 C++ 프로그램을 빌드합니다.
- 잡코리아 공고 수를 측정합니다.
- `data\job_post_counts.csv`, `data\job_post_counts.xlsx`가 바뀌면 자동 commit/push합니다.
- 한 번 GitHub에 push해두면 Codex 구독 여부와 무관하게 GitHub Actions가 계속 실행합니다.

GitHub repo의 Actions 권한에서 `Read and write permissions`가 허용되어 있어야 결과 파일을 push할 수 있습니다.

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
