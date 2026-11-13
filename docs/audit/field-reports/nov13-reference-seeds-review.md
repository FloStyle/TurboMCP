# NOV13-05: optional reference seed records

Implemented and focused-tested on stock Unreal 5.8.4. Candidate: 1fb69a9ec422a6f6599577cb581fd73bdceb9e9a.

IncludeSeeds defaults to true. False removes seed rows from presentation only, retaining traversal parent indices and root chains. Visible totals and cursor offsets match the filtered records; seedCount still counts input assets. Changing this option invalidates a continuation cursor.

Scope check passed against d0492d0b805bdaeea737f0d673fe0d04b4c58dde. Supervised run cdb5587da7a64e3792ca731f03fafedb passed build and test-refs-batch. The native regression checks all 700 edges exactly once, mode-change refusal, and an empty edge result with a nonzero seed count. The live wrapper checks default schema, folder selection, pagination, refusals and unchanged dirty packages.

The preceding run a3d8f2f64b22491a84d07ab038a615c2 failed compilation because the new test shadowed a local variable (C4456). That variable was renamed before the successful run. No full-suite or real-project performance claim is made. The new false option is covered natively; the live wrapper exercises the default option. Full-result file export remains NOV13-04.
