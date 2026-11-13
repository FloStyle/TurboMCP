# NOV09-16 DataTable reference review

Candidate 7246e96a1dd548e30aca05c84e025e932108bd22; approved base c393b0f74dd02a9145d554111d4111818c4074ec. Exact three-file scope passed.

The reader inspects active literal input pins on loaded Blueprint graphs, including loaded unsaved assets outside saved registry edges. Direct objects and exported exact paths are supported. Linked defaults and orphan pins are excluded. Known GetDataTableRow pins are labeled row-read sites; other literals are references whose behavior is not inferred. Registry dependencies remain candidates, and unloaded/non-Blueprint candidates are explicitly unsearched. Searched counts no longer count only matched graphs. No additional referencer loading, writes, compile or save is performed by the read tool.

Stock UE 5.8.4 execution d7ba886ad81349c38e72440dfcae8104 passed compilation and test-datatable-quality. One suite, 0.579 seconds process time; runner 25.315 seconds. Fixture covers loaded unsaved custom literal, known row-read node, different-table exclusion and unchanged dirty state, plus existing DataTable checks. No full regression.

Limits: exported string paths, split pins, linked defaults and large evidence truncation were source-reviewed, not separately fixture-tested. Evidence caps at 40 sites per candidate and 40 candidates, with explicit omission notices. Dynamic references, row-struct dataflow and actual runtime custom-function behavior are not inferred. DT-18 does not prove that a shared row struct identifies a particular table.
