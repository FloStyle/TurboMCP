# FT-166 integration review

Reviewed candidate: e5b475b75af61790c704067b724b757ea9adecfd.

Accepted for private integration: batch registry traversal, per-call adjacency caching, explicit kind and disk evidence, cursor pagination and legacy reader compatibility. No save, checkout, map load or PIE is invoked by the readers. Focused supervised build and live/native test report 768be01ee0984554b2df248b5463775d passed on the candidate. Tests include cycle termination, 700 edges across pages, kind/identity preservation, folder/class filtering, invalid input and unchanged dirty list. No broad rerun during integration.

700 synthetic seeds: depth 1 1.432 ms (700 adjacency queries); depth 2 1.918 ms (701 queries). This is an isolated hard-edge registry fixture, not real-project disk or MCP transport performance. Do not advertise these as field timings.

Limits: pagination recomputes traversal and rejects changed topology; total includes seed records. Up to 5000 seeds, depth 6 and 250000 records; partial/traversalCapped and exactness fields report limits. Follow nextCursor until empty even if complete is true (complete describes traversal). Class filter is exact. Registry roots mean saved packages with no incoming selected-kind edges, not semantic GameMode/PlayerController/config roots. Semantic/config root discovery remains deferred. Property paging and P1-P5 are not delivered by this patch.

Repository include checker reports existing local-header index failures; the actual pinned 5.8.3 compile/link passed. Public release acceptance remains separate. The focused test is registered in the catalog; test-refs currently invokes the batch wrapper too, so run either the combined suite or standalone batch, not both unnecessarily.
