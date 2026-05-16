# LooseGrownDiamondPriceTrackerCpp

C++로 Loose Grown Diamond 검색 조건의 최저 표시가를 매일 누적합니다.

대상 URL:

`https://www.loosegrowndiamond.com/lab-created-colored-diamonds/?shape=heart&carat=5.00,51.03&certificate=1,2&intensity=4&cut=3.00,4.00&color=pink&clarity=7.00,11.00`

수집 방식:

- `wp-json/ls/v1/inventory-colored` API를 호출합니다.
- API의 내부 `price`에 페이지에 표시된 할인율, 예: `SPRING30` 30%, 을 적용해 화면 최종가 기준으로 최저가를 계산합니다.
- API의 `org_price`는 화면 취소선 원가로 기록합니다.

출력:

- `data/loosegrown_price_history.csv`
- `data/loosegrown_tradingview_chart.html`

차트:

- 일봉: TradingView Lightweight Charts 라인차트
- 주봉: 일봉 최저가를 주 단위 OHLC로 집계한 TradingView 캔들차트

빌드:

```powershell
.\build.ps1
```

1회 실행:

```powershell
.\run.ps1
```

로컬 Windows 예약 작업:

```powershell
.\install_task.ps1 -Time "09:00"
```

GitHub Actions:

repo 루트의 `.github/workflows/daily-loosegrown-price-tracker.yml`은 매일 09:00 KST에 실행되고, CSV/SVG가 바뀌면 자동 commit/push합니다. 저장소 Actions 권한에서 `Read and write permissions`가 허용되어 있어야 합니다.
