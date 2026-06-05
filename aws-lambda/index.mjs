import https from "node:https";

const JOBKOREA_URL = "https://www.jobkorea.co.kr/recruit/joblist?menucode=local&localorder=1";
const DEFAULT_OWNER = "Ikboong";
const DEFAULT_REPO = "JobPostCounterCpp";
const DEFAULT_WORKFLOW = "record-jobkorea-count.yml";
const DEFAULT_REF = "main";

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function kstNow() {
  const kst = new Date(Date.now() + 9 * 60 * 60 * 1000);
  const iso = kst.toISOString();
  return {
    timestampKst: iso.slice(0, 19).replace("T", " "),
    dateKst: iso.slice(0, 10),
  };
}

function requestText(url, options = {}) {
  return new Promise((resolve, reject) => {
    const request = https.request(url, {
      method: options.method || "GET",
      headers: options.headers || {},
      timeout: options.timeoutMs || 25000,
    }, (response) => {
      const chunks = [];
      response.on("data", (chunk) => chunks.push(chunk));
      response.on("end", () => {
        const body = Buffer.concat(chunks).toString("utf8");
        if (response.statusCode < 200 || response.statusCode >= 300) {
          reject(new Error(`HTTP ${response.statusCode}: ${body.slice(0, 300)}`));
          return;
        }
        resolve(body);
      });
    });

    request.on("timeout", () => {
      request.destroy(new Error("request timed out"));
    });
    request.on("error", reject);

    if (options.body) {
      request.write(options.body);
    }
    request.end();
  });
}

async function requestTextWithRetry(url, options = {}, maxAttempts = 3) {
  let lastError;
  for (let attempt = 1; attempt <= maxAttempts; ++attempt) {
    try {
      return await requestText(url, options);
    } catch (error) {
      lastError = error;
      if (attempt < maxAttempts) {
        await sleep(attempt * 1500);
      }
    }
  }
  throw lastError;
}

function parseJobKoreaCount(html) {
  const hiddenInput = html.match(/id=["']hdnGICnt["'][^>]*value=["']?([0-9,]+)/i);
  if (hiddenInput) {
    return Number(hiddenInput[1].replace(/,/g, ""));
  }

  const visibleTotal = html.match(/전체\s*\(?\s*([0-9,]+)\s*건\s*\)?/);
  if (visibleTotal) {
    return Number(visibleTotal[1].replace(/,/g, ""));
  }

  throw new Error("JobKorea count was not found in response HTML.");
}

async function measureJobKorea() {
  const html = await requestTextWithRetry(JOBKOREA_URL, {
    headers: {
      "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/126 Safari/537.36",
      "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      "Accept-Language": "ko-KR,ko;q=0.9,en-US;q=0.7,en;q=0.5",
      "Cache-Control": "no-cache",
    },
  });
  const count = parseJobKoreaCount(html);
  if (!Number.isInteger(count) || count <= 0) {
    throw new Error(`Parsed count is invalid: ${count}`);
  }
  return count;
}

function readParam(params, names, fallback) {
  for (const name of names) {
    if (params && params[name] !== undefined && params[name] !== "") {
      return params[name];
    }
    if (process.env[name] !== undefined && process.env[name] !== "") {
      return process.env[name];
    }
  }
  return fallback;
}

function booleanParam(value) {
  return value === true || value === "true" || value === "1" || value === 1;
}

async function dispatchWorkflow(config, inputs) {
  const body = JSON.stringify({
    ref: config.ref,
    inputs,
  });

  await requestText(
    `https://api.github.com/repos/${config.owner}/${config.repo}/actions/workflows/${encodeURIComponent(config.workflow)}/dispatches`,
    {
      method: "POST",
      timeoutMs: 25000,
      headers: {
        "Authorization": `Bearer ${config.githubToken}`,
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "JobPostCounter-AwsLambda",
        "Content-Type": "application/json",
        "Content-Length": Buffer.byteLength(body),
      },
      body,
    },
  );
}

async function main(params = {}) {
  const config = {
    owner: readParam(params, ["GITHUB_OWNER", "github_owner", "githubOwner"], DEFAULT_OWNER),
    repo: readParam(params, ["GITHUB_REPO", "github_repo", "githubRepo"], DEFAULT_REPO),
    workflow: readParam(params, ["GITHUB_WORKFLOW", "github_workflow", "githubWorkflow"], DEFAULT_WORKFLOW),
    ref: readParam(params, ["GITHUB_REF", "github_ref", "githubRef"], DEFAULT_REF),
    githubToken: readParam(params, ["GITHUB_TOKEN", "github_token", "githubToken"], ""),
  };

  const now = kstNow();
  const inputs = {
    timestamp_kst: now.timestampKst,
    date_kst: now.dateKst,
    count: "",
    status: "error",
    message: "",
    source: JOBKOREA_URL,
  };

  try {
    const count = await measureJobKorea();
    inputs.count = String(count);
    inputs.status = "ok";
    inputs.message = "parsed joblist hdnGICnt";
  } catch (error) {
    inputs.status = "error";
    inputs.message = `AWS Lambda measurement failed: ${error.message}`;
  }

  if (booleanParam(readParam(params, ["DRY_RUN", "dry_run", "dryRun"], false))) {
    return {
      ok: inputs.status === "ok",
      dryRun: true,
      workflow: `${config.owner}/${config.repo}/${config.workflow}`,
      inputs,
    };
  }

  if (!config.githubToken) {
    throw new Error("GITHUB_TOKEN is required.");
  }

  await dispatchWorkflow(config, inputs);

  return {
    ok: inputs.status === "ok",
    dispatched: true,
    workflow: `${config.owner}/${config.repo}/${config.workflow}`,
    inputs,
  };
}

function normalizeLambdaEvent(event = {}) {
  let bodyParams = {};
  if (typeof event.body === "string" && event.body.trim()) {
    try {
      bodyParams = JSON.parse(event.body);
    } catch (_) {
      bodyParams = {};
    }
  }

  return {
    ...event,
    ...(event.queryStringParameters || {}),
    ...bodyParams,
  };
}

async function handler(event = {}) {
  return main(normalizeLambdaEvent(event));
}

export { main, handler };
