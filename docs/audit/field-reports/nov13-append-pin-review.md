# NOV05-14 quoted pin review

Candidate: 5662c31144d99bcb72943e406e5d5048e0a97a9a.
Approved base: 3ca02187959f5ee2986c5c8cb0bac5ba2ee5339c.
Assignment nov13-append-pins; exact three-file scope passed.

## Implementation and evidence

Names containing spaces were already supported. The lexer now scans escaped quotes and decodes names with the same escape rules as values, rather than ending the name at the first escaped quote. Malformed closing/name boundaries refuse. Append help includes quoted destination and source examples.

Stock UE 5.8.4 compilation passed. Automation TurboMCP.GraphAppend.AliasCollision passed in execution 4e2afc104f5f4af987243921dadf384f. The synthetic fixture appends two real setters: one stores 7 on Purchase Count and one links that spaced input to the exact quoted getter output. It asserts preview immutability, exactly two created nodes, reciprocal endpoint identity and unchanged unrelated graph. Lexer cases cover spaces, escaped quotes, semicolons and malformed names. No engine changes.

The earlier execution cc151d397f67422cb043cec02e81cefd failed: the first fixture incorrectly expected existing-only append forms to edit defaults. Append correctly returned unchanged. The corrected fixture creates nodes; the initial failure is retained, not erased.

The full append wrapper is not certified by this focused success. Its first run also failed a legacy pin-count message assertion and GraphAppend.Inventory's empty-event NodeFind assertion. Final execution 4e2afc104f5f4af987243921dadf384f failed only these two checks (wrapper exit 1); AliasCollision passed. Follow-up: reconcile the pin-count assertion with the current output contract, and diagnose the empty-event finder expectation before changing production behavior. Full runner time was 204.634 seconds. The supervised editor was stopped and fixture restoration completed.

## Limits

The original field call is unavailable. This resolves the supported quoted-name workflow and a demonstrated lexer defect; it does not claim reproduction of that exact historical call. Additional Return creation remains NOV05-13. No full regression or general append certification.
