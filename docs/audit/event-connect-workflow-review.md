# Multi-event connection repair workflow

Status: in progress; no acceptance claim yet.

## Scenario WF-BP-03-EVENT-CONNECT

A disposable Blueprint contains Purchase and Refund custom-event flows in EventGraph, plus an Audit event in another event graph. Each flow uses similar arithmetic and output nodes. Only Purchase has a wrong numeric source connection. Read overview, graph content, search and event context before choosing exact source/target IDs. Repair only that connection; never select by title alone.

## Acceptance

Review source, comments, schema and clear-English output for GraphConnect preview/edit. Preview must identify the intended endpoints and displaced connection without editing the live graph. Apply must replace only the chosen data input source, preserve nodes, positions, defaults and every other connection across all event flows and graphs. Repeating the same connection must report no edit needed. Missing pins, wrong graph IDs and execution/data mismatches must refuse without mutation. Compile explicitly; invoke each event on a disposable object and check independent expected values. Inspect a native capture. No save or workplace assets. Failure restoration and arbitrary node types remain separate coverage.

## Usage tracking

See workflow-tool-coverage.json for executed versus planned tool usage. Reuse of a tool in a new scenario adds evidence, not another unique tool to the coverage count.
