"""Static regression guard for nested user-widget traversal.

The runtime acceptance fixture is a host UserWidget containing another UserWidget,
whose tree has ordinary panel children and named-slot content.  These assertions
protect both readers' traversal contracts without requiring an editor server.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "Tools"
COMPACT = (ROOT / "Plugins" / "TurboMCP" / "Source/TurboMCP/TurboMCPWidgetTree.cpp").read_text(encoding="utf-8")
DIALECT = (ROOT / "Plugins" / "TurboMCP" / "Source/TurboMCP/TurboMCPWidgetTreeSync.cpp").read_text(encoding="utf-8")
FIXTURE = (TOOLS / "nested-child-widget-tree.fixture").read_text(encoding="utf-8")
failures = []


def check(label, condition):
    print("  %s %s" % ("ok  " if condition else "FAIL", label))
    if not condition:
        failures.append(label)


def main():
    check("fixture describes a host with a nested child user widget",
          "NestedChild [UserWidget]" in FIXTURE and "ChildRoot [CanvasPanel]" in FIXTURE)
    check("fixture includes both a child widget and named-slot content",
          "ChildLabel [TextBlock]" in FIXTURE
          and "ChildSlot [NamedSlot]" in FIXTURE
          and "SlottedLabel [TextBlock] named-slot=ChildSlot" in FIXTURE)
    check("fixture preserves descendant indentation",
          "  NestedChild [UserWidget]\n    ChildRoot [CanvasPanel]\n      ChildLabel [TextBlock]" in FIXTURE)
    check("compact reader descends into child UUserWidget::WidgetTree",
          "UserWidget->WidgetTree->RootWidget" in COMPACT)
    check("compact reader follows INamedSlotInterface content",
          "NamedSlots->GetContentForSlot(SlotName)" in COMPACT)
    check("compact reader records the named-slot host",
          "Entry.NamedSlotHost = Pending.NamedSlotHost" in COMPACT)
    check("compact reader preserves a UNamedSlot parent for its content",
          "const FName PanelNamedSlot = Cast<UNamedSlot>(Widget)" in COMPACT)
    check("dialect reader descends into child UUserWidget::WidgetTree", 
          "UserWidget->WidgetTree->RootWidget" in DIALECT)
    check("dialect reader follows INamedSlotInterface content",
          "NamedSlots->GetContentForSlot(SlotName)" in DIALECT)
    check("both traversals guard nested cycles and duplicate slot reachability",
          "TSet<const UWidget*> Visited" in COMPACT and "TSet<const UWidget*> Visited" in DIALECT)
    print("RESULT: %s" % ("PASS" if not failures else "FAIL"))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
