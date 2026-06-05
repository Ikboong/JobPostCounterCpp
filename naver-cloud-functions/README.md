# Cloud Function Dispatcher

This action measures the JobKorea posting count from a Korea-region serverless runtime, then dispatches the GitHub workflow that records the measured value.

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

## AWS Lambda setup

Use this when Naver Cloud Functions requires VPC/NAT Gateway for outbound internet.

1. Open the AWS console and select the Seoul region: `Asia Pacific (Seoul) ap-northeast-2`.
2. Go to Lambda and create a function:
   - Author from scratch
   - Function name: `jobkorea-dispatcher`
   - Runtime: Node.js 22.x
   - Architecture: x86_64
   - Do not enable VPC
3. Paste `jobkorea_dispatcher.js` into `index.js`.
4. Set the Lambda handler to `index.handler`.
5. Add environment variables:
   - `GITHUB_TOKEN`: GitHub token with Actions read/write on this repository
   - `DRY_RUN`: `true` for the first test
6. Run a Lambda test event with `{}` and confirm `status: ok`.
7. Change `DRY_RUN` to `false`.
8. Add EventBridge Scheduler schedules for KST:
   - daily 09:00
   - daily 09:10
   - daily 09:30
