# Naver Cloud Functions Dispatcher

This action measures the JobKorea posting count from a Korea-region Naver Cloud Functions runtime, then dispatches the GitHub workflow that records the measured value.

## Required secret/default parameter

- `GITHUB_TOKEN`: GitHub fine-grained token or classic PAT that can run Actions workflows on `Ikboong/JobPostCounterCpp`.

Minimum GitHub permissions:

- Actions: read/write

## Optional parameters

- `GITHUB_OWNER`: defaults to `Ikboong`
- `GITHUB_REPO`: defaults to `JobPostCounterCpp`
- `GITHUB_WORKFLOW`: defaults to `record-jobkorea-count.yml`
- `GITHUB_REF`: defaults to `main`
- `DRY_RUN`: set to `true` to measure without dispatching GitHub

## Naver setup

1. Create a Naver Cloud Functions action with Node.js runtime.
2. Paste `jobkorea_dispatcher.js` as the action code.
3. Add `GITHUB_TOKEN` as a secret/default parameter.
4. Run once with `DRY_RUN=true` and confirm `status: ok`.
5. Add Cron triggers for KST using the console schedule UI:
   - daily 09:00
   - daily 09:10
   - daily 09:30

If the console asks for a raw cron expression in UTC, use `0 0 * * *`, `10 0 * * *`, and `30 0 * * *` instead.

The GitHub side keeps one row per `DateKST`, preferring the earliest successful measurement for that date.
