"""Offline tests for the pure persistence identity validator.

These tests inject records only. They do not enumerate processes, read the
filesystem, bind/listen on ports, start editors, or call the engine. Every case
asserts an explicit outcome rather than trusting the implementation's own
constants as the only oracle.
"""

from __future__ import annotations

import copy
import unittest

import persistence_identity as identity


PROJECT = r"C:\ExampleDev\TurboMCP\TestProject\TurboMCPTest.uproject"
ENGINE = r"C:\ExampleUnrealengine\Engine\Binaries\Win64\UnrealEditor.exe"
PID = 1234
START = "2026-10-18T01:23:45.1234567Z"
FIRST = "2026-10-18T01:23:46.1234567Z"
SECOND = "2026-10-18T01:23:47.1234567Z"
EVAL = "2026-10-18T01:23:50.1234567Z"


def listener(pid=PID, start=START, project=PROJECT, engine=ENGINE,
             family="ipv4", address="127.0.0.1", port=8000, state="listen"):
    return {
        "family": family,
        "address": address,
        "port": port,
        "state": state,
        "pid": pid,
        "start_time_utc": start,
        "project_path": project,
        "engine_executable": engine,
    }


def process(pid=PID, start=START, project=PROJECT, engine=ENGINE):
    return {
        "pid": pid,
        "start_time_utc": start,
        "project_path": project,
        "engine_executable": engine,
    }


def snapshot(sequence, collected, listeners=None, proc=None, collection_ok=True):
    return {
        "sequence": sequence,
        "collected_at_utc": collected,
        "collection_ok": collection_ok,
        "process": process() if proc is None else proc,
        "listeners": [listener()] if listeners is None else listeners,
    }


def evidence():
    return {
        "schema": identity.SCHEMA,
        "expected": {
            "pid": PID,
            "start_time_utc": START,
            "project_path": PROJECT,
            "engine_executable": ENGINE,
            "endpoint": {"host": "127.0.0.1", "port": 8000},
        },
        "bounds": {
            "evaluated_at_utc": EVAL,
            "max_age_seconds": 10,
            "max_snapshot_gap_seconds": 5,
        },
        "observations": [
            snapshot(1, FIRST),
            snapshot(2, SECOND),
        ],
    }


class PersistenceIdentityTests(unittest.TestCase):
    def pass_case(self, payload=None):
        result = identity.validate_evidence(evidence() if payload is None else payload)
        self.assertTrue(result["ok"], result)
        self.assertEqual(result["reason"], "consistent_evidence")
        self.assertEqual(result["normalized"]["pid"], PID)

    def refuse(self, payload, reason):
        result = identity.validate_evidence(payload)
        self.assertFalse(result["ok"], result)
        self.assertEqual(result["reason"], reason, result)

    # Valid cases ---------------------------------------------------------
    def test_valid_ipv4_exact(self):
        self.pass_case()

    def test_valid_ipv6_exact(self):
        payload = evidence()
        payload["expected"]["endpoint"] = {"host": "::1", "port": 8000}
        row = listener(family="ipv6", address="::1")
        payload["observations"][0]["listeners"] = [row]
        payload["observations"][1]["listeners"] = [copy.deepcopy(row)]
        self.pass_case(payload)

    def test_valid_ipv4_wildcard_same_owner(self):
        payload = evidence()
        row = listener(address="0.0.0.0")
        payload["observations"][0]["listeners"] = [row]
        payload["observations"][1]["listeners"] = [copy.deepcopy(row)]
        self.pass_case(payload)

    def test_valid_ipv6_wildcard_same_owner(self):
        payload = evidence()
        payload["expected"]["endpoint"] = {"host": "::1", "port": 8000}
        row = listener(family="ipv6", address="::")
        payload["observations"][0]["listeners"] = [row]
        payload["observations"][1]["listeners"] = [copy.deepcopy(row)]
        self.pass_case(payload)

    def test_multiple_rows_same_owner_are_one_owner(self):
        payload = evidence()
        rows = [listener(address="127.0.0.1"), listener(address="0.0.0.0")]
        payload["observations"][0]["listeners"] = rows
        payload["observations"][1]["listeners"] = copy.deepcopy(rows)
        self.pass_case(payload)

    def test_explicit_dual_stack_rows_same_owner(self):
        payload = evidence()
        rows = [listener(address="127.0.0.1"), listener(family="ipv6", address="::")]
        payload["observations"][0]["listeners"] = rows
        payload["observations"][1]["listeners"] = copy.deepcopy(rows)
        self.pass_case(payload)

    def test_ipv6_explicit_dual_stack_rows_same_owner(self):
        payload = evidence()
        payload["expected"]["endpoint"] = {"host": "::1", "port": 8000}
        rows = [listener(family="ipv6", address="::1"), listener(address="0.0.0.0")]
        payload["observations"][0]["listeners"] = rows
        payload["observations"][1]["listeners"] = copy.deepcopy(rows)
        self.pass_case(payload)

    # Identity and ownership failures ------------------------------------
    def test_foreign_listener_with_intended_editor_alive(self):
        payload = evidence()
        # The intended process is alive and exactly matches expected; the
        # listening socket belongs to a different process.
        payload["observations"][0]["listeners"] = [listener(pid=9999, start=FIRST)]
        self.refuse(payload, identity.LISTENER_FOREIGN)

    def test_listener_same_pid_different_start_is_foreign(self):
        payload = evidence()
        payload["observations"][0]["listeners"] = [listener(start=FIRST)]
        self.refuse(payload, identity.LISTENER_FOREIGN)

    def test_listener_same_pid_different_project_is_foreign(self):
        payload = evidence()
        payload["observations"][0]["listeners"] = [
            listener(project=r"C:\Other\Other.uproject")
        ]
        self.refuse(payload, identity.LISTENER_FOREIGN)

    def test_pid_reuse_is_not_identity(self):
        payload = evidence()
        payload["observations"][0]["process"] = process(start=FIRST)
        self.refuse(payload, identity.PROCESS_MISMATCH)

    def test_changed_project_is_not_identity(self):
        payload = evidence()
        payload["observations"][0]["process"] = process(project=r"C:\Other\Other.uproject")
        self.refuse(payload, identity.PROCESS_MISMATCH)

    def test_changed_engine_is_not_identity(self):
        payload = evidence()
        payload["observations"][0]["process"] = process(engine=r"C:\Other\UnrealEditor.exe")
        self.refuse(payload, identity.PROCESS_MISMATCH)

    def test_multiple_listener_owners(self):
        payload = evidence()
        rows = [
            listener(address="127.0.0.1"),
            listener(pid=9999, start=FIRST, address="0.0.0.0"),
        ]
        payload["observations"][0]["listeners"] = rows
        payload["observations"][1]["listeners"] = copy.deepcopy(rows)
        self.refuse(payload, identity.LISTENER_MULTIPLE_OWNERS)

    def test_other_family_wildcard_is_ambiguous_without_endpoint_coverage(self):
        payload = evidence()
        row = listener(family="ipv6", address="::")
        payload["observations"][0]["listeners"] = [row]
        payload["observations"][1]["listeners"] = [copy.deepcopy(row)]
        self.refuse(payload, identity.LISTENER_AMBIGUOUS)

    def test_other_family_wildcard_with_foreign_owner_is_ambiguous(self):
        payload = evidence()
        rows = [
            listener(address="127.0.0.1"),
            listener(pid=9999, start=FIRST, family="ipv6", address="::"),
        ]
        payload["observations"][0]["listeners"] = rows
        payload["observations"][1]["listeners"] = copy.deepcopy(rows)
        self.refuse(payload, identity.LISTENER_AMBIGUOUS)

    def test_ipv6_endpoint_needs_ipv6_coverage_not_ipv4_wildcard(self):
        payload = evidence()
        payload["expected"]["endpoint"] = {"host": "::1", "port": 8000}
        row = listener(address="0.0.0.0")
        payload["observations"][0]["listeners"] = [row]
        payload["observations"][1]["listeners"] = [copy.deepcopy(row)]
        self.refuse(payload, identity.LISTENER_AMBIGUOUS)

    def test_missing_listener(self):
        payload = evidence()
        payload["observations"][0]["listeners"] = []
        payload["observations"][1]["listeners"] = []
        self.refuse(payload, identity.LISTENER_MISSING)

    def test_listener_port_mismatch(self):
        payload = evidence()
        row = listener(port=8001)
        payload["observations"][0]["listeners"] = [row]
        self.refuse(payload, identity.LISTENER_PORT_MISMATCH)

    def test_non_listen_state(self):
        payload = evidence()
        row = listener(state="established")
        payload["observations"][0]["listeners"] = [row]
        self.refuse(payload, identity.INVALID_VALUE)

    def test_listener_family_address_mismatch(self):
        payload = evidence()
        row = listener(family="ipv4", address="::1")
        payload["observations"][0]["listeners"] = [row]
        self.refuse(payload, identity.INVALID_VALUE)

    # Collection and timing ----------------------------------------------
    def test_failed_collection(self):
        payload = evidence()
        payload["observations"][0] = snapshot(1, FIRST, proc=None, listeners=[])
        payload["observations"][0]["collection_ok"] = False
        self.refuse(payload, identity.COLLECTION_FAILED)

    def test_unreadable_evidence_without_payload(self):
        self.refuse(None, identity.INVALID_TYPE)

    def test_empty_observations(self):
        payload = evidence()
        payload["observations"] = []
        self.refuse(payload, identity.INVALID_VALUE)

    def test_stale_observation(self):
        payload = evidence()
        payload["bounds"]["evaluated_at_utc"] = "2026-10-18T01:24:10.1234567Z"
        self.refuse(payload, identity.STALE)

    def test_reversed_observations(self):
        payload = evidence()
        payload["observations"][0]["collected_at_utc"] = SECOND
        payload["observations"][1]["collected_at_utc"] = FIRST
        self.refuse(payload, identity.ORDER)

    def test_sequence_out_of_order(self):
        payload = evidence()
        payload["observations"][0]["sequence"] = 2
        payload["observations"][1]["sequence"] = 1
        self.refuse(payload, identity.ORDER)

    def test_snapshot_gap_exceeded(self):
        payload = evidence()
        payload["bounds"]["max_age_seconds"] = 100
        payload["bounds"]["max_snapshot_gap_seconds"] = 0
        payload["observations"][1]["collected_at_utc"] = "2026-10-18T01:23:47.1234567Z"
        self.refuse(payload, identity.STALE)

    def test_creation_time_equal_to_collection_is_allowed(self):
        payload = evidence()
        for observation in payload["observations"]:
            observation["collected_at_utc"] = FIRST
            observation["process"]["start_time_utc"] = FIRST
            observation["listeners"][0]["start_time_utc"] = FIRST
        payload["expected"]["start_time_utc"] = FIRST
        self.pass_case(payload)

    def test_future_creation_time_by_one_tick_is_refused(self):
        future = "2026-10-18T01:23:46.1234568Z"
        payload = evidence()
        payload["expected"]["start_time_utc"] = future
        for observation in payload["observations"]:
            observation["process"]["start_time_utc"] = future
            observation["listeners"][0]["start_time_utc"] = future
        self.refuse(payload, identity.FUTURE_START_TIME)

    def test_future_process_creation_time_is_refused(self):
        future = "2027-01-01T00:00:00.0000000Z"
        payload = evidence()
        payload["expected"]["start_time_utc"] = future
        for observation in payload["observations"]:
            observation["process"]["start_time_utc"] = future
            observation["listeners"][0]["start_time_utc"] = future
        self.refuse(payload, identity.FUTURE_START_TIME)

    def test_future_listener_creation_time_is_independently_refused(self):
        payload = evidence()
        payload["observations"][0]["listeners"][0]["start_time_utc"] = (
            "2026-10-18T01:23:46.1234568Z"
        )
        self.refuse(payload, identity.FUTURE_START_TIME)

    def test_future_observation(self):
        payload = evidence()
        payload["observations"][1]["collected_at_utc"] = "2026-10-18T01:23:51.1234567Z"
        payload["bounds"]["evaluated_at_utc"] = EVAL
        self.refuse(payload, identity.ORDER)

    def test_identity_changed_between_snapshots(self):
        payload = evidence()
        payload["observations"][1]["listeners"] = [listener(address="0.0.0.0")]
        self.refuse(payload, identity.CHANGED)

    def test_listener_ownership_changed_between_snapshots(self):
        payload = evidence()
        payload["observations"][1]["listeners"] = [listener(pid=9999, start=FIRST)]
        self.refuse(payload, identity.LISTENER_FOREIGN)

    # Endpoints -----------------------------------------------------------
    def test_hostname_endpoint_refused(self):
        for host in ("localhost", "example.com", "8.8.8.8", "[::1]"):
            payload = evidence()
            payload["expected"]["endpoint"]["host"] = host
            self.refuse(payload, identity.INVALID_VALUE)

    def test_endpoint_bool_port_refused(self):
        payload = evidence()
        payload["expected"]["endpoint"]["port"] = True
        self.refuse(payload, identity.INVALID_TYPE)

    # Schema and types ----------------------------------------------------
    def test_wrong_schema(self):
        payload = evidence()
        payload["schema"] = "turbomcp.persistence-identity/0"
        self.refuse(payload, identity.INVALID_SCHEMA)

    def test_missing_field(self):
        payload = evidence()
        del payload["expected"]["engine_executable"]
        self.refuse(payload, identity.MISSING_FIELD)

    def test_unknown_top_level_field(self):
        payload = evidence()
        payload["notes"] = "extra"
        self.refuse(payload, identity.UNKNOWN_FIELD)

    def test_unknown_listener_field(self):
        payload = evidence()
        payload["observations"][0]["listeners"][0]["extra"] = True
        self.refuse(payload, identity.UNKNOWN_FIELD)

    def test_wrong_observation_count(self):
        payload = evidence()
        payload["observations"].append(snapshot(1, FIRST))
        self.refuse(payload, identity.INVALID_VALUE)

    def test_boolean_pid_refused(self):
        payload = evidence()
        payload["expected"]["pid"] = True
        self.refuse(payload, identity.INVALID_TYPE)

    def test_boolean_sequence_refused(self):
        payload = evidence()
        payload["observations"][0]["sequence"] = True
        self.refuse(payload, identity.INVALID_TYPE)

    def test_boolean_bounds_refused(self):
        payload = evidence()
        payload["bounds"]["max_age_seconds"] = True
        self.refuse(payload, identity.INVALID_TYPE)

    def test_process_not_object(self):
        payload = evidence()
        payload["observations"][0]["process"] = "pid=1234"
        self.refuse(payload, identity.INVALID_TYPE)

    def test_listeners_not_list(self):
        payload = evidence()
        payload["observations"][0]["listeners"] = {"pid": PID}
        self.refuse(payload, identity.INVALID_TYPE)

    def test_non_string_dict_keys_refused_at_all_levels(self):
        def root(payload):
            payload[1] = "extra"

        def expected(payload):
            payload["expected"][1] = "extra"

        def endpoint(payload):
            payload["expected"]["endpoint"][1] = "extra"

        def bounds(payload):
            payload["bounds"][1] = "extra"

        def observation(payload):
            payload["observations"][0][1] = "extra"

        def process(payload):
            payload["observations"][0]["process"][1] = "extra"

        def listener_row(payload):
            payload["observations"][0]["listeners"][0][1] = "extra"

        cases = {
            "root": root,
            "expected": expected,
            "endpoint": endpoint,
            "bounds": bounds,
            "observation": observation,
            "process": process,
            "listener": listener_row,
        }
        for label, mutate in cases.items():
            payload = evidence()
            mutate(payload)
            with self.subTest(level=label):
                self.refuse(payload, identity.INVALID_TYPE)

    def test_none_dict_key_refused(self):
        payload = evidence()
        payload[None] = "extra"
        self.refuse(payload, identity.INVALID_TYPE)

    def test_relative_path_refused(self):
        payload = evidence()
        payload["expected"]["project_path"] = r"TestProject\TurboMCPTest.uproject"
        self.refuse(payload, identity.INVALID_VALUE)

    def test_unc_path_refused(self):
        payload = evidence()
        payload["expected"]["engine_executable"] = r"\\server\share\UnrealEditor.exe"
        self.refuse(payload, identity.INVALID_VALUE)

    def test_device_path_refused(self):
        payload = evidence()
        payload["expected"]["engine_executable"] = r"\\.\C:\UnrealEditor.exe"
        self.refuse(payload, identity.INVALID_VALUE)

    def test_extended_device_path_refused(self):
        payload = evidence()
        payload["expected"]["engine_executable"] = r"\\?\C:\UnrealEditor.exe"
        self.refuse(payload, identity.INVALID_VALUE)

    def test_dot_segment_refused(self):
        payload = evidence()
        payload["expected"]["project_path"] = r"C:\ExampleDev\..\TurboMCPTest.uproject"
        self.refuse(payload, identity.INVALID_VALUE)

    def test_forward_slash_refused(self):
        payload = evidence()
        payload["expected"]["project_path"] = "C:/ExampleDev/SampleRoot004.uproject"
        self.refuse(payload, identity.INVALID_VALUE)

    def test_wrong_extension_refused(self):
        payload = evidence()
        payload["expected"]["engine_executable"] = r"C:\ExampleUnrealengine\SampleRoot007"
        self.refuse(payload, identity.INVALID_VALUE)

    def test_reserved_device_name_refused(self):
        payload = evidence()
        payload["expected"]["project_path"] = r"C:\ExampleDev\CON.uproject"
        self.refuse(payload, identity.INVALID_VALUE)

    def test_forbidden_win32_path_characters_refused(self):
        for char in '<>"|?*':
            payload = evidence()
            payload["expected"]["project_path"] = (
                r"C:\Bad" + char + r"Name\Test.uproject"
            )
            with self.subTest(char=char):
                self.refuse(payload, identity.INVALID_VALUE)

    def test_control_characters_in_path_refused(self):
        for char in ("\x00", "\x01", "\x1f", "\x7f"):
            payload = evidence()
            payload["expected"]["project_path"] = r"C:\Bad" + char + r"Name\Test.uproject"
            with self.subTest(code=ord(char)):
                self.refuse(payload, identity.INVALID_VALUE)

    def test_forbidden_path_characters_refused_at_process_level(self):
        payload = evidence()
        payload["observations"][0]["process"]["project_path"] = (
            r"C:\Bad<Name\Test.uproject"
        )
        self.refuse(payload, identity.INVALID_VALUE)

    def test_forbidden_path_characters_refused_at_listener_level(self):
        payload = evidence()
        payload["observations"][0]["listeners"][0]["engine_executable"] = (
            r"C:\Bad|Name\UnrealEditor.exe"
        )
        self.refuse(payload, identity.INVALID_VALUE)

    def test_valid_unicode_paths_allowed(self):
        payload = evidence()
        project = "C:\\Donn\u00e9es\\T\u00e9st.uproject"
        engine = "C:\\Donn\u00e9es\\UnrealEditor.exe"
        payload["expected"]["project_path"] = project
        payload["expected"]["engine_executable"] = engine
        for observation in payload["observations"]:
            observation["process"]["project_path"] = project
            observation["process"]["engine_executable"] = engine
            observation["listeners"][0]["project_path"] = project
            observation["listeners"][0]["engine_executable"] = engine
        self.pass_case(payload)


if __name__ == "__main__":
    unittest.main(verbosity=2)
