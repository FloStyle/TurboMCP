# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""The TurboMCP result contract: one authoritative final line, parsed by the client.

WHY THIS EXISTS
    Release findings R05, R06 and R07 all come from one hole: a tool's outcome could not be determined from its
    answer. A refusal and a success were both MCP transport successes, an agent had to read prose to decide what
    happened, and the CLI's exit status was derived from a regex over the text. ARC02 settles this with a single
    machine-readable line appended by the OUTER tool boundary:

        ; tmcp-result-v1: {"schema":1,"outcome":"changed","code":"ok","changed":true,"assets":["/Game/Example"],
                           "compile":"passed","rollback":"not_needed","save":"not_requested","complete":true,
                           "operationId":"opaque-id"}

    Three rules make it usable rather than decorative, and each is a release requirement:

      * THE FINAL LINE IS AUTHORITATIVE. Text inside asset content is not metadata, so a body that quotes a
        result line must not be able to forge one (C05). Only the last line is read, and it must be the last
        thing in the answer.
      * MALFORMED OR MISSING METADATA FROM A MIGRATED TOOL IS A PROTOCOL FAILURE, NEVER SUCCESS (C04). Silence
        must not read as "fine".
      * METADATA IS NOT TRUNCATED BY AN OUTPUT BUDGET (C08). A truncated answer may lose body text and must say
        so; it may not lose the outcome.

    THIS MODULE IS THE PURE HALF. It formats and parses; it does not talk to a server. That is what makes C03 to
    C08 testable without an editor, which is what the acceptance catalog asks for.

WHAT IT DELIBERATELY DOES NOT DO
    It does not validate that `outcome` agrees with the body's prose, because that is not decidable. It does not
    infer an outcome from text. It does not treat a missing line as an implicit success. And it does not claim
    the engine's MCP dispatch reports a plugin refusal as a transport error - it does not, and ARC02 records
    that as a documented limitation rather than something to patch in this release.
"""

from __future__ import annotations

import hashlib
import json
import re
import uuid

#: The marker that begins the authoritative final line.
PREFIX = "; tmcp-result-v1:"

#: The schema version this module reads and writes.
SCHEMA = 1

#: Every defined outcome, from ARC02's table.
OUTCOMES = ("read", "planned", "unchanged", "changed", "refused", "failed", "partial", "outcome_unknown")

#: Every defined value for the closed enumerations, so a typo is a protocol failure rather than a surprise.
ENUMERATIONS = {
    "compile": ("passed", "failed", "not_applicable", "not_run"),
    "rollback": ("complete", "failed", "not_needed", "not_supported"),
    "save": ("saved", "failed", "partial", "not_requested", "not_applicable"),
}

#: The keys every result line must carry.
REQUIRED_KEYS = ("schema", "outcome", "code", "changed", "assets", "compile", "rollback", "save",
                 "complete", "operationId")

#: Exit codes for the CLI, from ARC02. Distinct on purpose: 2 means "the outcome is not established", which is
#: neither success nor a refusal, and collapsing it into 1 would hide the one case an agent must investigate.
EXIT_OK = 0
EXIT_FAILED = 1
EXIT_UNKNOWN = 2


class ResultProtocolError(ValueError):
    """A migrated tool's answer did not carry a valid result line.

    This is NOT a tool refusal and NOT a failure: it is the absence of a decidable outcome, which is why it maps
    to exit 2 rather than 1. Calling it success is the defect this class exists to prevent.
    """


class TurboMCPResult:
    """One parsed result line, with the fields ARC02 defines."""

    def __init__(self, fields, raw=""):
        self.fields = fields
        self.raw = raw

    # --- the fields, by name, with no inference -------------------------------------------------
    @property
    def schema(self):
        return self.fields.get("schema")

    @property
    def outcome(self):
        return self.fields.get("outcome")

    @property
    def code(self):
        return self.fields.get("code")

    @property
    def changed(self):
        return self.fields.get("changed")

    @property
    def assets(self):
        return self.fields.get("assets") or []

    @property
    def compile(self):
        return self.fields.get("compile")

    @property
    def rollback(self):
        return self.fields.get("rollback")

    @property
    def save(self):
        return self.fields.get("save")

    @property
    def complete(self):
        return self.fields.get("complete")

    @property
    def operation_id(self):
        return self.fields.get("operationId")

    @property
    def diagnostics(self):
        return self.fields.get("diagnostics") or []

    @property
    def effects(self):
        return self.fields.get("effects") or []

    @property
    def truncated(self):
        return bool(self.fields.get("truncated"))

    @property
    def omitted(self):
        return self.fields.get("omitted")

    def suggest_exit_code(self):
        """ARC02's mapping from an operation outcome to a process exit code.

        `outcome_unknown`, a FAILED ROLLBACK, and an incomplete answer all mean "the caller cannot conclude that
        the requested change happened", so they share exit 2 with a protocol failure. Treating any of them as 0
        is the defect this function exists to make impossible.

        *** WHY A FAILED ROLLBACK IS 2 AND NOT 1. *** ARC05 is explicit: when restoration fails, the tool
        reports `failed` with `rollback=failed` and `changed=null`, and subsequent plugin mutations are refused
        until the session recovers. Exit 1 would say "this operation failed, nothing changed", which is exactly
        the wrong thing for a caller to conclude - the asset may be in an unknown state and a human has to look.
        So the outcome decides first (a plain failure is 1), and a failed restoration overrides it to 2.
        """
        if self.outcome == "outcome_unknown":
            return EXIT_UNKNOWN
        if self.rollback == "failed":
            return EXIT_UNKNOWN
        if self.outcome in ("failed", "refused", "partial"):
            return EXIT_FAILED
        if self.schema != SCHEMA:
            return EXIT_UNKNOWN
        if self.complete is False:
            return EXIT_UNKNOWN
        if self.rollback == "not_supported" and self.outcome == "changed":
            return EXIT_UNKNOWN
        return EXIT_OK

    def __repr__(self):
        return "TurboMCPResult(%s/%s, complete=%s)" % (self.outcome, self.code, self.complete)


def new_operation_id(prefix=""):
    """A unique id for one invocation.

    ARC02 is explicit that this is an identifier and NOT a deduplication guarantee: the release does not claim
    that replaying an operation id is safe, which is why R02's client refuses to replay a mutation at all.
    """
    return "%s%s" % (prefix, uuid.uuid4())


def format_result(outcome, code="ok", changed=None, assets=(), compile="not_applicable",
                  rollback="not_needed", save="not_requested", complete=True,
                  operation_id=None, diagnostics=None, effects=None, extra=None):
    """The authoritative final line, as text.

    The caller is expected to have decided every field. The values are validated here rather than at the end of
    a long report, so a misspelled outcome fails where it is written instead of producing a line no client can
    read.
    """
    if outcome not in OUTCOMES:
        raise ResultProtocolError("outcome %r is not one of %s" % (outcome, ", ".join(OUTCOMES)))
    for key, allowed in ENUMERATIONS.items():
        value = {"compile": compile, "rollback": rollback, "save": save}[key]
        if value not in allowed:
            raise ResultProtocolError("%s=%r is not one of %s" % (key, value, ", ".join(allowed)))
    if changed not in (True, False, None):
        raise ResultProtocolError("changed must be true, false or null, not %r" % (changed,))
    if not code:
        raise ResultProtocolError("code must be a stable non-empty diagnostic code")
    if not re.match(r"^[a-z][a-z0-9_]*$", code):
        raise ResultProtocolError("code %r is not snake_case" % code)

    fields = {
        "schema": SCHEMA,
        "outcome": outcome,
        "code": code,
        "changed": changed,
        "assets": list(assets),
        "compile": compile,
        "rollback": rollback,
        "save": save,
        "complete": bool(complete),
        "operationId": operation_id or new_operation_id(),
    }
    if diagnostics:
        fields["diagnostics"] = list(diagnostics)
    if effects:
        fields["effects"] = list(effects)
    if extra:
        for key, value in extra.items():
            if key in fields:
                raise ResultProtocolError("extra key %r would overwrite a defined field" % key)
            fields[key] = value

    # `separators` with a space after the colon, because that is the shape ARC02 shows and a client reading it
    # by eye should not have to care which writer produced it.
    return "%s %s" % (PREFIX, json.dumps(fields, sort_keys=False, separators=(", ", ": ")))


def parse_result_line(text):
    """Parse ONE line that must be the result line. Raises ResultProtocolError on any defect.

    Every check below exists because a client that guesses is worse than a client that refuses. In particular:
      * the prefix must be at the START of the line, so a result-looking string inside prose is not a result;
      * unknown outcomes and out-of-range enumerations are refused rather than passed through;
      * `null` is allowed for `changed` and means unknown, which is different from `false`.
    """
    if text is None:
        raise ResultProtocolError("no result line was supplied")
    line = text.strip()
    if not line.startswith(PREFIX):
        raise ResultProtocolError("the answer does not end with a %r line" % PREFIX)
    body = line[len(PREFIX):].strip()
    if not body:
        raise ResultProtocolError("the result line carries no JSON object")
    try:
        fields = json.loads(body)
    except ValueError as error:
        raise ResultProtocolError("the result line is not valid JSON: %s" % error)
    if not isinstance(fields, dict):
        raise ResultProtocolError("the result line is JSON but not an object")

    missing = [key for key in REQUIRED_KEYS if key not in fields]
    if missing:
        raise ResultProtocolError("the result line is missing %s" % ", ".join(missing))
    if fields["schema"] != SCHEMA:
        raise ResultProtocolError("schema %r is not the supported version %d" % (fields["schema"], SCHEMA))
    if fields["outcome"] not in OUTCOMES:
        raise ResultProtocolError("outcome %r is not one of %s" % (fields["outcome"], ", ".join(OUTCOMES)))
    for key, allowed in ENUMERATIONS.items():
        if fields[key] not in allowed:
            raise ResultProtocolError("%s=%r is not one of %s" % (key, fields[key], ", ".join(allowed)))
    if fields["changed"] not in (True, False, None):
        raise ResultProtocolError("changed must be true, false or null")
    if not isinstance(fields["assets"], list):
        raise ResultProtocolError("assets must be a list of canonical paths")
    if not isinstance(fields["complete"], bool):
        raise ResultProtocolError("complete must be a boolean")
    if not fields["operationId"]:
        raise ResultProtocolError("operationId must be non-empty")
    return TurboMCPResult(fields, raw=line)


def split_answer(text):
    """The body and the result line, or (body, None) when there is no result line.

    THE LINE MUST BE LAST. A body that quotes a result line - an asset whose content contains one, a help page
    that documents the format - must not be able to masquerade as the outcome, so anything after the line means
    the line was not authoritative and the answer has no result. That is C05's whole point, and it is why this
    function returns the FIRST marker found from the end rather than searching the whole text.
    """
    if text is None:
        return "", None
    lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    while lines and not lines[-1].strip():
        lines.pop()
    if not lines:
        return "", None
    candidate = lines[-1].strip()
    if not candidate.startswith(PREFIX):
        return text, None
    return "\n".join(lines[:-1]), candidate


def parse_answer(text):
    """The result line of a whole answer, or None when it carries none.

    Returns None for a MISSING line rather than raising, because a native engine tool legitimately has none and
    the caller decides what that means (C07). A line that is PRESENT but invalid raises, because that is the
    case where silence would read as success (C04).
    """
    _, line = split_answer(text)
    if line is None:
        return None
    return parse_result_line(line)


def body_of(text):
    """The compatibility view: the answer without the metadata line.

    ARC02 requires the human-readable body to survive for the existing text method, so this is what that method
    returns. It never strips a line that is not a valid result - a malformed line is left visible in the body,
    where a reader can see it, rather than silently removed.
    """
    body, line = split_answer(text)
    if line is None:
        return text
    return body


def canonical_assets(paths):
    """Canonical asset paths, sorted and de-duplicated, so a fingerprint over them is stable.

    ARC06 requires that unordered sets be sorted before hashing, and `assets` is a set in practice. Sorting here
    rather than at every call site is the difference between a fingerprint that is comparable and one that
    depends on iteration order.
    """
    seen = []
    for path in paths:
        text = str(path).strip()
        if text and text not in seen:
            seen.append(text)
    return sorted(seen)


def fingerprint(data):
    """A deterministic SHA-256 over canonical semantic data.

    ARC06: sort unordered sets and maps before hashing; include the things that decide behaviour and nothing
    that does not. This helper is the pure part - the caller assembles `data` from semantic fields only, and
    must not include transient pointers, timestamps, UI selection or log contents.
    """
    canonical = json.dumps(_canonicalise(data), sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def _canonicalise(value):
    if isinstance(value, dict):
        return {str(key): _canonicalise(value[key]) for key in sorted(value, key=str)}
    if isinstance(value, (list, tuple)):
        items = [_canonicalise(item) for item in value]
        # A list of comparable scalars is treated as a SET, because the sets ARC06 names are exactly these.
        if all(isinstance(item, (str, int, float, bool)) or item is None for item in items):
            try:
                return sorted(items, key=lambda item: (str(type(item)), str(item)))
            except TypeError:
                return items
        return items
    return value


def apply_output_budget(text, budget, reserve=None):
    """Trim an answer's BODY to a character budget while keeping the metadata intact.

    *** METADATA IS RESERVED BEFORE THE BUDGET IS APPLIED, WHICH IS C08. *** A budget that could cut the result
    line would turn a decidable outcome into a protocol failure, which is precisely backwards: the whole point of
    the line is that it survives. So the line is measured first, the reserve is at least its length, and the body
    records how much was omitted and what to do next - an omitted list must name its omitted count and a
    continuation, per ARC02.
    """
    body, line = split_answer(text)
    if line is None:
        # No metadata to protect. The caller gets a plain trim and a marker, and nothing claims otherwise.
        if budget is None or len(body) <= budget:
            return text, False
        return body[:max(0, budget)] + "\n; truncated: %d character(s) omitted" % (len(body) - budget), True

    if budget is None:
        return text, False

    needed = len(line) + 1
    reserve = max(reserve or 0, needed)
    notice_template = ("\n; truncated: %d character(s) omitted from this answer. Ask for a narrower query, or "
                       "use the tool's own continuation parameters.")
    # The notice is measured WITH a placeholder count so its real length is known before the body is cut. The
    # answer must fit the budget it was given; a notice that overflowed it would be the same defect as a lost
    # metadata line, one step smaller.
    notice_floor = len(notice_template % 999999)
    if budget < reserve:
        # THE BUDGET CANNOT HOLD THE METADATA. Truncating it would be a protocol failure, so the metadata wins
        # and the body is emptied: an answer with a decidable outcome and no body is strictly better than an
        # answer with a body and an undecidable outcome.
        return line, True

    room = budget - reserve
    if len(body) <= room:
        return text, False

    # Make room for the notice, so `body + notice + line` fits the budget exactly.
    room = max(0, room - notice_floor - 1)
    omitted = len(body) - room
    trimmed = body[:room] + (notice_template % omitted)
    return trimmed + "\n" + line, True


def mark_truncated(result, omitted):
    """Add an explicit omission record to a result line, keeping the line valid.

    Used when the body was trimmed and the caller still has the parsed result: the fields `truncated` and
    `omitted` are additive, so a client that does not know them ignores them and a client that does can tell the
    answer from a complete one.
    """
    fields = dict(result.fields)
    fields["truncated"] = True
    if omitted is not None:
        fields["omitted"] = int(omitted)
    return format_line_from_fields(fields)


def format_line_from_fields(fields):
    """Re-emit a line from already-validated fields. Used by `mark_truncated` and by tests."""
    missing = [key for key in REQUIRED_KEYS if key not in fields]
    if missing:
        raise ResultProtocolError("cannot re-emit a line missing %s" % ", ".join(missing))
    return "%s %s" % (PREFIX, json.dumps(fields, sort_keys=False, separators=(", ", ": ")))


