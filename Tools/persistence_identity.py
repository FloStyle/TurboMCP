"""Pure validation of injected evidence for a Windows editor/listener identity.

The module never collects evidence. It performs no process, network, filesystem,
environment, settings, lease or engine access. Callers must supply already
canonicalized records produced by a future trusted collector.

Schema version: ``turbomcp.persistence-identity/1``.

Top-level object (exact keys)::

    {
      "schema": "turbomcp.persistence-identity/1",
      "expected": {
        "pid": 1234,
        "start_time_utc": "2026-10-18T01:23:45.1234567Z",
        "project_path": "C:\\ExampleProject\\SampleRoot006.uproject",
        "engine_executable": "C:\\ExampleUnrealengine\\Engine\\Binaries\\Win64\\UnrealEditor.exe",
        "endpoint": {"host": "127.0.0.1", "port": 8000}
      },
      "bounds": {
        "evaluated_at_utc": "2026-10-18T01:23:50.1234567Z",
        "max_age_seconds": 10,
        "max_snapshot_gap_seconds": 5
      },
      "observations": [snapshot, snapshot]
    }

Each snapshot (exact keys)::

    {
      "sequence": 1,
      "collected_at_utc": "2026-10-18T01:23:45.1234567Z",
      "collection_ok": true,
      "process": {
        "pid": 1234,
        "start_time_utc": "2026-10-18T01:23:45.1234567Z",
        "project_path": "C:\\ExampleProject\\SampleRoot006.uproject",
        "engine_executable": "C:\\ExampleUnrealengine\\Engine\\Binaries\\Win64\\UnrealEditor.exe"
      },
      "listeners": [
        {
          "family": "ipv4",
          "address": "127.0.0.1",
          "port": 8000,
          "state": "listen",
          "pid": 1234,
          "start_time_utc": "2026-10-18T01:23:45.1234567Z",
          "project_path": "C:\\ExampleProject\\SampleRoot006.uproject",
          "engine_executable": "C:\\ExampleUnrealengine\\Engine\\Binaries\\Win64\\UnrealEditor.exe"
        }
      ]
    }

Only literal loopback endpoints ``127.0.0.1`` and ``::1`` are accepted. Listener
matching is by address family: the exact endpoint address or that family's
wildcard. Multiple rows owned by the same verified process are allowed. Multiple
owners, missing coverage, and other-family wildcard rows without one verified
owner are refused. An other-family wildcard is treated as explicit dual-stack
coverage only when the endpoint family is covered and every supplied wildcard
row belongs to the same verified process; otherwise it is ambiguous.

Both snapshots must be within ``max_age_seconds`` of ``evaluated_at_utc``, in
sequence/time order, no more than ``max_snapshot_gap_seconds`` apart, and must
agree on process and listener identity. The result proves only that the supplied
evidence is internally consistent. It does not prove live ownership, and a
connection made after validation is subject to a residual race between the
snapshot and the connect. The collector must be trusted to read the process and
TCP tables truthfully and atomically enough for the stated bounds.

``validate_evidence(payload)`` returns an object with ``ok``, ``reason``,
``detail`` and, on success, ``normalized``. Refusal reasons are explicit codes in
``REASON_CODES``. Invalid input never raises.
"""

from __future__ import annotations

import datetime as _datetime
import ntpath
import re

SCHEMA = "turbomcp.persistence-identity/1"

PASS = "consistent_evidence"
INVALID_SCHEMA = "invalid_schema"
MISSING_FIELD = "missing_field"
UNKNOWN_FIELD = "unknown_field"
INVALID_TYPE = "invalid_type"
INVALID_VALUE = "invalid_value"
COLLECTION_FAILED = "collection_failed"
PROCESS_MISMATCH = "process_identity_mismatch"
LISTENER_MISSING = "listener_missing"
LISTENER_FOREIGN = "foreign_listener"
LISTENER_MULTIPLE_OWNERS = "multiple_listener_owners"
LISTENER_AMBIGUOUS = "ambiguous_listener_coverage"
LISTENER_PORT_MISMATCH = "listener_port_mismatch"
STALE = "stale_observation"
ORDER = "observation_order"
CHANGED = "observation_changed"

REASON_CODES = frozenset({
    PASS, INVALID_SCHEMA, MISSING_FIELD, UNKNOWN_FIELD, INVALID_TYPE,
    INVALID_VALUE, COLLECTION_FAILED, PROCESS_MISMATCH, LISTENER_MISSING,
    LISTENER_FOREIGN, LISTENER_MULTIPLE_OWNERS, LISTENER_AMBIGUOUS,
    LISTENER_PORT_MISMATCH, STALE, ORDER, CHANGED,
})

_ROOT_KEYS = {"schema", "expected", "bounds", "observations"}
_EXPECTED_KEYS = {"pid", "start_time_utc", "project_path", "engine_executable", "endpoint"}
_ENDPOINT_KEYS = {"host", "port"}
_BOUNDS_KEYS = {"evaluated_at_utc", "max_age_seconds", "max_snapshot_gap_seconds"}
_SNAPSHOT_KEYS = {"sequence", "collected_at_utc", "collection_ok", "process", "listeners"}
_PROCESS_KEYS = {"pid", "start_time_utc", "project_path", "engine_executable"}
_LISTENER_KEYS = {
    "family", "address", "port", "state", "pid", "start_time_utc",
    "project_path", "engine_executable",
}

_MAX_PATH = 4096
_MAX_LISTENERS = 32
_MAX_AGE = 300
_MAX_GAP = 300
_MAX_TIMESTAMP = 40
_TIMESTAMP_RE = re.compile(
    r"^(?P<date>\d{4}-\d{2}-\d{2})T(?P<time>\d{2}:\d{2}:\d{2})\.(?P<frac>\d{7})Z$"
)
_DRIVE_RE = re.compile(r"^[A-Za-z]:$")
_RESERVED_RE = re.compile(
    r"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?$", re.IGNORECASE
)


class _Refusal(Exception):
    def __init__(self, reason, detail):
        super().__init__(detail)
        self.reason = reason
        self.detail = detail


def _refuse(reason, detail):
    raise _Refusal(reason, detail)


def _fail(reason, detail):
    return {"ok": False, "reason": reason, "detail": detail}


def _dict(value, keys, label):
    if not isinstance(value, dict):
        _refuse(INVALID_TYPE, "%s must be an object" % label)
    missing = sorted(keys - set(value))
    if missing:
        _refuse(MISSING_FIELD, "%s missing field(s): %s" % (label, ", ".join(missing)))
    unknown = sorted(set(value) - keys)
    if unknown:
        _refuse(UNKNOWN_FIELD, "%s has unknown field(s): %s" % (label, ", ".join(unknown)))
    return value


def _list(value, label):
    if not isinstance(value, list):
        _refuse(INVALID_TYPE, "%s must be a list" % label)
    return value


def _string(value, label, maximum=256):
    if not isinstance(value, str):
        _refuse(INVALID_TYPE, "%s must be a string" % label)
    if not value:
        _refuse(INVALID_VALUE, "%s must not be empty" % label)
    if len(value) > maximum:
        _refuse(INVALID_VALUE, "%s is longer than %d characters" % (label, maximum))
    return value


def _integer(value, label, low, high):
    if isinstance(value, bool) or not isinstance(value, int):
        _refuse(INVALID_TYPE, "%s must be an integer, not a bool or other type" % label)
    if value < low or value > high:
        _refuse(INVALID_VALUE, "%s must be in %d..%d" % (label, low, high))
    return value


def _boolean(value, label):
    if not isinstance(value, bool):
        _refuse(INVALID_TYPE, "%s must be a boolean" % label)
    return value


class _Timestamp:
    __slots__ = ("text", "ticks")

    def __init__(self, text, ticks):
        self.text = text
        self.ticks = ticks


_EPOCH = _datetime.datetime(1970, 1, 1)


def _timestamp(value, label):
    text = _string(value, label, _MAX_TIMESTAMP)
    match = _TIMESTAMP_RE.fullmatch(text)
    if not match:
        _refuse(INVALID_VALUE, "%s must be canonical UTC YYYY-MM-DDTHH:MM:SS.fffffffZ" % label)
    try:
        base = _datetime.datetime.strptime(
            match.group("date") + "T" + match.group("time") + "Z",
            "%Y-%m-%dT%H:%M:%SZ",
        )
    except ValueError:
        _refuse(INVALID_VALUE, "%s is not a valid date/time" % label)
    ticks = int((base - _EPOCH).total_seconds()) * 10_000_000 + int(match.group("frac"))
    return _Timestamp(text, ticks)


def _windows_path(value, label, extension):
    text = _string(value, label, _MAX_PATH)
    if text.startswith("\\\\"):
        _refuse(INVALID_VALUE, "%s must not be a UNC or device path" % label)
    if "/" in text:
        _refuse(INVALID_VALUE, "%s must use Windows backslash separators" % label)
    drive, tail = ntpath.splitdrive(text)
    if not _DRIVE_RE.fullmatch(drive):
        _refuse(INVALID_VALUE, "%s must be an absolute drive-letter Windows path" % label)
    if not tail.startswith("\\") or tail.startswith("\\\\"):
        _refuse(INVALID_VALUE, "%s must be an absolute canonical Windows path" % label)
    if text.endswith("\\"):
        _refuse(INVALID_VALUE, "%s must name a file, not a directory" % label)
    parts = tail[1:].split("\\")
    for part in parts:
        if not part:
            _refuse(INVALID_VALUE, "%s contains an empty path segment" % label)
        if part in (".", ".."):
            _refuse(INVALID_VALUE, "%s contains dot-segment ambiguity" % label)
        if ":" in part:
            _refuse(INVALID_VALUE, "%s contains a colon outside the drive" % label)
        if part.endswith(".") or part.endswith(" "):
            _refuse(INVALID_VALUE, "%s contains a segment with a trailing dot or space" % label)
        if _RESERVED_RE.fullmatch(part):
            _refuse(INVALID_VALUE, "%s contains a Windows device name" % label)
    if ntpath.splitext(parts[-1])[1].casefold() != extension:
        _refuse(INVALID_VALUE, "%s must end in %s" % (label, extension))
    return text


def _endpoint(value):
    item = _dict(value, _ENDPOINT_KEYS, "expected.endpoint")
    host = _string(item["host"], "expected.endpoint.host", 16)
    if host not in ("127.0.0.1", "::1"):
        _refuse(INVALID_VALUE, "expected.endpoint.host must be literal 127.0.0.1 or ::1")
    port = _integer(item["port"], "expected.endpoint.port", 1, 65535)
    return {"host": host, "port": port, "family": "ipv4" if host == "127.0.0.1" else "ipv6"}


def _identity(item, label):
    return {
        "pid": _integer(item["pid"], label + ".pid", 1, 2**32 - 1),
        "start_time_utc": _timestamp(item["start_time_utc"], label + ".start_time_utc"),
        "project_path": _windows_path(item["project_path"], label + ".project_path", ".uproject"),
        "engine_executable": _windows_path(
            item["engine_executable"], label + ".engine_executable", ".exe"
        ),
    }


def _process(value, label):
    item = _dict(value, _PROCESS_KEYS, label)
    return _identity(item, label)


def _owner(identity):
    return (
        identity["pid"],
        identity["start_time_utc"].text,
        identity["project_path"].casefold(),
        identity["engine_executable"].casefold(),
    )


def _listener(value, endpoint, label):
    item = _dict(value, _LISTENER_KEYS, label)
    family = _string(item["family"], label + ".family", 8)
    if family not in ("ipv4", "ipv6"):
        _refuse(INVALID_VALUE, label + ".family must be ipv4 or ipv6")
    address = _string(item["address"], label + ".address", 32)
    allowed = {"ipv4": {"127.0.0.1", "0.0.0.0"}, "ipv6": {"::1", "::"}}[family]
    if address not in allowed:
        _refuse(INVALID_VALUE, label + ".address is not an accepted loopback/wildcard address")
    port = _integer(item["port"], label + ".port", 1, 65535)
    if port != endpoint["port"]:
        _refuse(LISTENER_PORT_MISMATCH, label + ".port does not match the configured endpoint")
    state = _string(item["state"], label + ".state", 16)
    if state != "listen":
        _refuse(INVALID_VALUE, label + ".state must be listen")
    identity = _identity(item, label)
    return {
        "family": family,
        "address": address,
        "port": port,
        "state": state,
        "identity": identity,
        "owner": _owner(identity),
    }


def _snapshot(value, endpoint, index):
    label = "observations[%d]" % index
    item = _dict(value, _SNAPSHOT_KEYS, label)
    sequence = _integer(item["sequence"], label + ".sequence", 1, 2)
    collected = _timestamp(item["collected_at_utc"], label + ".collected_at_utc")
    collection_ok = _boolean(item["collection_ok"], label + ".collection_ok")
    if not collection_ok:
        if item["process"] is not None or item["listeners"] != []:
            _refuse(COLLECTION_FAILED, label + " failed collection but supplied partial records")
        return {"sequence": sequence, "collected": collected, "ok": False}
    process = _process(item["process"], label + ".process")
    listeners_raw = _list(item["listeners"], label + ".listeners")
    if len(listeners_raw) > _MAX_LISTENERS:
        _refuse(INVALID_VALUE, label + ".listeners has more than %d rows" % _MAX_LISTENERS)
    listeners = [
        _listener(raw, endpoint, "%s.listeners[%d]" % (label, i))
        for i, raw in enumerate(listeners_raw)
    ]
    return {
        "sequence": sequence,
        "collected": collected,
        "ok": True,
        "process": process,
        "listeners": listeners,
    }


def _listener_set(listeners, endpoint, expected):
    endpoint_family = endpoint["family"]
    exact = endpoint["host"]
    wildcard = "0.0.0.0" if endpoint_family == "ipv4" else "::"
    other_loopback = "::1" if endpoint_family == "ipv4" else "127.0.0.1"
    other_wildcard = "::" if endpoint_family == "ipv4" else "0.0.0.0"

    relevant = []
    dual = []
    for row in listeners:
        if row["family"] == endpoint_family:
            if row["address"] not in (exact, wildcard):
                _refuse(INVALID_VALUE, "listener address is not this endpoint's loopback or wildcard")
            relevant.append(row)
        elif row["address"] == other_loopback:
            # An exact loopback listener in the other family is a different endpoint.
            continue
        elif row["address"] == other_wildcard:
            dual.append(row)
        else:
            _refuse(INVALID_VALUE, "listener address is not loopback or a family wildcard")

    if not relevant:
        if dual:
            _refuse(LISTENER_AMBIGUOUS, "only an other-family wildcard was supplied; coverage is ambiguous")
        _refuse(LISTENER_MISSING, "no listener covers the configured loopback endpoint")

    owners = {row["owner"] for row in relevant}
    if len(owners) > 1:
        _refuse(LISTENER_MULTIPLE_OWNERS, "relevant listener rows have multiple owners")
    owner = next(iter(owners))
    expected_owner = _owner(expected)
    if owner != expected_owner:
        _refuse(LISTENER_FOREIGN, "relevant listener is owned by a different process identity")

    if dual:
        dual_owners = {row["owner"] for row in dual}
        if dual_owners != {owner}:
            _refuse(LISTENER_AMBIGUOUS, "other-family wildcard coverage is not owned by the same process")

    normalized = sorted(
        (row["family"], row["address"], row["port"], row["state"], row["owner"])
        for row in relevant + dual
    )
    return normalized


def _validate(payload):
    root = _dict(payload, _ROOT_KEYS, "evidence")
    if root["schema"] != SCHEMA:
        _refuse(INVALID_SCHEMA, "schema must be %s" % SCHEMA)

    expected_raw = _dict(root["expected"], _EXPECTED_KEYS, "expected")
    endpoint = _endpoint(expected_raw["endpoint"])
    expected = _identity(expected_raw, "expected")

    bounds = _dict(root["bounds"], _BOUNDS_KEYS, "bounds")
    evaluated = _timestamp(bounds["evaluated_at_utc"], "bounds.evaluated_at_utc")
    max_age = _integer(bounds["max_age_seconds"], "bounds.max_age_seconds", 1, _MAX_AGE)
    max_gap = _integer(
        bounds["max_snapshot_gap_seconds"], "bounds.max_snapshot_gap_seconds", 0, _MAX_GAP
    )

    observations = _list(root["observations"], "observations")
    if len(observations) != 2:
        _refuse(INVALID_VALUE, "observations must contain exactly two snapshots")
    snapshots = [
        _snapshot(raw, endpoint, index) for index, raw in enumerate(observations)
    ]
    if any(not snap["ok"] for snap in snapshots):
        _refuse(COLLECTION_FAILED, "one or both observations failed collection")
    if [snap["sequence"] for snap in snapshots] != [1, 2]:
        _refuse(ORDER, "observations must be sequence 1 then 2")
    first = snapshots[0]["collected"]
    second = snapshots[1]["collected"]
    if first.ticks > second.ticks:
        _refuse(ORDER, "the second observation predates the first")
    if first.ticks > evaluated.ticks or second.ticks > evaluated.ticks:
        _refuse(ORDER, "an observation is later than evaluated_at_utc")
    if evaluated.ticks - first.ticks > max_age * 10_000_000:
        _refuse(STALE, "the first observation is older than max_age_seconds")
    if evaluated.ticks - second.ticks > max_age * 10_000_000:
        _refuse(STALE, "the second observation is older than max_age_seconds")
    if second.ticks - first.ticks > max_gap * 10_000_000:
        _refuse(STALE, "the observations exceed max_snapshot_gap_seconds")

    normalized_listeners = []
    for index, snap in enumerate(snapshots):
        if _owner(snap["process"]) != _owner(expected):
            _refuse(PROCESS_MISMATCH, "snapshot %d process does not match expected identity" % (index + 1))
        normalized_listeners.append(_listener_set(snap["listeners"], endpoint, expected))

    if _owner(snapshots[0]["process"]) != _owner(snapshots[1]["process"]):
        _refuse(CHANGED, "process identity changed between observations")
    if normalized_listeners[0] != normalized_listeners[1]:
        _refuse(CHANGED, "listener identity or ownership changed between observations")

    return {
        "schema": SCHEMA,
        "pid": expected["pid"],
        "start_time_utc": expected["start_time_utc"].text,
        "endpoint": {"host": endpoint["host"], "port": endpoint["port"]},
        "project_path": expected["project_path"],
        "engine_executable": expected["engine_executable"],
        "listeners": normalized_listeners[0],
    }


def validate_evidence(payload):
    """Validate one already-collected evidence object.

    Returns a plain dict. Invalid input is refused, never raised. A pass means
    only that the supplied snapshots agree with the expected identity; it is not
    proof of live ownership or of a future connection.
    """
    try:
        normalized = _validate(payload)
    except _Refusal as refusal:
        return _fail(refusal.reason, refusal.detail)
    return {
        "ok": True,
        "reason": PASS,
        "detail": "two snapshots agree on the supplied process/listener identity",
        "normalized": normalized,
    }
