[CmdletBinding()]
param(
    [string]$Ledger = 'C:\Users\Example\.dsh\task-board\ledger-v2.json',
    [string]$OutDir = '',
    [switch]$Overwrite
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrEmpty($OutDir)) {
    if ($PSScriptRoot) { $OutDir = $PSScriptRoot }
    else { $OutDir = 'C:\Users\Example\Desktop\DSH_Workspaces\TurboMCP_UnrealEngine\_task_export' }
}
$utf8 = [System.Text.UTF8Encoding]::new($false)

$raw = Get-Content -Raw -LiteralPath $Ledger
$root = $raw | ConvertFrom-Json -Depth 100
$revision = $root.revision
$exportedAt = [DateTimeOffset]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')

function Esc([string]$s) {
    if ($null -eq $s) { return '' }
    return $s.Replace('\', '\\').Replace('"', '\"').Replace("`r", '').Replace("`n", ' ')
}

function Iso([object]$ms) {
    $v = [long]$ms
    if ($v -le 0) { return '' }
    return [DateTimeOffset]::FromUnixTimeMilliseconds($v).UtcDateTime.ToString('yyyy-MM-ddTHH:mm:ssZ')
}

function Slug([string]$title) {
    $s = $title.ToLowerInvariant()
    $s = $s -replace '[^a-z0-9]+', '-'
    $s = $s.Trim('-')
    if ($s.Length -gt 80) { $s = $s.Substring(0, 80).Trim('-') }
    if ([string]::IsNullOrEmpty($s)) { $s = 'task' }
    return $s
}

function Cell([string]$v) {
    if ([string]::IsNullOrEmpty($v)) { return '' }
    return (($v -replace '\|', '\|') -replace "\r?\n", ' ').Trim()
}

function TagNames($task) {
    $names = @()
    if ($task.tags) { foreach ($tag in @($task.tags)) { if ($tag -and $tag.name) { $names += [string]$tag.name } } }
    return $names
}

function PriorityOf($tags) {
    foreach ($name in $tags) { if ($name -match '^P[0-9]$') { return $name } }
    return ''
}

$selected = @($root.tasks | Where-Object {
        (-not $_.archivedAt -or [long]$_.archivedAt -eq 0) -and
        $_.status -in @('backlog', 'todo', 'running')
    })

if ($selected.Count -eq 0) { throw 'No tasks matched the export filter.' }

# Fresh-folder guard: never wipe a directory this script did not create.
if (Test-Path -LiteralPath $OutDir) {
    $existing = @(Get-ChildItem -LiteralPath $OutDir -Force)
    if ($existing.Count -gt 0 -and -not (Test-Path -LiteralPath (Join-Path $OutDir '.dsh-task-export'))) {
        throw "Output directory exists and is not a previous export: $OutDir"
    }
    foreach ($f in @(Get-ChildItem -LiteralPath $OutDir -File -Filter '*.md')) { Remove-Item -LiteralPath $f.FullName -Force }
}
else {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

$indexRows = New-Object System.Collections.Generic.List[object]
$written = New-Object System.Collections.Generic.List[string]

foreach ($t in ($selected | Sort-Object { PriorityOf (TagNames $_) }, { $_.title })) {
    $tags = TagNames $t
    $priority = PriorityOf $tags
    $status = [string]$t.status
    $id8 = ([string]$t.id).Substring(0, 8)
    $file = "$id8-$(Slug ([string]$t.title)).md"
    $execs = @($t.executions | Where-Object { $_ })

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add('---')
    $lines.Add('id: "' + (Esc $t.id) + '"')
    $lines.Add('title: "' + (Esc $t.title) + '"')
    $lines.Add('status: ' + $status)
    if ($status -ne [string]$t.status) { $lines.Add('source_status: "' + (Esc $t.status) + '"') }
    $lines.Add('priority: "' + $priority + '"')
    if ($tags.Count -gt 0) {
        $quoted = ($tags | ForEach-Object { '"' + (Esc $_) + '"' }) -join ', '
        $lines.Add('tags: [' + $quoted + ']')
    }
    else { $lines.Add('tags: []') }
    $lines.Add('workspace: "' + (Esc $t.workspaceId) + '"')
    if ($t.parentId) { $lines.Add('parent: "' + (Esc $t.parentId) + '"') }
    if ($t.mode) { $lines.Add('mode: "' + (Esc $t.mode) + '"') }
    if ($t.permission) { $lines.Add('permission: "' + (Esc $t.permission) + '"') }
    if ($t.skipVerification) { $lines.Add('skip_verification: true') }
    $lines.Add('created: "' + (Iso $t.createdAt) + '"')
    $lines.Add('updated: "' + (Iso $t.updatedAt) + '"')
    $lines.Add('executions: ' + $execs.Count)
    $lines.Add('board_revision: ' + $revision)
    $lines.Add('exported: "' + $exportedAt + '"')
    $lines.Add('---')
    $lines.Add('')
    $lines.Add('# ' + $t.title)
    $lines.Add('')
    $lines.Add('| Field | Value |')
    $lines.Add('|---|---|')
    $lines.Add('| Card ID | `' + $t.id + '` |')
    $lines.Add('| Status | ' + $status + ' |')
    $lines.Add('| Priority | ' + $(if ($priority) { $priority } else { '—' }) + ' |')
    $lines.Add('| Tags | ' + $(if ($tags.Count) { ($tags | ForEach-Object { '`' + $_ + '`' }) -join ' ' } else { '—' }) + ' |')
    $lines.Add('| Workspace | `' + $t.workspaceId + '` |')
    if ($t.parentId) { $lines.Add('| Parent card | `' + $t.parentId + '` |') }
    if ($t.mode) { $lines.Add('| Agent preset | `' + $t.mode + '` |') }
    if ($t.permission) { $lines.Add('| Permission | `' + $t.permission + '` |') }
    $lines.Add('| Created (UTC) | ' + (Iso $t.createdAt) + ' |')
    $lines.Add('| Updated (UTC) | ' + (Iso $t.updatedAt) + ' |')
    $lines.Add('| Executions recorded | ' + $execs.Count + ' |')
    $lines.Add('')
    $lines.Add('## Description')
    $lines.Add('')
    $desc = [string]$t.description
    if ([string]::IsNullOrWhiteSpace($desc)) { $lines.Add('_No description on the card._') }
    else { $lines.Add($desc.TrimEnd()) }
    $lines.Add('')

    $prompt = [string]$t.prompt
    if (-not [string]::IsNullOrWhiteSpace($prompt) -and $prompt.Trim() -ne $desc.Trim()) {
        $lines.Add('## Execution prompt')
        $lines.Add('')
        $lines.Add('```text')
        $lines.Add($prompt.TrimEnd())
        $lines.Add('```')
        $lines.Add('')
    }

    $lines.Add('## Execution history')
    $lines.Add('')
    if ($execs.Count -eq 0) {
        $lines.Add('_No execution record. The card state is a board declaration, not run evidence._')
        $lines.Add('')
    }
    else {
        $n = 0
        foreach ($e in $execs) {
            $n++
            $lines.Add('### Run ' + $n + ' — ' + [string]$e.result)
            $lines.Add('')
            $lines.Add('- Started (UTC): ' + (Iso $e.startedAt))
            $lines.Add('- Ended (UTC): ' + (Iso $e.endedAt))
            if ($e.initiatedBy) { $lines.Add('- Initiated by: `' + $e.initiatedBy + '`') }
            if ($e.sessionId) { $lines.Add('- Session: `' + $e.sessionId + '`') }
            if ($e.cascade) { $lines.Add('- Cascade run: true (own result: ' + [string]$e.ownResult + ')') }
            if ($e.error) {
                $lines.Add('')
                $lines.Add('> ' + ([string]$e.error).Replace("`r`n", "`n").Replace("`n", "`n> "))
            }
            $lines.Add('')
        }
    }

    $lines.Add('---')
    $lines.Add('')
    $lines.Add('_Exported from the DSH task board, revision ' + $revision + ', at ' + $exportedAt + '. The board is authoritative; this file is a snapshot._')

    [System.IO.File]::WriteAllText((Join-Path $OutDir $file), (($lines -join "`n") + "`n"), $utf8)
    $written.Add($file)
    $indexRows.Add([pscustomobject]@{
            Id         = [string]$t.id
            Title      = [string]$t.title
            Status     = $status
            Priority   = $priority
            Tags       = ($tags -join ' ')
            Executions = $execs.Count
            File       = $file
            Updated    = Iso $t.updatedAt
        })
}

# INDEX.md
$idx = New-Object System.Collections.Generic.List[string]
$idx.Add('# TurboMCP task board export')
$idx.Add('')
$idx.Add('Snapshot of the DSH task board taken at ' + $exportedAt + ' (board revision ' + $revision + ').')
$idx.Add('Scope: backlog, todo and running cards. One Markdown file per card.')
$idx.Add('')
$idx.Add('The board is authoritative. This folder is a readable copy; edit the board, then re-export.')
$idx.Add('')
$idx.Add('## Counts')
$idx.Add('')
$idx.Add('| Status | Cards |')
$idx.Add('|---|---:|')
foreach ($g in ($indexRows | Group-Object Status | Sort-Object Name)) {
    $idx.Add('| ' + $g.Name + ' | ' + $g.Count + ' |')
}
$idx.Add('| **Total** | **' + $indexRows.Count + '** |')
$idx.Add('')
$idx.Add('| Priority | Cards |')
$idx.Add('|---|---:|')
foreach ($g in ($indexRows | Group-Object { if ($_.Priority) { $_.Priority } else { 'none' } } | Sort-Object Name)) {
    $idx.Add('| ' + $g.Name + ' | ' + $g.Count + ' |')
}
$idx.Add('')

foreach ($statusGroup in ($indexRows | Group-Object Status | Sort-Object Name)) {
    $idx.Add('## ' + $statusGroup.Name + ' (' + $statusGroup.Count + ')')
    $idx.Add('')
    $idx.Add('| Priority | Task | Tags | Runs |')
    $idx.Add('|---|---|---|---:|')
    $rows = $statusGroup.Group | Sort-Object { if ($_.Priority) { $_.Priority } else { 'P9' } }, Title
    foreach ($r in $rows) {
        $label = $r.Title.Replace('|', '\|')
        $idx.Add('| ' + $(if ($r.Priority) { $r.Priority } else { '—' }) + ' | [' + $label + '](<' + $r.File + '>) | ' + (Cell $r.Tags) + ' | ' + $r.Executions + ' |')
    }
    $idx.Add('')
}

[System.IO.File]::WriteAllText((Join-Path $OutDir 'INDEX.md'), (($idx -join "`n") + "`n"), $utf8)

# README.md
$readme = @()
$readme += '# Tasks'
$readme += ''
$readme += 'Written copy of the DSH task board for this project, one Markdown file per card.'
$readme += ''
$readme += '- Scope: `backlog`, `todo` and `running` cards (running cards are recorded as `todo`).'
$readme += '- Source of truth: the DSH task board, not this folder.'
$readme += '- `INDEX.md` lists every card by status and priority.'
$readme += '- `snapshot.json` holds the same cards as machine-readable JSON.'
$readme += '- `export_task_board.ps1` regenerates this folder from the board ledger.'
$readme += ''
$readme += 'Snapshot: board revision ' + $revision + ', exported ' + $exportedAt + '.'
$readme += ''
$readme += 'Re-export replaces `*.md` and `snapshot.json` in this folder. Do not store hand-written notes here;'
$readme += 'put them on the card instead, so the next export keeps them.'
[System.IO.File]::WriteAllText((Join-Path $OutDir 'README.md'), (($readme -join "`n") + "`n"), $utf8)

$snapshotPath = Join-Path $OutDir 'snapshot.json'
$snapshot = [ordered]@{
    exported     = $exportedAt
    boardRevision = $revision
    filter       = @('backlog', 'todo', 'running')
    count        = $selected.Count
    tasks        = @($selected)
}
[System.IO.File]::WriteAllText($snapshotPath, ($snapshot | ConvertTo-Json -Depth 100), $utf8)

[System.IO.File]::WriteAllText((Join-Path $OutDir '.dsh-task-export'), "dsh-task-board-export`n", $utf8)

"exported $($selected.Count) cards to $OutDir"
"files: $($written.Count) task files + INDEX.md + README.md + snapshot.json"
