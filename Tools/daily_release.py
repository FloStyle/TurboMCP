"""Publish owner-authorized daily source snapshots after verified main-push CI."""
import datetime as dt
import json
import os
from pathlib import Path
import re
import subprocess
from zoneinfo import ZoneInfo

REPOSITORY = "FloStyle/TurboMCP"
SHA = re.compile(r"[0-9a-f]{40}")
DAILY = re.compile(r"daily-\d{4}-\d{2}-\d{2}")


def daily_plan(run, main_sha, day, releases):
    """Pure decision logic. Refuse foreign/failed runs; skip stale or unchanged work."""
    if (run.get("name") != "Repository validation"
            or run.get("path") != ".github/workflows/validate.yml"
            or run.get("event") != "push"
            or run.get("head_branch") != "main"
            or run.get("head_repository", {}).get("full_name") != REPOSITORY
            or run.get("status") != "completed"
            or run.get("conclusion") != "success"):
        raise ValueError("Only successful upstream main-push validation may publish.")
    sha = run.get("head_sha", "")
    if not SHA.fullmatch(sha) or not SHA.fullmatch(main_sha):
        raise ValueError("Expected exact full commit identities.")
    dt.date.fromisoformat(day)
    tag = "daily-" + day
    if not DAILY.fullmatch(tag):
        raise ValueError("Invalid daily tag.")
    if sha != main_sha:
        return {"action": "skip", "reason": "Validated commit is no longer main."}
    daily = [r for r in releases if DAILY.fullmatch(r.get("tag_name", ""))]
    if any(r.get("target_commitish") == sha and r.get("prerelease")
           and not r.get("draft") for r in daily):
        return {"action": "skip", "reason": "This commit already has a daily snapshot."}
    existing = next((r for r in daily if r["tag_name"] == tag), None)
    if existing and (not existing.get("prerelease") or existing.get("draft")):
        raise ValueError("The daily name is occupied by an unexpected release.")
    return {"action": "update" if existing else "create", "tag": tag,
            "sha": sha, "release_id": existing.get("id") if existing else None}


def gh_api(path, method="GET", payload=None):
    command = ["gh", "api", "repos/" + REPOSITORY + "/" + path,
               "--method", method]
    if payload is not None:
        command += ["--input", "-"]
    result = subprocess.run(command, input=json.dumps(payload) if payload is not None else None,
                            capture_output=True, text=True, encoding="utf-8")
    if result.returncode:
        # Do not echo request data, environment or authentication headers.
        raise RuntimeError("GitHub API failed: " + method + " " + path)
    return json.loads(result.stdout) if result.stdout.strip() else None


def main():
    if os.environ.get("GITHUB_REPOSITORY") != REPOSITORY:
        raise ValueError("Daily publisher is restricted to the owner repository.")
    if os.environ.get("GITHUB_EVENT_NAME") != "workflow_run":
        raise ValueError("Daily publisher requires a completed validation event.")
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8"))
    run_id = event.get("workflow_run", {}).get("id")
    if type(run_id) is not int or run_id <= 0:
        raise ValueError("Missing validation run identity.")
    # Verify with GitHub; never accept an artifact as proof of validation.
    run = gh_api("actions/runs/" + str(run_id))
    main_sha = gh_api("git/ref/heads/main")["object"]["sha"]
    releases = gh_api("releases?per_page=100")
    day = dt.datetime.now(ZoneInfo("Europe/Paris")).date().isoformat()
    plan = daily_plan(run, main_sha, day, releases)
    if plan["action"] == "skip":
        print(plan["reason"])
        return
    tag, sha = plan["tag"], plan["sha"]
    refs = gh_api("git/matching-refs/tags/" + tag)
    ref = next((r for r in refs if r["ref"] == "refs/tags/" + tag), None)
    if ref and ref["object"]["type"] != "commit":
        raise ValueError("Daily publisher only owns lightweight daily tags.")
    # Avoid publishing a run superseded while metadata was being read.
    if gh_api("git/ref/heads/main")["object"]["sha"] != sha:
        print("Main advanced; the newer validation run will publish.")
        return
    body = ("Daily development source snapshot. Not a stable release or installation package.\n\n"
            + "Commit: `" + sha + "`\n\n"
            + "Validation: https://github.com/" + REPOSITORY + "/actions/runs/" + str(run_id)
            + "\n\nThis day\'s tag may move after another successful main push. "
            + "Record the commit when reporting an issue. Native acceptance and engine support "
            + "remain as documented in the repository. No binaries are included.\n")
    payload = {"tag_name": tag, "target_commitish": sha, "name": "Daily source - " + day,
               "body": body, "draft": False, "prerelease": True, "make_latest": "false"}
    if ref:
        if ref["object"]["sha"] != sha:
            gh_api("git/refs/tags/" + tag, "PATCH", {"sha": sha, "force": True})
    else:
        gh_api("git/refs", "POST", {"ref": "refs/tags/" + tag, "sha": sha})
    if plan["release_id"]:
        gh_api("releases/" + str(plan["release_id"]), "PATCH", payload)
    else:
        gh_api("releases", "POST", payload)
    print("Published " + tag + " at " + sha)


if __name__ == "__main__":
    main()
