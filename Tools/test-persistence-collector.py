"""Offline behavior tests for Tools/persistence_collector.py.

The fake backend supplies records and clocks only; it never reads a process,
socket or file. These tests prove orchestration behavior, not a Windows adapter.
"""

from __future__ import annotations

import copy
import unittest

import persistence_collector as collector


PROJECT = r"C:\ExampleDev\TurboMCP\TestProject\TurboMCPTest.uproject"
ENGINE = r"C:\ExampleUnrealengine\Engine\Binaries\Win64\UnrealEditor.exe"
START = "2026-10-18T00:59:59.0000000Z"
NOW = "2026-10-18T01:00:00.0000000Z"
PID = 1234
ENDPOINT = {"host": "127.0.0.1", "port": 8000}


def file_id(volume=0, high=0, low=0, link=1):
    return {
        "volume_serial": volume,
        "file_index_high": high,
        "file_index_low": low,
        "link_count": link,
    }


def path_value(path, volume=0, high=0, low=0, link=1, exists=True,
               reparse=False, short=False, long_path=None):
    return {
        "path": path,
        "exists": exists,
        "has_reparse_component": reparse,
        "has_short_name_component": short,
        "long_path": path if long_path is None else long_path,
        "volume_serial": volume,
        "file_index_high": high,
        "file_index_low": low,
        "link_count": link,
    }


def listener(owner=PID, family="ipv4", address="127.0.0.1", port=8000):
    return {
        "family": family,
        "address": address,
        "port": port,
        "state": "listen",
        "owner_pid": owner,
    }


def expected():
    return {
        "schema": collector.EXPECTED_SCHEMA,
        "launch_id": "launch-1",
        "pid": PID,
        "editor_start_ticks": collector.utc_to_filetime_ticks(START),
        "project": {"path": PROJECT, "file_id": file_id()},
        "engine": {"path": ENGINE, "file_id": file_id()},
        "endpoint": copy.deepcopy(ENDPOINT),
    }


def policy():
    return collector.default_policy()


class ScriptedBackend:
    def __init__(self):
        self.calls = []
        self.mono = 10**15
        self.mono_values = None
        self.utc = NOW
        self.utc_values = None
        self.process_values = None
        self.parse_values = None
        self.listener_values = None
        self.path_values = {}
        self.process_exception = None
        self.listeners_exception = None
        self.last_listener_value = None

    def _pop(self, attr):
        values = getattr(self, attr)
        if values is None:
            return None
        if not values:
            raise AssertionError("scripted backend exhausted: " + attr)
        return values.pop(0)

    def monotonic_ns(self):
        self.calls.append(("monotonic",))
        if self.mono_values is not None:
            if self.mono_values:
                return self.mono_values.pop(0)
            return self.mono
        self.mono += 1
        return self.mono

    def utc_now(self):
        self.calls.append(("utc",))
        value = self._pop("utc_values")
        return self.utc if value is None else value

    def read_process(self, pid, deadline):
        self.calls.append(("process", pid, deadline))
        if self.process_exception:
            raise self.process_exception
        value = self._pop("process_values")
        if value is not None:
            return value
        return {"ok": True, "value": {
            "pid": PID,
            "start_ticks": collector.utc_to_filetime_ticks(START),
            "executable_path": ENGINE,
            "command_line": '"' + ENGINE + '" "' + PROJECT + '"',
        }}

    def parse_project(self, command_line, deadline):
        self.calls.append(("parse", deadline))
        value = self._pop("parse_values")
        if value is not None:
            return value
        return {"ok": True, "value": {"project_path": PROJECT, "token_count": 1}}

    def read_path(self, path, deadline):
        self.calls.append(("path", path, deadline))
        if path in self.path_values and self.path_values[path]:
            return {"ok": True, "value": self.path_values[path].pop(0)}
        return {"ok": True, "value": path_value(path)}

    def read_listeners(self, port, deadline):
        self.calls.append(("listeners", port, deadline))
        if self.listeners_exception:
            raise self.listeners_exception
        value = self._pop("listener_values")
        if value is not None:
            self.last_listener_value = value
            return value
        value = [listener()]
        self.last_listener_value = value
        return {"ok": True, "value": value}


class CollectorTests(unittest.TestCase):
    def success(self, backend=None, exp=None, pol=None, endpoint=None):
        backend = ScriptedBackend() if backend is None else backend
        result = collector.collect_pair(
            expected() if exp is None else exp,
            copy.deepcopy(ENDPOINT) if endpoint is None else endpoint,
            backend,
            policy() if pol is None else pol,
        )
        self.assertTrue(result["ok"], result)
        self.assertEqual(result["reason"], "consistent_evidence")
        return result, backend

    def refusal(self, reason, backend=None, exp=None, pol=None, endpoint=None):
        backend = ScriptedBackend() if backend is None else backend
        result = collector.collect_pair(
            expected() if exp is None else exp,
            copy.deepcopy(ENDPOINT) if endpoint is None else endpoint,
            backend,
            policy() if pol is None else pol,
        )
        self.assertFalse(result["ok"], result)
        self.assertEqual(result["reason"], reason, result)
        return result, backend

    # Success and identity ------------------------------------------------
    def test_valid_pair_with_zero_file_components(self):
        result, _ = self.success()
        self.assertEqual(result["payload"]["schema"], "turbomcp.persistence-identity/1")
        self.assertEqual(len(result["payload"]["observations"]), 2)

    def test_uint32_max_file_components(self):
        exp = expected()
        for key in ("project", "engine"):
            exp[key]["file_id"] = file_id(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)
        backend = ScriptedBackend()
        backend.path_values[PROJECT] = [
            path_value(PROJECT, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)
        ] * 4
        backend.path_values[ENGINE] = [
            path_value(ENGINE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)
        ] * 4
        self.success(backend, exp)

    def test_nonzero_monotonic_epoch_and_equal_utc(self):
        backend = ScriptedBackend()
        backend.mono = 9_000_000_000_000_000_000
        result, _ = self.success(backend, pol=policy())
        self.assertEqual(result["payload"]["bounds"]["max_age_seconds"], 10)

    def test_foreign_listener_refused_before_unrelated_process_read(self):
        backend = ScriptedBackend()
        backend.listener_values = [
            {"ok": True, "value": [listener(owner=999)]},
        ]
        self.refusal("listener_foreign", backend)
        self.assertFalse(any(call[0] == "process" and call[1] == 999 for call in backend.calls))

    def test_missing_listener(self):
        backend = ScriptedBackend()
        backend.listener_values = [{"ok": True, "value": []}]
        self.refusal("listener_missing", backend)

    def test_listener_port_mismatch(self):
        backend = ScriptedBackend()
        backend.listener_values = [{"ok": True, "value": [listener(port=8001)]}]
        self.refusal("listener_port_mismatch", backend)

    def test_overflow(self):
        backend = ScriptedBackend()
        backend.listener_values = [
            {"ok": True, "value": [listener() for _ in range(33)]},
        ]
        self.refusal("listener_overflow", backend)

    def test_multiple_rows_same_owner_allowed(self):
        backend = ScriptedBackend()
        rows = [listener(address="127.0.0.1"), listener(address="0.0.0.0")]
        backend.listener_values = [
            {"ok": True, "value": rows},
            {"ok": True, "value": copy.deepcopy(rows)},
            {"ok": True, "value": rows},
            {"ok": True, "value": copy.deepcopy(rows)},
        ]
        self.success(backend)

    # Final rechecks / multiset -------------------------------------------
    def test_reordered_identical_listener_rows_allowed(self):
        backend = ScriptedBackend()
        a = listener(address="127.0.0.1")
        b = listener(address="0.0.0.0")
        backend.listener_values = [
            {"ok": True, "value": [a, b]},
            {"ok": True, "value": [b, a]},
            {"ok": True, "value": [a, b]},
            {"ok": True, "value": [b, a]},
        ]
        self.success(backend)

    def test_duplicate_count_change_refused(self):
        backend = ScriptedBackend()
        a = listener()
        backend.listener_values = [
            {"ok": True, "value": [a, a]},
            {"ok": True, "value": [a]},
        ]
        self.refusal("observation_changed", backend)

    def test_final_process_change_refused(self):
        backend = ScriptedBackend()
        good = {"ok": True, "value": {
            "pid": PID,
            "start_ticks": collector.utc_to_filetime_ticks(START),
            "executable_path": ENGINE,
            "command_line": '"' + ENGINE + '" "' + PROJECT + '"',
        }}
        bad = copy.deepcopy(good)
        bad["value"]["start_ticks"] += 1
        backend.process_values = [good, good, good, bad]
        self.refusal("observation_changed", backend)

    def test_final_path_id_change_refused(self):
        backend = ScriptedBackend()
        backend.path_values[ENGINE] = [
            path_value(ENGINE),
            path_value(ENGINE),
            path_value(ENGINE),
            path_value(ENGINE, volume=1),
        ]
        self.refusal("observation_changed", backend)

    def test_reparse_path_refused(self):
        backend = ScriptedBackend()
        backend.path_values[PROJECT] = [path_value(PROJECT, reparse=True)]
        self.refusal("path_mismatch", backend)

    def test_short_name_path_refused(self):
        backend = ScriptedBackend()
        backend.path_values[ENGINE] = [path_value(ENGINE, short=True)]
        self.refusal("path_mismatch", backend)

    def test_link_count_two_refused(self):
        backend = ScriptedBackend()
        backend.path_values[PROJECT] = [path_value(PROJECT, link=2)]
        self.refusal("path_mismatch", backend)

    def test_link_count_zero_is_malformed(self):
        backend = ScriptedBackend()
        backend.path_values[PROJECT] = [path_value(PROJECT, link=0)]
        self.refusal("backend_malformed", backend)

    # Backend/result validation -------------------------------------------
    def test_backend_timeout_refused(self):
        backend = ScriptedBackend()
        backend.process_values = [{"ok": False, "error": "timeout"}]
        self.refusal("backend_timeout", backend)
        self.assertFalse(any(call[0] in ("parse", "path", "listeners") for call in backend.calls))

    def test_backend_exception_refused(self):
        backend = ScriptedBackend()
        backend.process_exception = RuntimeError("secret")
        self.refusal("backend_unknown", backend)
        self.assertNotIn("secret", str(backend.calls))

    def test_malformed_read_result(self):
        backend = ScriptedBackend()
        backend.process_values = [{"ok": "yes", "value": {}}]
        self.refusal("backend_malformed", backend)

    def test_malformed_discriminants_at_process_boundary(self):
        for bad in (
            {"ok": True, "error": "timeout"},
            {"ok": False, "value": {"pid": PID}},
            {"ok": False, "error": []},
            {"ok": False, "error": {}},
            {"ok": False, "error": None},
            {"ok": False, "error": 1},
            {"ok": False, "error": "not-a-category"},
        ):
            backend = ScriptedBackend()
            backend.process_values = [bad]
            with self.subTest(bad=bad):
                self.refusal("backend_malformed", backend)
                self.assertFalse(any(call[0] in ("parse", "path", "listeners") for call in backend.calls))

    def test_malformed_discriminants_across_other_boundaries(self):
        cases = [
            ("parse", lambda b: setattr(b, "parse_values", [{"ok": True, "error": "timeout"}])),
            ("path", lambda b: b.path_values.__setitem__(PROJECT, [{"ok": True, "error": "timeout"}])),
            ("listeners", lambda b: setattr(b, "listener_values", [{"ok": True, "error": "timeout"}])),
        ]
        for label, mutate in cases:
            backend = ScriptedBackend()
            mutate(backend)
            with self.subTest(boundary=label):
                self.refusal("backend_malformed", backend)

    def test_malformed_utc_value(self):
        backend = ScriptedBackend()
        backend.utc = "not-a-timestamp"
        self.refusal("backend_malformed", backend)

    def test_malformed_monotonic_value(self):
        backend = ScriptedBackend()
        backend.mono_values = [True]
        self.refusal("backend_malformed", backend)

    def test_malformed_process_record(self):
        backend = ScriptedBackend()
        backend.process_values = [{"ok": True, "value": {"pid": PID}}]
        self.refusal("backend_malformed", backend)

    def test_malformed_parse_record(self):
        backend = ScriptedBackend()
        backend.parse_values = [{"ok": True, "value": {"project_path": PROJECT}}]
        self.refusal("backend_malformed", backend)

    def test_malformed_listener_record(self):
        backend = ScriptedBackend()
        backend.listener_values = [{"ok": True, "value": [{"owner_pid": PID}]}]
        self.refusal("backend_malformed", backend)

    def test_listener_exception_is_backend_unknown(self):
        backend = ScriptedBackend()
        backend.listeners_exception = RuntimeError("raw provider text")
        self.refusal("backend_unknown", backend)

    def test_malformed_path_record(self):
        backend = ScriptedBackend()
        backend.path_values[PROJECT] = [{"path": PROJECT}]
        self.refusal("backend_malformed", backend)

    def test_out_of_range_file_id_refused(self):
        exp = expected()
        exp["project"]["file_id"]["volume_serial"] = 0x100000000
        self.refusal("invalid_expected", exp=exp)

    def test_bool_file_id_refused(self):
        backend = ScriptedBackend()
        backend.path_values[ENGINE] = [path_value(ENGINE, volume=True)]
        self.refusal("backend_malformed", backend)

    def test_launch_id_bound_refused(self):
        exp = expected()
        exp["launch_id"] = "x" * 129
        self.refusal("invalid_expected", exp=exp)

    def test_launch_id_control_refused(self):
        exp = expected()
        exp["launch_id"] = "line\nbreak"
        self.refusal("invalid_expected", exp=exp)

    def test_expected_date_out_of_range_refused(self):
        exp = expected()
        exp["editor_start_ticks"] = 2**63 - 1
        self.refusal("invalid_expected", exp=exp)

    def test_path_bound_refused(self):
        exp = expected()
        exp["project"]["path"] = "C:\\" + ("x" * 4090) + ".uproject"
        self.refusal("invalid_expected", exp=exp)

    def test_command_line_bound_refused(self):
        backend = ScriptedBackend()
        backend.process_values = [{"ok": True, "value": {
            "pid": PID,
            "start_ticks": collector.utc_to_filetime_ticks(START),
            "executable_path": ENGINE,
            "command_line": "x" * 32769,
        }}]
        self.refusal("backend_malformed", backend)

    def test_parse_token_count_refused(self):
        backend = ScriptedBackend()
        backend.parse_values = [{"ok": True, "value": {"project_path": PROJECT, "token_count": 2}}]
        self.refusal("process_mismatch", backend)

    def test_pair_policy_less_than_attempt_refused(self):
        pol = policy()
        pol["pair_ns"] = pol["attempt_ns"] - 1
        self.refusal("invalid_policy", pol=pol)

    def test_default_policy_shape(self):
        self.assertEqual(set(collector.default_policy()), {
            "per_call_ns", "attempt_ns", "pair_ns", "snapshot_gap_ns",
            "age_ns", "max_listener_rows",
        })

    def test_unknown_expected_field(self):
        exp = expected()
        exp["extra"] = True
        self.refusal("invalid_expected", exp=exp)

    def test_bool_pid_refused(self):
        exp = expected()
        exp["pid"] = True
        self.refusal("invalid_expected", exp=exp)

    def test_partial_policy_refused(self):
        pol = policy()
        del pol["age_ns"]
        self.refusal("invalid_policy", pol=pol)

    def test_unknown_policy_field_refused(self):
        pol = policy()
        pol["extra"] = 1
        self.refusal("invalid_policy", pol=pol)

    def test_bool_policy_refused(self):
        pol = policy()
        pol["age_ns"] = True
        self.refusal("invalid_policy", pol=pol)

    def test_endpoint_mismatch_refused_without_backend_calls(self):
        backend = ScriptedBackend()
        endpoint = {"host": "127.0.0.1", "port": 8001}
        self.refusal("endpoint_mismatch", backend, endpoint=endpoint)
        self.assertEqual(backend.calls, [])

    # Time boundaries and conversion --------------------------------------
    def test_sub_second_policy(self):
        pol = policy()
        pol.update({
            "per_call_ns": 1_000_000,
            "attempt_ns": 2_000_000,
            "pair_ns": 5_000_000,
            "snapshot_gap_ns": 1_000_000,
            "age_ns": 2_000_000,
        })
        self.success(pol=pol)

    def test_per_call_budget_is_recomputed(self):
        pol = policy()
        pol.update({"per_call_ns": 2, "attempt_ns": 10_000, "pair_ns": 20_000})
        self.success(pol=pol)

    def test_equal_utc_timestamps_allowed(self):
        backend = ScriptedBackend()
        backend.utc_values = [NOW] * 6
        self.success(backend)

    def test_second_attempt_start_before_previous_end_refused(self):
        backend = ScriptedBackend()
        backend.utc_values = [
            "2026-10-18T01:00:00.0000000Z",  # pair start
            "2026-10-18T01:00:01.0000000Z",  # attempt 1 start
            "2026-10-18T01:00:03.0000000Z",  # attempt 1 end
            "2026-10-18T01:00:02.0000000Z",  # attempt 2 start
            "2026-10-18T01:00:04.0000000Z",  # attempt 2 end
            "2026-10-18T01:00:05.0000000Z",  # pair end
        ]
        self.refusal("clock_rollback", backend)
        # Attempt 2 must be refused at its start, before another process read.
        self.assertEqual(sum(1 for call in backend.calls if call[0] == "process"), 2)

    def test_both_snapshot_age_enforced(self):
        backend = ScriptedBackend()
        pol = policy()
        pol["age_ns"] = 1_000_000_000
        backend.utc_values = [
            "2026-10-18T01:00:00.0000000Z",
            "2026-10-18T01:00:00.0000000Z",
            "2026-10-18T01:00:00.1000000Z",
            "2026-10-18T01:00:00.2000000Z",
            "2026-10-18T01:00:00.3000000Z",
            "2026-10-18T01:00:02.0000000Z",
        ]
        self.refusal("stale_observation", backend, pol=pol)

    def test_age_equality_allowed(self):
        backend = ScriptedBackend()
        pol = policy()
        pol["age_ns"] = 1_000_000_000
        backend.utc_values = [
            "2026-10-18T01:00:00.0000000Z",
            "2026-10-18T01:00:00.0000000Z",
            "2026-10-18T01:00:00.1000000Z",
            "2026-10-18T01:00:00.2000000Z",
            "2026-10-18T01:00:00.3000000Z",
            "2026-10-18T01:00:01.1000000Z",
        ]
        self.success(backend, pol=pol)

    def test_payload_bounds_use_policy_not_observed_elapsed(self):
        backend = ScriptedBackend()
        pol = policy()
        pol["age_ns"] = 2_500_000_000
        pol["snapshot_gap_ns"] = 2_500_000_000
        result, _ = self.success(backend, pol=pol)
        self.assertEqual(result["payload"]["bounds"]["max_age_seconds"], 3)
        self.assertEqual(result["payload"]["bounds"]["max_snapshot_gap_seconds"], 3)

    def test_age_one_tick_over_refused(self):
        backend = ScriptedBackend()
        pol = policy()
        pol["age_ns"] = 1_000_000_000
        backend.utc_values = [
            "2026-10-18T01:00:00.0000000Z",
            "2026-10-18T01:00:00.0000000Z",
            "2026-10-18T01:00:00.1000000Z",
            "2026-10-18T01:00:00.2000000Z",
            "2026-10-18T01:00:00.3000000Z",
            "2026-10-18T01:00:01.1000001Z",
        ]
        self.refusal("stale_observation", backend, pol=pol)

    def test_gap_equality_allowed(self):
        backend = ScriptedBackend()
        pol = policy()
        pol["snapshot_gap_ns"] = 200_000_000
        backend.utc_values = [
            "2026-10-18T01:00:00.0000000Z",
            "2026-10-18T01:00:00.0000000Z",
            "2026-10-18T01:00:00.1000000Z",
            "2026-10-18T01:00:00.2000000Z",
            "2026-10-18T01:00:00.3000000Z",
            "2026-10-18T01:00:00.4000000Z",
        ]
        self.success(backend, pol=pol)

    def test_gap_one_tick_over_refused(self):
        backend = ScriptedBackend()
        pol = policy()
        pol["snapshot_gap_ns"] = 200_000_000
        backend.utc_values = [
            "2026-10-18T01:00:00.0000000Z",
            "2026-10-18T01:00:00.0000000Z",
            "2026-10-18T01:00:00.1000000Z",
            "2026-10-18T01:00:00.2000000Z",
            "2026-10-18T01:00:00.3000001Z",
            "2026-10-18T01:00:00.4000000Z",
        ]
        self.refusal("stale_observation", backend, pol=pol)

    def test_deadline_equality_refused(self):
        backend = ScriptedBackend()
        backend.mono_values = [0, 0, 2_000_000_000]
        self.refusal("deadline_exceeded", backend)

    def test_clock_rollback_refused(self):
        backend = ScriptedBackend()
        backend.mono_values = [10, 9]
        self.refusal("clock_rollback", backend)

    def test_utc_rollback_refused(self):
        backend = ScriptedBackend()
        backend.utc_values = [
            "2026-10-18T01:00:01.0000000Z",
            "2026-10-18T01:00:00.0000000Z",
        ]
        self.refusal("clock_rollback", backend)

    def test_filetime_epoch_conversion(self):
        epoch = "1970-01-01T00:00:00.0000000Z"
        exp = expected()
        exp["editor_start_ticks"] = collector.utc_to_filetime_ticks(epoch)
        backend = ScriptedBackend()
        backend.utc = epoch
        good = {"ok": True, "value": {
            "pid": PID,
            "start_ticks": exp["editor_start_ticks"],
            "executable_path": ENGINE,
            "command_line": '"' + ENGINE + '" "' + PROJECT + '"',
        }}
        backend.process_values = [good, good, good, good]
        result, _ = self.success(backend, exp)
        self.assertEqual(result["payload"]["expected"]["start_time_utc"], epoch)

    # Immutability/aliasing -----------------------------------------------
    def test_inputs_not_mutated_and_output_not_aliased(self):
        exp = expected()
        pol = policy()
        before_exp = copy.deepcopy(exp)
        before_pol = copy.deepcopy(pol)
        backend = ScriptedBackend()
        result, _ = self.success(backend, exp, pol)
        self.assertEqual(exp, before_exp)
        self.assertEqual(pol, before_pol)
        payload = result["payload"]
        before_payload = copy.deepcopy(payload)
        backend.last_listener_value[0]["port"] = 9999
        self.assertEqual(payload, before_payload)
        self.assertEqual(exp, before_exp)


if __name__ == "__main__":
    unittest.main(verbosity=2)
