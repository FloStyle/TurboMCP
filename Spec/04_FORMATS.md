# Shared formats

Formats are shared by many tools. Define each one once in code and use it
everywhere. Most bugs seen in earlier work came from two tools disagreeing
on one of these.

---

## 1. Result layout

Every tool result starts with one status line, then the body.

- Read and Look: what was read, how big it is, and the token estimate.
- Preview: `would change: <summary>` or `nothing to change: <reason>`.
- Edit: `changed: <summary>` or `unchanged: <reason>`. When a measure is
  involved: `<measure> before -> after`.
- Errors: `error: <what failed>`, then the closest valid inputs.

Lists end with `(N more, use <exact call> to see them)` when capped.

---

## 2. Graph text

The text form of a Blueprint graph. The Read tools print it, the write
tools accept it. You design the exact syntax. It must meet every rule
below.

### 2.1 Required properties

1. **Parseable.** S-expression based, because the engine's own graph write
   tool uses S-expressions and agents already know that shape. A real
   parser can read it back with no guessing. Comments start with `;`.
2. **One form per entry point.** Each event, custom event, function and
   override is its own top level form, with its kind written in the head.
   A whole event graph is a list of these forms. There is no top level form
   for "a whole graph".
3. **Exec flow reads in order.** A chain of exec nodes reads top to bottom.
   Where two exec paths join, the join is printed once and the other path
   jumps to it by name.
4. **Every node carries its node ID** (section 3), so the agent can act on
   it. At detail `bodies` the ID can sit in a comment. At detail `full` the
   node type ID (the one the engine's node creation tool accepts) and pin
   indexes are printed too.
5. **Pins by name.** Links and literal values are written as
   `pin name, value`. Use the pin's real name. When the display name and the
   real name differ, print the real name.
6. **Shared values once.** A pure node used by more than one consumer is
   defined once and referred to by name. A pure node used once is written
   inline where it is used, never moved away from its use.
7. **Knots.** Plain reroute knots are invisible at `bodies`: links read as
   direct. At `full`, a link that runs through knots names them, because an
   agent breaking that link must break it at the knot.
8. **Hidden defaults.** Pins left at their default value are not printed.
   The `self` pin and the world context pin are not printed when they are
   implicit. The write tools apply the same rules in reverse.
9. **Own calls look like own calls.** A call to a function of this
   Blueprint prints as a call on self, with the type ID that resolves to
   this Blueprint. A function input reads as an input, not as a variable.
10. **Variables.** Gets and sets name the variable and its owner when the
    owner is not self. Local variables are declared at the top of their
    function.
11. **Disconnected groups** are printed after the entry point forms, in
    their own section, each group once, with a count.
12. **Errors inline.** A node that cannot be printed becomes a comment line
    `; error: <reason>` in its place, never a silent gap.

### 2.2 Round trip

The writer accepts every graph text a reader prints at `full` detail.
Reading a function, writing the text back with Replace, and reading again
gives the same text. This is the main test of the format (T-WRITE-4).

### 2.3 Literals

- Numbers, booleans, strings in the usual way.
- Text values marked as text, not string.
- Enum values by their name, with the enum type when it is not clear.
- Object and class values by full object path.
- Struct values in the engine's own export text form.

### 2.4 Detail levels

See `03_TOOLS.md` R-BP-LOAD. Every level is a strict superset of the one
below it.

### 2.5 Materials

Material graphs use the same S-expression rules:

- Outputs as top level forms, one per connected output property.
- Named reroute labels as `@Label`, defined once, in a labels section.
- Shared values defined once, by node ID.
- A value that has a label is only referred to by its label.
- Every input printed: linked value, or the expression's own constant.
- Parameters listed with defaults and groups at the top.

### 2.6 Engine write dialect (bridge)

R-GRAPH-LOAD format `write` prints a graph in the form the engine's own
graph write tool accepts, because agents will keep using that tool until
the plugin's writer is done. Check the engine's tool for its exact rules.
Rules known so far are in `07_ENGINE_NOTES.md` section 3.3. Whatever it
cannot build is left as a comment line saying what was skipped and which
engine tool can add it.

---

## 3. Node IDs

### 3.1 Form

```
<AssetName>:<GraphName>:<NodeKey>
```

- `AssetName` is the asset's own name, in full. No abbreviation.
- `GraphName` is the graph's name. Quote it if it has spaces.
- `NodeKey` is the first 6 hex digits of the node's GUID, lower case.
  Use more digits only when two nodes in the same graph share the first 6.

Example: `WBP_ShopCard:RefreshPrice:3fa91c`

Long form, always accepted, never ambiguous:

```
/Game/UI/WBP_ShopCard.WBP_ShopCard:RefreshPrice:3fa91c
```

### 3.2 Rules

- An ID never changes while the node exists, whatever is added or removed
  around it.
- Every reader starts its output with the long asset path.
- An `AssetName` that fits more than one asset is refused with every full
  path it fits.
- An ID whose node is gone says so, and lists the nodes in that graph with
  the same title, if any.
- Material expressions use the same form, with the expression's GUID.
  Expressions without a GUID get one (a Preview must not do this; it uses
  a stable fallback and says so).

---

## 4. Asset paths

| Form | Example |
|---|---|
| Short name | `WBP_ShopCard` |
| Package path | `/Game/UI/WBP_ShopCard` |
| Object path | `/Game/UI/WBP_ShopCard.WBP_ShopCard` |
| Generated class path | `/Game/UI/WBP_ShopCard.WBP_ShopCard_C` |
| Widget inside a widget tree | `/Game/UI/WBP_ShopCard.WBP_ShopCard:WidgetTree.PriceText` |

Every tool accepts all of them. Tools print the object path unless another
form is needed for an engine tool, and then they say which form it is.

---

## 5. Token estimate

Outputs are S-expressions, not prose. Most punctuation (brackets, colons,
pipes, dots) costs about a token per character, so "characters divided by
four" undercounts by about half.

Estimate: split the text into runs of letters and digits, and single other
visible characters. Each unbroken sequence of letters and digits counts `ceil(length / 4)`.
Every other visible character counts 1. Whitespace counts 0.

It is an estimate, and every tool description that mentions tokens says so.

### Budget steps

When an output is over its budget:

1. Drop graph bodies to signatures, largest graph first.
2. Drop the relation layer details to counts.
3. Drop variables and widget details to names only.
4. Stop, and add a closing block that lists each reduced item and the exact
   call that returns it in full.

Never cut text in the middle of a form.

---

## 6. Captures

- PNG files under `<Project>/Saved/<plugin folder>/Captures/`, named
  `<Asset>_<Tab>_<Framing>_<time>.png` unless the agent gives a path.
- The result gives the path, the image size, and what was framed.

## 7. Logs

- Call log and trace log: one JSON object per line, under
  `<Project>/Saved/<plugin folder>/Logs/`, one file per day.
