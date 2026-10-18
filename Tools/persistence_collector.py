"""Pure orchestration for one bounded persistence identity collection pair.

This module deliberately contains no Windows adapter. Process, path, listener and
command-line parsing come from an injected backend. All deadlines, identity
comparisons, snapshot assembly and final schema-v1 validation happen here.

The public result is a structured success/refusal. Backend records and raw error
text never cross that boundary. A backend that never returns cannot be
interrupted by this pure module; the future adapter must enforce the deadline it
is given.
"""

from __future__ import annotations

import datetime as _datetime
import ntpath
import re
from collections import Counter

import persistence_identity


EXPECTED_SCHEMA = "turbomcp.persistence-expected/1"
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z$")
FILETIME_UNIX_EPOCH_TICKS = 116_444_736_000_000_000
NS_PER_TICK = 100
TICKS_PER_SECOND = 10_000_000
MAX_PATH = 4096
MAX_COMMAND_LINE = 32768
MAX_LAUNCH_ID = 128
MAX_UINT32 = 0xFFFFFFFF

DEFAULT_POLICY = {
    "per_call_ns": 2_000_000_000,
    "attempt_ns": 5_000_000_000,
    "pair_ns": 15_000_000_000,
    "snapshot_gap_ns": 5_000_000_000,
    "age_ns": 10_000_000_000,
    "max_listener_rows": 32,
}


def default_policy():
    """Return a fresh complete policy dictionary."""
    return dict(DEFAULT_POLICY)

_BACKEND_ERRORS = {
    "timeout": "backend_timeout",
    "permission": "backend_permission",
    "not_found": "backend_not_found",
    "provider_error": "backend_provider_error",
    "incomplete": "backend_incomplete",
    "ambiguous": "backend_ambiguous",
    "unknown": "backend_unknown",
}

_CATEGORY = {
    "invalid_expected": "schema",
    "invalid_policy": "schema",
    "endpoint_mismatch": "schema",
    "backend_timeout": "backend",
    "backend_permission": "backend",
    "backend_not_found": "backend",
    "backend_provider_error": "backend",
    "backend_incomplete": "backend",
    "backend_ambiguous": "backend",
    "backend_malformed": "backend",
    "backend_unknown": "backend",
    "clock_rollback": "clock",
    "deadline_exceeded": "deadline",
    "process_mismatch": "identity",
    "path_mismatch": "identity",
    "listener_missing": "identity",
    "listener_foreign": "identity",
    "listener_multiple_owners": "identity",
    "listener_port_mismatch": "identity",
    "listener_overflow": "identity",
    "observation_changed": "identity",
    "observation_order": "clock",
    "stale_observation": "clock",
    "validator_refused": "validator",
}


class _Refusal(Exception):
    def __init__(self, reason):
        super().__init__(reason)
        self.reason = reason


def _refuse(reason):
    raise _Refusal(reason)


def _copy_public_refusal(reason):
    return {"ok": False, "reason": reason, "category": _CATEGORY[reason]}


def _reason_for(label):
    if label.startswith("backend"):
        return "backend_malformed"
    if label.startswith("policy"):
        return "invalid_policy"
    return "invalid_expected"


def _require_int(value, low, high, label):
    reason = _reason_for(label)
    if isinstance(value, bool) or not isinstance(value, int):
        _refuse(reason)
    if value < low or value > high:
        _refuse(reason)
    return value


def _require_str(value, maximum, label):
    reason = _reason_for(label)
    if not isinstance(value, str) or not value or len(value) > maximum:
        _refuse(reason)
    return value


def _uint32(value, label):
    return _require_int(value, 0, MAX_UINT32, label)


def _exact_object(value, keys, label):
    reason = _reason_for(label)
    if not isinstance(value, dict):
        _refuse(reason)
    for key in value:
        if not isinstance(key, str):
            _refuse(reason)
    if set(value) != set(keys):
        _refuse(reason)
    return value


def _path_text(value, label, extension=None):
    reason = _reason_for(label)
    if not isinstance(value, str) or not value or len(value) > MAX_PATH:
        _refuse(reason)
    if value.startswith("\\\\") or "/" in value:
        _refuse(reason)
    if any(ch in '<>"|?*' or ord(ch) < 0x20 or ord(ch) == 0x7F for ch in value):
        _refuse(reason)
    drive, tail = ntpath.splitdrive(value)
    if not re.fullmatch(r"[A-Za-z]:", drive) or not tail.startswith("\\"):
        _refuse(reason)
    if value.endswith("\\"):
        _refuse(reason)
    parts = tail[1:].split("\\")
    if any(not part or part in (".", "..") or ":" in part for part in parts):
        _refuse(reason)
    if any(part.endswith(".") or part.endswith(" ") for part in parts):
        _refuse(reason)
    if any(re.fullmatch(r"(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?", part, re.IGNORECASE)
           for part in parts):
        _refuse(reason)
    if extension and ntpath.splitext(parts[-1])[1].casefold() != extension:
        _refuse(reason)
    return value


def _path_key(value):
    # ASCII-only case folding, matching the accepted validator.
    return value.translate(str.maketrans("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz"))


def _parse_utc(value, label):
    reason = _reason_for(label)
    if not isinstance(value, str) or not UTC_RE.fullmatch(value):
        _refuse(reason)
    try:
        base = _datetime.datetime.strptime(
            value[:19] + "Z", "%Y-%m-%dT%H:%M:%SZ"
        )
    except ValueError:
        _refuse(reason)
    epoch = _datetime.datetime(1970, 1, 1)
    delta = base - epoch
    seconds = delta.days * 86400 + delta.seconds
    return seconds * TICKS_PER_SECOND + int(value[20:27])


def _format_utc(ticks):
    seconds, fraction = divmod(ticks, TICKS_PER_SECOND)
    try:
        base = _datetime.datetime(1970, 1, 1) + _datetime.timedelta(seconds=seconds)
    except (OverflowError, ValueError):
        _refuse("clock_rollback")
    return base.strftime("%Y-%m-%dT%H:%M:%S") + ".%07dZ" % fraction


def _filetime_utc(ticks, reason):
    if isinstance(ticks, bool) or not isinstance(ticks, int) or ticks < 0:
        _refuse(reason)
    try:
        return _format_utc(ticks - FILETIME_UNIX_EPOCH_TICKS)
    except _Refusal:
        _refuse(reason)


def filetime_to_utc(ticks):
    """Convert a FILETIME 100 ns count to canonical UTC or refuse if out of range."""
    return _filetime_utc(ticks, "invalid_expected")


def utc_to_filetime_ticks(value):
    """Test/support conversion from canonical UTC to FILETIME ticks."""
    return _parse_utc(value, "expected") + FILETIME_UNIX_EPOCH_TICKS


def _fileid(value, label):
    item = _exact_object(
        value,
        {"volume_serial", "file_index_high", "file_index_low", "link_count"},
        label,
    )
    volume = _uint32(item["volume_serial"], label + ".volume_serial")
    high = _uint32(item["file_index_high"], label + ".file_index_high")
    low = _uint32(item["file_index_low"], label + ".file_index_low")
    if isinstance(item["link_count"], bool) or not isinstance(item["link_count"], int):
        _refuse("invalid_expected")
    if item["link_count"] != 1:
        _refuse("invalid_expected")
    return {"volume_serial": volume, "file_index_high": high, "file_index_low": low, "link_count": 1}


def _endpoint(value, label):
    item = _exact_object(value, {"host", "port"}, label)
    host = item["host"]
    if host not in ("127.0.0.1", "::1"):
        _refuse("invalid_expected")
    port = _require_int(item["port"], 1, 65535, label + ".port")
    return {"host": host, "port": port}


def _expected_record(value):
    item = _exact_object(
        value,
        {"schema", "launch_id", "pid", "editor_start_ticks", "project", "engine", "endpoint"},
        "expected",
    )
    if item["schema"] != EXPECTED_SCHEMA:
        _refuse("invalid_expected")
    launch = item["launch_id"]
    if (
        not isinstance(launch, str)
        or not 1 <= len(launch) <= MAX_LAUNCH_ID
        or any(ord(ch) < 0x20 or ord(ch) > 0x7E for ch in launch)
    ):
        _refuse("invalid_expected")
    pid = _require_int(item["pid"], 1, 2**32 - 1, "expected.pid")
    start_ticks = _require_int(
        item["editor_start_ticks"], 1, 2**63 - 1, "expected.editor_start_ticks"
    )
    # This also rejects dates outside the validator's representable range.
    filetime_to_utc(start_ticks)
    project = _exact_object(item["project"], {"path", "file_id"}, "expected.project")
    engine = _exact_object(item["engine"], {"path", "file_id"}, "expected.engine")
    project_path = _path_text(project["path"], "expected.project.path", ".uproject")
    engine_path = _path_text(engine["path"], "expected.engine.path", ".exe")
    return {
        "launch_id": launch,
        "pid": pid,
        "start_ticks": start_ticks,
        "project_path": project_path,
        "project_file_id": _fileid(project["file_id"], "expected.project.file_id"),
        "engine_path": engine_path,
        "engine_file_id": _fileid(engine["file_id"], "expected.engine.file_id"),
        "endpoint": _endpoint(item["endpoint"], "expected.endpoint"),
    }


def _policy(value):
    item = _exact_object(
        value,
        {"per_call_ns", "attempt_ns", "pair_ns", "snapshot_gap_ns", "age_ns", "max_listener_rows"},
        "policy",
    )
    result = {}
    for key in ("per_call_ns", "attempt_ns", "pair_ns", "age_ns", "max_listener_rows"):
        result[key] = _require_int(item[key], 1, 30_000_000_000, "policy." + key)
    result["snapshot_gap_ns"] = _require_int(
        item["snapshot_gap_ns"], 0, 10_000_000_000, "policy.snapshot_gap_ns"
    )
    if result["per_call_ns"] > 5_000_000_000:
        _refuse("invalid_policy")
    if result["attempt_ns"] > 10_000_000_000:
        _refuse("invalid_policy")
    if result["pair_ns"] > 30_000_000_000 or result["pair_ns"] < result["attempt_ns"]:
        _refuse("invalid_policy")
    if result["age_ns"] > 30_000_000_000:
        _refuse("invalid_policy")
    if result["max_listener_rows"] > 32:
        _refuse("invalid_policy")
    return result


def _read_result(raw):
    if not isinstance(raw, dict) or set(raw) not in ({"ok", "value"}, {"ok", "error"}):
        _refuse("backend_malformed")
    if not isinstance(raw["ok"], bool):
        _refuse("backend_malformed")
    if raw["ok"]:
        return raw["value"]
    error = raw.get("error")
    if error not in _BACKEND_ERRORS:
        _refuse("backend_malformed")
    _refuse(_BACKEND_ERRORS[error])


def _call(backend, method, *args):
    try:
        return getattr(backend, method)(*args)
    except Exception:
        _refuse("backend_unknown")


def _read(backend, method, clock, deadline_ns, *args):
    value = _read_result(_call(backend, method, *args, deadline_ns))
    if clock.now() >= deadline_ns:
        _refuse("deadline_exceeded")
    return value


def _monotonic(backend):
    value = _call(backend, "monotonic_ns")
    if isinstance(value, bool) or not isinstance(value, int):
        _refuse("backend_malformed")
    return value


class _Clock:
    def __init__(self, backend):
        self.backend = backend
        self.last = None

    def now(self):
        value = _monotonic(self.backend)
        if self.last is not None and value < self.last:
            _refuse("clock_rollback")
        self.last = value
        return value


def _utc(backend):
    value = _call(backend, "utc_now")
    if not isinstance(value, str):
        _refuse("backend_malformed")
    return _parse_utc(value, "backend.utc")


def _process(value):
    if not isinstance(value, dict) or set(value) != {"pid", "start_ticks", "executable_path", "command_line"}:
        _refuse("backend_malformed")
    pid = _require_int(value["pid"], 1, 2**32 - 1, "backend.process.pid")
    start_ticks = _require_int(value["start_ticks"], 1, 2**63 - 1, "backend.process.start_ticks")
    _filetime_utc(start_ticks, "backend_malformed")
    executable = _path_text(value["executable_path"], "backend.process.executable_path", ".exe")
    command = value["command_line"]
    if not isinstance(command, str) or len(command) > MAX_COMMAND_LINE:
        _refuse("backend_malformed")
    return {
        "pid": pid,
        "start_ticks": start_ticks,
        "executable_path": executable,
        "command_line": command,
    }


def _project(value):
    if not isinstance(value, dict) or set(value) != {"project_path", "token_count"}:
        _refuse("backend_malformed")
    path = _path_text(value["project_path"], "backend.project.project_path", ".uproject")
    count = _require_int(value["token_count"], 0, 1000, "backend.project.token_count")
    return {"project_path": path, "token_count": count}


def _path(value, label):
    if not isinstance(value, dict) or set(value) != {
        "path", "exists", "has_reparse_component", "has_short_name_component",
        "long_path", "volume_serial", "file_index_high", "file_index_low", "link_count",
    }:
        _refuse("backend_malformed")
    path = _path_text(value["path"], label + ".path")
    long_path = _path_text(value["long_path"], label + ".long_path")
    for key in ("exists", "has_reparse_component", "has_short_name_component"):
        if not isinstance(value[key], bool):
            _refuse("backend_malformed")
    volume = _uint32(value["volume_serial"], label + ".volume_serial")
    high = _uint32(value["file_index_high"], label + ".file_index_high")
    low = _uint32(value["file_index_low"], label + ".file_index_low")
    link = value["link_count"]
    if isinstance(link, bool) or not isinstance(link, int):
        _refuse("backend_malformed")
    if link != 1:
        _refuse("path_mismatch" if link > 1 else "backend_malformed")
    return {
        "path": path,
        "exists": value["exists"],
        "has_reparse_component": value["has_reparse_component"],
        "has_short_name_component": value["has_short_name_component"],
        "long_path": long_path,
        "volume_serial": volume,
        "file_index_high": high,
        "file_index_low": low,
        "link_count": 1,
    }


def _listener(value, endpoint):
    if not isinstance(value, dict) or set(value) != {
        "family", "address", "port", "state", "owner_pid"
    }:
        _refuse("backend_malformed")
    family = value["family"]
    address = value["address"]
    if family == "ipv4" and address in ("127.0.0.1", "0.0.0.0"):
        pass
    elif family == "ipv6" and address in ("::1", "::"):
        pass
    else:
        _refuse("backend_malformed")
    port = _require_int(value["port"], 1, 65535, "backend.listener.port")
    if port != endpoint["port"]:
        _refuse("listener_port_mismatch")
    if value["state"] != "listen":
        _refuse("backend_malformed")
    owner = _require_int(value["owner_pid"], 1, 2**32 - 1, "backend.listener.owner_pid")
    return {"family": family, "address": address, "port": port, "state": "listen", "owner_pid": owner}


def _verify_path(path, expected_path, expected_file_id):
    if not path["exists"]:
        _refuse("path_mismatch")
    if path["has_reparse_component"] or path["has_short_name_component"]:
        _refuse("path_mismatch")
    if _path_key(path["path"]) != _path_key(expected_path):
        _refuse("path_mismatch")
    if _path_key(path["long_path"]) != _path_key(path["path"]):
        _refuse("path_mismatch")
    observed = (path["volume_serial"], path["file_index_high"], path["file_index_low"])
    expected = (
        expected_file_id["volume_serial"],
        expected_file_id["file_index_high"],
        expected_file_id["file_index_low"],
    )
    if observed != expected:
        _refuse("path_mismatch")


def _read_identity(backend, expected, clock, next_deadline, final=False):
    process = _process(_read(backend, "read_process", clock, next_deadline(), expected["pid"]))
    if process["pid"] != expected["pid"] or process["start_ticks"] != expected["start_ticks"]:
        _refuse("observation_changed" if final else "process_mismatch")
    if _path_key(process["executable_path"]) != _path_key(expected["engine_path"]):
        _refuse("observation_changed" if final else "process_mismatch")
    project = _project(_read(backend, "parse_project", clock, next_deadline(), process["command_line"]))
    if project["token_count"] != 1:
        _refuse("observation_changed" if final else "process_mismatch")
    if _path_key(project["project_path"]) != _path_key(expected["project_path"]):
        _refuse("observation_changed" if final else "path_mismatch")
    project_path = _path(
        _read(backend, "read_path", clock, next_deadline(), project["project_path"]),
        "backend.project_path",
    )
    engine_path = _path(
        _read(backend, "read_path", clock, next_deadline(), process["executable_path"]),
        "backend.engine_path",
    )
    try:
        _verify_path(project_path, expected["project_path"], expected["project_file_id"])
        _verify_path(engine_path, expected["engine_path"], expected["engine_file_id"])
    except _Refusal:
        _refuse("observation_changed" if final else "path_mismatch")
    return process, project["project_path"], project_path, engine_path


def _read_listeners(backend, expected, endpoint, policy, clock, next_deadline):
    rows = _read(backend, "read_listeners", clock, next_deadline(), endpoint["port"])
    if not isinstance(rows, list):
        _refuse("backend_malformed")
    if len(rows) > policy["max_listener_rows"]:
        _refuse("listener_overflow")
    result = []
    for index, raw in enumerate(rows):
        row = _listener(raw, endpoint)
        if row["owner_pid"] != expected["pid"]:
            # Do not read or inspect an unrelated process's command line.
            _refuse("listener_foreign")
        result.append(row)
    if not result:
        _refuse("listener_missing")
    return result


def _listener_key(row):
    return (row["family"], row["address"], row["port"], row["state"], row["owner_pid"])


def _attempt(sequence, expected, endpoint, backend, policy, clock, pair_start_mono, pair_start_utc, pair_deadline_ns):
    attempt_start_mono = clock.now()
    attempt_start_utc = _utc(backend)
    if attempt_start_mono < pair_start_mono:
        _refuse("clock_rollback")
    if attempt_start_utc < pair_start_utc:
        _refuse("clock_rollback")
    attempt_deadline_ns = attempt_start_mono + policy["attempt_ns"]
    now = attempt_start_mono
    if now >= attempt_deadline_ns or now >= pair_deadline_ns:
        _refuse("deadline_exceeded")

    def call_deadline():
        nonlocal now
        now = clock.now()
        if now < attempt_start_mono:
            _refuse("clock_rollback")
        limit = min(attempt_deadline_ns, pair_deadline_ns, now + policy["per_call_ns"])
        if now >= limit:
            _refuse("deadline_exceeded")
        return limit

    initial_process, initial_project, initial_project_path, initial_engine_path = _read_identity(
        backend, expected, clock, call_deadline, final=False
    )
    listeners = _read_listeners(
        backend, expected, endpoint, policy, clock, call_deadline
    )

    # Independent final process recheck, including parsed project/path IDs.
    final_process, final_project, final_project_path, final_engine_path = _read_identity(
        backend, expected, clock, call_deadline, final=True
    )
    final_listeners = _read_listeners(
        backend, expected, endpoint, policy, clock, call_deadline
    )

    if (
        initial_process != final_process
        or initial_project != final_project
        or initial_project_path != final_project_path
        or initial_engine_path != final_engine_path
    ):
        _refuse("observation_changed")
    if Counter(_listener_key(row) for row in listeners) != Counter(
        _listener_key(row) for row in final_listeners
    ):
        _refuse("observation_changed")

    end_mono = clock.now()
    end_utc = _utc(backend)
    if end_mono < attempt_start_mono:
        _refuse("clock_rollback")
    if end_mono >= attempt_deadline_ns or end_mono >= pair_deadline_ns:
        _refuse("deadline_exceeded")
    if end_utc < attempt_start_utc:
        _refuse("clock_rollback")
    if end_utc < expected["start_ticks"] - FILETIME_UNIX_EPOCH_TICKS:
        _refuse("clock_rollback")

    observation = {
        "sequence": sequence,
        "collected_at_utc": _format_utc(end_utc),
        "collection_ok": True,
        "process": {
            "pid": expected["pid"],
            "start_time_utc": filetime_to_utc(expected["start_ticks"]),
            "project_path": expected["project_path"],
            "engine_executable": expected["engine_path"],
        },
        "listeners": [
            {
                "family": row["family"],
                "address": row["address"],
                "port": row["port"],
                "state": row["state"],
                "pid": expected["pid"],
                "start_time_utc": filetime_to_utc(expected["start_ticks"]),
                "project_path": expected["project_path"],
                "engine_executable": expected["engine_path"],
            }
            for row in final_listeners
        ],
        "_collected_ticks": end_utc,
    }
    return observation


def _ceil_seconds(nanoseconds):
    return (nanoseconds + 999_999_999) // 1_000_000_000


def collect_pair(expected_record, endpoint, backend, policy):
    """Collect and validate two snapshots from an injected read-only backend.

    Returns a public success with a schema-v1 payload, or a structured refusal.
    The backend must enforce the absolute monotonic deadline passed to each read;
    this pure function cannot interrupt a backend that never returns.
    """
    try:
        expected = _expected_record(expected_record)
        supplied_endpoint = _endpoint(endpoint, "endpoint")
        if expected["endpoint"] != supplied_endpoint:
            _refuse("endpoint_mismatch")
        policy = _policy(policy)

        clock = _Clock(backend)
        pair_start_mono = clock.now()
        pair_start_utc = _utc(backend)
        pair_deadline_ns = pair_start_mono + policy["pair_ns"]
        observations = []
        for sequence in (1, 2):
            observations.append(
                _attempt(
                    sequence,
                    expected,
                    supplied_endpoint,
                    backend,
                    policy,
                    clock,
                    pair_start_mono,
                    pair_start_utc,
                    pair_deadline_ns,
                )
            )

        pair_end_mono = clock.now()
        pair_end_utc = _utc(backend)
        if pair_end_mono < pair_start_mono or pair_end_utc < pair_start_utc:
            _refuse("clock_rollback")
        if pair_end_mono >= pair_deadline_ns:
            _refuse("deadline_exceeded")

        first_ticks = observations[0]["_collected_ticks"]
        second_ticks = observations[1]["_collected_ticks"]
        if second_ticks < first_ticks:
            _refuse("observation_order")
        gap_ns = (second_ticks - first_ticks) * NS_PER_TICK
        if gap_ns > policy["snapshot_gap_ns"]:
            _refuse("stale_observation")
        for observation in observations:
            age_ns = (pair_end_utc - observation["_collected_ticks"]) * NS_PER_TICK
            if age_ns < 0:
                _refuse("clock_rollback")
            if age_ns > policy["age_ns"]:
                _refuse("stale_observation")

        max_age_seconds = max(1, _ceil_seconds(max(
            (pair_end_utc - observations[0]["_collected_ticks"]) * NS_PER_TICK,
            (pair_end_utc - observations[1]["_collected_ticks"]) * NS_PER_TICK,
        )))
        max_gap_seconds = _ceil_seconds(gap_ns)

        payload = {
            "schema": persistence_identity.SCHEMA,
            "expected": {
                "pid": expected["pid"],
                "start_time_utc": filetime_to_utc(expected["start_ticks"]),
                "project_path": expected["project_path"],
                "engine_executable": expected["engine_path"],
                "endpoint": dict(expected["endpoint"]),
            },
            "bounds": {
                "evaluated_at_utc": _format_utc(pair_end_utc),
                "max_age_seconds": max_age_seconds,
                "max_snapshot_gap_seconds": max_gap_seconds,
            },
            "observations": [
                {key: value for key, value in observation.items() if key != "_collected_ticks"}
                for observation in observations
            ],
        }
        validated = persistence_identity.validate_evidence(payload)
        if not validated.get("ok"):
            _refuse("validator_refused")
        return {"ok": True, "reason": "consistent_evidence", "payload": payload}
    except _Refusal as refusal:
        return _copy_public_refusal(refusal.reason)
