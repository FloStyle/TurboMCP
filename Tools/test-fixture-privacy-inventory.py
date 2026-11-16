"""Read the exact public-preview fixture set in a supervised TestProject editor."""

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys

from execution_runtime import canonical, require_context
from tmcp import READ_TOOLSET, Session, StructuredAnswer, URL, call_structured
from tmcp_paths import find_engine


ROOT = Path(__file__).resolve().parent.parent
CONTENT = ROOT / "TestProject/Content"
PROJECT = ROOT / "TestProject/TurboMCPTest.uproject"
MANIFEST = ROOT / "Tests/fixture-privacy-inventory.json"
SOURCE = "0df188fc56156e8b1bdcdb2d48885f4ff60cc573"
PACKAGE_PATH = re.compile(r"TestProject/Content/Tests/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+\.(?:uasset|umap)")
HEX40 = re.compile(r"[0-9a-f]{40}")
HEX64 = re.compile(r"[0-9a-f]{64}")
CLASSES = {"Blueprint", "WidgetBlueprint", "Material", "UserDefinedEnum", "World"}
READERS = {name: "ReadTurboMCP" + name for name in (
    "AssetDescribe", "AssetCost", "DirtyList", "BlueprintLoad", "GraphLoad",
    "WidgetTree", "MaterialLoad", "MaterialTrace",
)}
MAX_CALLS = 200
MAX_REPLY_BYTES = 1_000_000
PAGE_CHARS = 24000
MAX_PAGES = 8


class InventoryError(Exception):
    """The message is a controlled reason code, safe for the local summary."""


def require(condition, code):
    if not condition:
        raise InventoryError(code)


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def git(*args, cwd=ROOT):
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True,
                          timeout=30).stdout


def load_manifest():
    data = read_json(MANIFEST)
    rows = data["packages"]
    require(data.get("schema") == 1 and data.get("source_commit") == SOURCE and len(rows) == 15,
            "manifest_identity_invalid")
    result = {}
    for row in rows:
        path = row["path"]
        require(PACKAGE_PATH.fullmatch(path) and path.casefold() not in result
                and HEX40.fullmatch(row["git_blob"]) and HEX64.fullmatch(row["sha256"])
                and row["class"] in CLASSES and (path.endswith(".umap") == (row["class"] == "World")),
                "manifest_row_invalid")
        result[path.casefold()] = row
    return {row["path"]: row for row in result.values()}


def unlinked(path):
    """Reject all reparse points, including Windows directory junctions."""
    item = path.lstat()
    require(not stat.S_ISLNK(item.st_mode)
            and not (getattr(item, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT),
            "linked_path")
    return item


def require_components(path, boundary):
    for i in range(1, len(path.relative_to(boundary).parts) + 1):
        unlinked(boundary.joinpath(*path.relative_to(boundary).parts[:i]))


def snapshot(manifest):
    require_components(CONTENT, ROOT)
    files = set()

    def walk_error(_):
        raise InventoryError("fixture_directory_unreadable")

    for parent, dirs, names in os.walk(CONTENT, followlinks=False, onerror=walk_error):
        for name in dirs + names:
            path = Path(parent) / name
            st = unlinked(path)
            if name.lower().endswith((".uasset", ".umap")):
                require(stat.S_ISREG(st.st_mode) and st.st_nlink == 1, "fixture_not_private_regular_file")
                files.add(path.relative_to(ROOT).as_posix())
    require(files == set(manifest), "disk_package_set_mismatch")
    hashes = {}
    for name in sorted(files):
        path = ROOT / name
        require_components(path, ROOT)
        with path.open("rb") as handle:
            hashes[name] = hashlib.file_digest(handle, "sha256").hexdigest()
    return hashes


def verify_tree(manifest):
    require(canonical(git("rev-parse", "--show-toplevel").decode("utf8").strip()) == canonical(ROOT),
            "foreign_git_worktree")
    tracked = {}
    for line in git("ls-tree", "-rz", "HEAD", "--", "TestProject/Content").split(b"\0"):
        if not line:
            continue
        meta, path = line.decode("utf8").split("\t", 1)
        if path.lower().endswith((".uasset", ".umap")):
            mode, kind, oid = meta.split()
            require(mode == "100644" and kind == "blob", "git_package_not_regular")
            tracked[path] = oid
    # A fresh public history need not contain the source commit. The file identities must still match.
    require(tracked == {name: row["git_blob"] for name, row in manifest.items()}, "git_package_set_mismatch")


def evidence_directory():
    context = Path(os.environ["TURBOMCP_EXECUTION_CONTEXT"])
    executions = ROOT / ".local/executions"
    run = context.parent
    out = Path(os.environ["TURBOMCP_RUNNER_LOG_ROOT"])
    require(context.name == "context.json" and run.parent == executions
            and re.fullmatch(r"[0-9a-f]{32}", run.name) and out.parent == run
            and os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1"
            and re.fullmatch(r"[0-9]{2}-suites", out.name), "foreign_evidence_directory")
    require_components(context, ROOT)
    # The runner may create its suite log directory only after the first child.
    # Its parent was checked above; create only this exact owned child, not parents.
    out.mkdir(exist_ok=True)
    require_components(out, ROOT)
    require(out.is_dir(), "evidence_not_directory")
    return out


def owned_editor(engine, source, engine_commit):
    lease = read_json(ROOT / ".agent/turbo-mcp-lease.json")
    expected_owner = os.environ.get("TURBOMCP_OWNER_ID")
    require(URL == "http://localhost:8000/mcp" and bool(expected_owner)
            and lease["OwnerId"] == expected_owner and lease["McpPort"] == 8000
            and type(lease["EditorPid"]) is int and lease["EditorPid"] > 0
            and canonical(lease["RepoRoot"]) == canonical(ROOT)
            and canonical(lease["Project"]) == canonical(PROJECT)
            and canonical(lease["Engine"]) == canonical(engine)
            and canonical(lease["FixtureDir"]) == canonical(CONTENT / "Tests"),
            "foreign_editor_lease")
    build = read_json(ROOT / ".agent/build-provenance.json")
    module = build["ArtifactSha256"].lower()
    require(build["BuildExitCode"] == 0 and build["SourceCommit"] == source
            and build["EngineIdentity"] == engine_commit and HEX64.fullmatch(module)
            and canonical(build["RepoRoot"]) == canonical(ROOT)
            and canonical(build["Project"]) == canonical(PROJECT), "foreign_build_provenance")
    # The existing health probe verifies the loaded DLL against this build and checks the lease PID.
    health = subprocess.run([sys.executable, str(ROOT / "Tools/check-mcp-server.py"),
                             "--project", str(PROJECT), "--editor-pid", str(lease["EditorPid"])],
                            cwd=ROOT, capture_output=True, timeout=45)
    require(health.returncode == 0, "owned_editor_health_failed")
    return lease["EditorPid"], module


def session_schemas(session):
    session.connect()
    wire = session.rpc("tools/call", {
        "name": "describe_toolset", "arguments": {"toolset_name": READ_TOOLSET},
    })
    require(not wire.get("isError") and isinstance(wire.get("content"), list), "reader_schema_refused")
    answer = StructuredAnswer([block["text"] for block in wire["content"]], None)
    tools = json.loads(answer.unwrapped_text())["tools"]
    schemas = {}
    for tool in tools:
        name = tool["name"]
        short = name.rsplit(".", 1)[-1]
        if short in READERS.values():
            require(name in {short, READ_TOOLSET + "." + short} and short not in schemas,
                    "reader_schema_ambiguous")
            schemas[short] = tool["inputSchema"]
    return schemas


def schema_arguments(schema, values):
    properties = schema["properties"]
    matches = {key.casefold(): key for key in properties}
    require(len(matches) == len(properties), "reader_schema_ambiguous")
    normalized = {}
    for proposed, value in values.items():
        field = matches.get(proposed.casefold())
        expected = {str: ("string",), bool: ("boolean",), int: ("integer", "number")}[type(value)]
        require(field is not None and properties[field].get("type") in expected, "reader_schema_mismatch")
        normalized[field] = value
    require(set(schema.get("required", ())) <= set(normalized), "reader_required_argument_missing")
    return normalized


class Reader:
    def __init__(self, session, schemas, raw):
        self.session, self.schemas, self.raw = session, schemas, raw
        self.count = 0
        self.limit = MAX_CALLS

    def read(self, scope, name, **values):
        tool = READERS[name]
        require(tool in self.schemas, "reader_unavailable")
        args = schema_arguments(self.schemas[tool], values)
        require(self.count < self.limit, "reader_call_limit")
        self.count += 1
        answer = call_structured(self.session, READ_TOOLSET, tool, args, requires_result=False)
        body = answer.body.strip()
        # Never emit a server response to the terminal, even when it refuses.
        raw_bytes = sum(len(block.encode("utf8", "surrogatepass")) for block in answer.blocks)
        require(raw_bytes <= MAX_REPLY_BYTES, "reader_reply_limit")
        self.raw.write(json.dumps({"scope": scope, "reader": tool, "arguments": args, "blocks": answer.blocks,
                                   "transport_error": answer.is_error}) + "\n")
        self.raw.flush()
        if answer.is_error or body.startswith(("error:", "!!", "(no content)")):
            raise InventoryError("reader_refused")
        require(body and not body.startswith("partial:")
                and not (answer.result and answer.suggest_exit_code() != 0), "reader_incomplete")
        return body


def asset_identity(reader, locator, package, expected_class):
    result = reader.read(locator, "AssetDescribe", assets=package)
    object_path = package + "." + package.rsplit("/", 1)[-1]
    require(result.startswith("read: describing 1 asset(s), all 1 described\n")
            and re.findall(r"(?m)^package: (.+)$", result) == [package]
            and re.findall(r"(?m)^  object: (.+)$", result) == [object_path]
            and re.findall(r"(?m)^  class: (.+)$", result) == [expected_class]
            and re.search(r"(?m)^  on disk: yes\b", result), "asset_identity_unproved")
    state = re.findall(r"(?m)^  unsaved changes: (.*)$", result)
    return object_path, {"yes": "dirty", "no": "clean"}.get(state[0], "unknown") if len(state) == 1 else "unknown"


def dirty_content(reader):
    body = reader.read("TestProject/Content", "DirtyList", limit="1000")
    match = re.match(r"read: unsaved content, (\d+) content package\(s\) dirty(?:, listing (\d+))?(?:\n|$)", body)
    require(match is not None, "dirty_list_unproved")
    count = int(match[1])
    rows = re.findall(r"(?m)^(/\S+)  \[[^\r\n]+\]$", body)
    require(count == int(match[2] or 0) == len(rows) == len(set(rows)), "dirty_list_incomplete")
    return set(rows)


def blueprint_graphs(reader, scope, target):
    body = reader.read(scope, "BlueprintLoad", assets=target, detail="manifest", graphFilter="*",
                       format="json", tokenBudget="60000")
    header, delimiter, payload = body.partition("\n\n===== " + target + " =====\n")
    require(header.startswith("read: 1 of 1 asset(s), requested detail manifest, ") and delimiter,
            "blueprint_manifest_incomplete")
    data = json.loads(payload)
    require(data["ok"] is True and data["asset"] == target and data["detail"] == "manifest"
            and data["scope"] == "whole asset", "blueprint_identity_unproved")
    names = [graph["name"] for graph in data["graphs"]]
    require(data["counts"]["graphs"] == len(names) <= 32
            and all(isinstance(name, str) and 0 < len(name) <= 256 for name in names)
            and len({name.casefold() for name in names}) == len(names), "graph_manifest_ambiguous")
    return names


def graph_report(reader, scope, target, graph, ordinal):
    offset, revision, total, note_count = 0, "first", None, 0
    for page in range(MAX_PAGES):
        data = json.loads(reader.read(scope, "GraphLoad", asset=target, graphName=graph, format="json",
                                     detail="full", offset=offset, maxChars=PAGE_CHARS,
                                     revision=revision, entryFilter="*"))
        token, count, size, following = (data[key] for key in ("revision", "count", "total", "nextOffset"))
        require(data["schema"] == 1 and data["asset"] == target and data["graph"] == graph
                and data["detail"] == "full" and data["offset"] == offset
                and isinstance(token, str) and HEX40.fullmatch(token.lower())
                and type(count) is int and type(size) is int and 0 <= count <= PAGE_CHARS
                and offset + count <= size
                and len(data["body"].encode("utf-16-le", "surrogatepass")) // 2 == count,
                "graph_page_invalid")
        require(page == 0 or (revision == token and total == size), "graph_revision_changed")
        require(isinstance(data["body_notes"], list), "graph_body_notes_invalid")
        note_count += len(data["body_notes"])
        if following == -1:
            require(offset + count == size, "graph_page_incomplete")
            return {"ordinal": ordinal, "pages": page + 1, "body_units": size,
                    "reader_note_count": note_count, "represented_content_only": True}
        require(type(following) is int and following == offset + count and offset < following < size,
                "graph_page_incomplete")
        offset, revision, total = following, token, size
    raise InventoryError("graph_page_limit")


def inspect_package(reader, row):
    scope, target, kind = row["path"], row["object"], row["class"]
    cost = reader.read(scope, "AssetCost", asset=target, cap=100)
    package = row["package"]
    require(cost.startswith("read: asset package sizes and direct registry dependencies\n")
            and re.findall(r"(?m)^asset: (.+)$", cost) in ([package], [target]),
            "dependency_identity_unproved")
    summary = re.search(r"(?m)^DIRECT DEPENDENCIES BY KIND \((\d+) dependency entr\(ies\) examined\)$", cost)
    require(summary is not None, "dependency_summary_missing")
    truncated = re.search(r"(?m)^  \.\.\. \d+ unknown-size entries omitted by Cap", cost)
    contributors = re.search(r"(?m)^LARGEST CONTRIBUTORS \((\d+) of (\d+), capped at \d+\)$", cost)
    require(contributors is not None, "dependency_listing_unproved")
    limited = bool(truncated) or contributors[1] != contributors[2]
    row["readers"]["AssetCost"] = {"status": "partial" if limited else "collected",
                                   "registry_entries_examined": int(summary[1])}
    if limited:
        row["gaps"].append("dependency_listing_capped")
    row["gaps"].append("registry_does_not_cover_all_references_or_imports")
    if kind in {"Blueprint", "WidgetBlueprint"}:
        graphs = blueprint_graphs(reader, scope, target)
        row["readers"]["BlueprintLoad"] = {"status": "collected", "reported_graphs": len(graphs)}
        row["readers"]["GraphLoad"] = {"status": "collecting", "graphs": []}
        for ordinal, graph in enumerate(graphs, 1):
            row["readers"]["GraphLoad"]["graphs"].append(graph_report(reader, scope, target, graph, ordinal))
        graph_scope = row["readers"]["GraphLoad"]
        graph_scope["status"] = "partial" if any(x["reader_note_count"] for x in graph_scope["graphs"]) else "collected"
        row["gaps"].append("blueprint_nested_objects_and_unsupported_graph_fields_need_review")
    if kind == "WidgetBlueprint":
        widget = reader.read(scope, "WidgetTree", asset=target)
        # WidgetTree emits the designer form, unlike the counted BlueprintLoad text section.
        require(widget.startswith("read: (widget-tree\n") and "\n; Coverage: hierarchy," in widget,
                "widget_tree_unproved")
        row["readers"]["WidgetTree"] = {"status": "collected", "designer_tree_form": True}
        row["gaps"].append("widget_text_bindings_animations_and_other_properties_not_exhaustive")
    if kind == "Material":
        load = reader.read(scope, "MaterialLoad", asset=target, bIncludeInstanceOverrides=False, tokenBudget=3000)
        require(load.splitlines()[0] in {"read: " + package, "read: " + target}, "material_identity_unproved")
        count = re.search(r"(?m)^connected outputs: (\d+) of \d+$", load)
        outputs = re.findall(r"(?m)^\((MP_[A-Za-z0-9_]+)\b", load)
        require(count is not None and 0 < int(count[1]) == len(outputs) == len(set(outputs)) <= 32,
                "material_outputs_unproved")
        row["readers"]["MaterialLoad"] = {"status": "collected", "connected_outputs": len(outputs)}
        row["readers"]["MaterialTrace"] = {"status": "collecting", "traced_outputs": 0}
        for output in outputs:
            trace = reader.read(scope, "MaterialTrace", asset=target, outputProperty=output, tokenBudget=3000)
            require(trace.splitlines()[0] in {"read: " + package + ", output " + output,
                                             "read: " + target + ", output " + output}, "material_trace_unproved")
            row["readers"]["MaterialTrace"]["traced_outputs"] += 1
        row["readers"]["MaterialTrace"]["status"] = "collected"
        row["gaps"].append("unused_material_expressions_and_import_metadata_need_review")
    if kind == "World":
        row["gaps"].append("world_actors_components_exports_and_map_dirty_coverage_unknown")
    if kind == "UserDefinedEnum":
        row["gaps"].append("serialized_enum_values_and_localized_labels_not_read")


def issue(error):
    # JSON decoder, subprocess and client errors can include private payloads or local paths.
    return str(error) if isinstance(error, InventoryError) else "reader_or_environment_error"


def checkpoint(stream, report):
    # Preserve the last record while the suite waits on a bounded native request.
    stream.seek(0)
    json.dump(report, stream, indent=2)
    stream.write("\n")
    stream.truncate()
    stream.flush()


def main():
    require(len(sys.argv) == 1, "arguments_not_supported")
    # A missing context refuses before engine lookup or MCP access.
    require(os.environ.get("TURBOMCP_EXECUTION_CONTEXT"), "supervisor_context_required")
    engine = find_engine()
    require_context(ROOT, engine)
    out = evidence_directory()
    manifest = load_manifest()
    verify_tree(manifest)
    source = git("rev-parse", "HEAD").decode("ascii").strip()
    engine_commit = git("rev-parse", "HEAD", cwd=engine).decode("ascii").strip()
    require(engine_commit == read_json(ROOT / "engine.lock.json")["commit"], "engine_revision_mismatch")
    before = snapshot(manifest)
    report = {"schema": 1, "source_commit": source, "fixture_source_commit": SOURCE, "engine_commit": engine_commit,
              "plugin_tree": git("rev-parse", "HEAD:Plugins/TurboMCP").decode("ascii").strip(),
              "status": "incomplete", "semantic_clearance": False, "packages": [], "issues": []}
    for path, item in manifest.items():
        package = "/Game/" + PurePosixPath(path).with_suffix("").relative_to("TestProject/Content").as_posix()
        report["packages"].append({**item, "package": package, "disk_before": before[path], "disk_after": None,
                                   "dirty_before": "unknown", "dirty_after": "unknown",
                                   "status": "not_run", "readers": {}, "gaps": [], "issues": []})
    with (out / "fixture-privacy-inventory.json").open("x", encoding="utf8") as summary:
        checkpoint(summary, report)
        reader, dirty_before = None, None
        try:
            require(before == {path: row["sha256"] for path, row in manifest.items()}, "fixture_pre_hash_mismatch")
            pid, report["loaded_module_sha256"] = owned_editor(engine, source, engine_commit)
            session = Session()
            with (out / "fixture-privacy-inventory-raw.jsonl").open("x", encoding="utf8") as raw:
                reader = Reader(session, session_schemas(session), raw)
                try:
                    dirty_before = dirty_content(reader)
                    report["dirty_content_before_count"] = len(dirty_before)
                    for row in report["packages"]:
                        try:
                            row["object"], row["dirty_before"] = asset_identity(
                                reader, row["path"], row["package"], row["class"])
                            row["readers"]["AssetDescribe"] = {"status": "collected"}
                            require(row["dirty_before"] != "dirty" and row["package"] not in dirty_before,
                                    "fixture_dirty_before_read")
                        except Exception as error:
                            row["status"] = "incomplete"
                            row["issues"].append(issue(error))
                            raise
                    # Reserve the per-package after checks and global DirtyList from the call cap.
                    reader.limit = MAX_CALLS - len(manifest) - 1
                    for row in report["packages"]:
                        row["status"] = "collecting"
                        checkpoint(summary, report)
                        try:
                            inspect_package(reader, row)
                            row["status"] = "collected_with_known_gaps"
                        except Exception as error:
                            row["status"] = "incomplete"
                            row["issues"].append(issue(error))
                finally:
                    reader.limit = MAX_CALLS
                    if dirty_before is not None:
                        try:
                            dirty_after = dirty_content(reader)
                            report["dirty_content_after_count"] = len(dirty_after)
                            require(dirty_after == dirty_before, "dirty_content_changed")
                        except Exception as error:
                            report["issues"].append(issue(error))
                    for row in report["packages"]:
                        if "object" not in row:
                            continue
                        try:
                            _, row["dirty_after"] = asset_identity(reader, row["path"], row["package"], row["class"])
                            require(row["dirty_after"] != "dirty"
                                    and not (row["dirty_before"] == "clean" and row["dirty_after"] == "unknown"),
                                    "fixture_dirty_state_changed_or_lost")
                            if "unknown" in (row["dirty_before"], row["dirty_after"]):
                                row["gaps"].append("package_dirty_state_unknown_when_unloaded")
                        except Exception as error:
                            row["status"] = "incomplete"
                            row["issues"].append(issue(error))
            require(owned_editor(engine, source, engine_commit) == (pid, report["loaded_module_sha256"]),
                    "owned_editor_identity_changed")
        except Exception as error:
            report["issues"].append(issue(error))
        finally:
            try:
                after = snapshot(manifest)
                for row in report["packages"]:
                    row["disk_after"] = after[row["path"]]
                    if row["disk_before"] != row["disk_after"]:
                        row["status"] = "incomplete"
                        row["issues"].append("fixture_disk_changed")
                require(before == after, "fixture_disk_changed")
            except (InventoryError, OSError) as error:
                report["issues"].append(issue(error))
            collected = sum(row["status"] == "collected_with_known_gaps" for row in report["packages"])
            passed = collected == len(manifest) and not report["issues"]
            report["status"] = "collected_with_known_gaps" if passed else "incomplete"
            report["tool_calls"] = reader.count if reader else 0
            checkpoint(summary, report)
    print(f"RESULT: {'PASS' if passed else 'FAIL'}; {collected}/{len(manifest)} package inventories collected; semantic review pending")
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print("RESULT: REFUSED; " + issue(error))
        sys.exit(2)
