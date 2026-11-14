# NOV05-17: widget reference property repair contract

Source review at private main `7799f8c3`: `TurboMCPWidgetTreeTools.cpp` accepts editable scalar and scalar-struct values only. `IsWidgetValueType` excludes object and class references, so the reader omits them and Preview/Edit refuse them. The public tool descriptions explicitly say that dotted paths and object values are unsupported. The field report WT-18 used the stock editor object-property tool to set an entry widget class and a brush texture. This is a capability gap, not evidence that the current writer corrupted data.

Implement one bounded extension of Read/Preview/EditTurboMCPWidgetProperty:

- Accept an exact top-level property name or a dotted path through named struct fields. Reject empty segments, array/map/set traversal, ambiguous names and traversal through object references. Keep the current Navigation special case.
- List readable, editable object/class reference paths alongside the existing value fields. Never list ownership, widget-tree, slot, delegate, instanced, transient or editor-only object fields as writable. The reader must distinguish an unset reference from an unsupported field.
- Accept `None` to clear a supported reference and an exact Unreal object or class path to set it. Validate the target property's expected class, including a class property's meta-class, before any write. Refuse maps, redirectors, missing assets and incompatible types. No automatic class search, compile, checkout or save. Reference loading can run native load hooks; disclose that in preview and edit output.
- Preview must show the resolved property path, type, before/after value and whether a referenced asset would be loaded. Edit must revalidate against the current object, use a transaction and editor notifications, then read back the value. Preserve all other fields of a containing struct. A repeated value is unchanged.
- Keep unsupported reference types and container elements as explicit refusals. Do not infer that this tool edits a child widget's class or generated Blueprint member variable; those are separate operations.

Focused acceptance: a synthetic Widget Blueprint sets, reads back and clears a brush resource object and a list-entry widget class; a nested brush edit preserves unrelated brush fields. Preview does not change the package or value. Incompatible class, missing path, ownership field, container field and stale target refuse without mutation. Review help and plain-English output with the code. Run the focused native case and a stock-engine build only when the batch is ready; no broad suite is implied.

This contract does not close NOV05-17. The field project's exact asset values are unavailable, so the synthetic fixture proves the supported behavior but does not certify that project.
