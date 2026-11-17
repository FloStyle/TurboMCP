"use strict";

// The page uses local data and assets only. Catalogue text is rendered as text,
// never HTML. Source declarations describe contracts, not live acceptance.
const $ = (selector) => document.querySelector(selector);
const element = (tag, text, className) => {
  const node = document.createElement(tag);
  if (text !== undefined) node.textContent = text;
  if (className) node.className = className;
  return node;
};
let notification;
async function copyText(text) {
  try {
    await navigator.clipboard.writeText(text);
    $("#copy-status").textContent = "Copied.";
  } catch {
    $("#copy-status").textContent = "Clipboard unavailable. Select the visible text and copy it.";
  }
  clearTimeout(notification);
  notification = setTimeout(() => { $("#copy-status").textContent = ""; }, 5000);
}
function copyButton(label, text, accessibleLabel) {
  const button = element("button", label, "copy");
  button.type = "button";
  button.setAttribute("aria-label", accessibleLabel || label);
  button.addEventListener("click", () => copyText(text));
  return button;
}

const captures = {
  overview: {
    title: "Graph capture: 69-node fixture",
    description: "Mode=graph captures the selected graph. This recorded overview contains 69 nodes; labels need a closer node or neighbour capture.",
    caption: "Whole-graph view: 69 nodes in the synthetic VisionEvents_0 graph. Open for full size.",
    mode: "graph", size: "1600x900", target: "Graph selected by GraphName", width: 1600, height: 900
  },
  focus: {
    title: "Node capture: 1200 × 800",
    description: "Pass the exact node ID returned by NodeFind as Target, with Mode=node and Size=1200x800. The image shows the captured region of the synthetic graph.",
    caption: "Exact-node view: make the selected area readable. Open the image for full size.",
    mode: "node", size: "1200x800", target: "Exact node ID from NodeFind", width: 1200, height: 800
  },
  neighbours: {
    title: "Neighbour capture: one hop, six nodes",
    description: "See the target and its immediate connections. In neighbours mode, Size is a hop count, not an image resolution. This capture contains six nodes.",
    caption: "Connected-node view: one hop from the selected Sequence node. Open for full size.",
    mode: "neighbours", size: "1 (hop)", target: "Exact Sequence node ID", width: 1038, height: 1058
  }
};
document.querySelectorAll("[data-capture]").forEach((button) => {
  button.addEventListener("click", () => {
    const key = button.dataset.capture;
    const capture = captures[key];
    const source = `assets/graph-${key}.png`;
    $("#capture-image").src = source;
    $("#capture-image").alt = capture.caption;
    $("#capture-image").width = capture.width;
    $("#capture-image").height = capture.height;
    $("#capture-link").href = source;
    $("#capture-title").textContent = capture.title;
    $("#capture-description").textContent = capture.description;
    $("#capture-caption").textContent = capture.caption;
    $("#capture-settings").replaceChildren();
    for (const [label, value] of [["Mode", capture.mode], ["Size", capture.size], ["Target", capture.target]]) {
      $("#capture-settings").append(element("dt", label), element("dd", value));
    }
    document.querySelectorAll("[data-capture]").forEach((tab) => {
      tab.setAttribute("aria-pressed", String(tab === button));
    });
  });
});

const recipes = [
  {
    title: "Understand a complex Blueprint",
    benefit: "Expected: graph names, exact node IDs, nearby execution links and input sources.",
    tools: ["ReadTurboMCPBlueprintLoad", "ReadTurboMCPNodeFind", "ReadTurboMCPNodeContext"],
    prompt: "Discover the live TurboMCP schema. Read /Game/Examples/BP_Demo without editing it. Find the calls involved in its input flow, follow their node context, and explain the result with exact asset, graph and node IDs. Report skipped or unreadable content."
  },
  {
    title: "Audit references and widgets",
    benefit: "Expected: saved reference edges and matching widget instances, with continuation and coverage limits.",
    tools: ["ReadTurboMCPRefsBatch", "ReadTurboMCPWidgetInventory"],
    prompt: "Discover the live TurboMCP schema. Audit referencers of /Game/Examples/Input and widget uses of those input assets. Read every result page, preserve reference kinds and instance values, and report skipped assets and coverage limits. Do not edit, save or start PIE."
  },
  {
    title: "Inspect data before a batch edit",
    benefit: "Expected: current typed values and a validated list of proposed row changes.",
    tools: ["ReadTurboMCPDataAssetProperty", "PreviewTurboMCPDataTableRows"],
    prompt: "Discover the live TurboMCP schema. Read the requested properties of /Game/Examples/DA_Demo. For /Game/Examples/DT_Demo, inspect the row schema and preview only my requested row changes. Show the affected rows and current values before any edit. Do not apply or save changes yet."
  },
  {
    title: "Make graph layout readable",
    benefit: "Expected: layout findings and a proposed arrangement for the selected graph.",
    tools: ["LookTurboMCPLayoutReport", "PreviewTurboMCPGraphTidy", "LookTurboMCPCapture"],
    prompt: "Discover the live TurboMCP schema. Inspect layout in the EventGraph of /Game/Examples/BP_Demo. Capture the current graph, then preview a tidy operation limited to the selected graph. Explain the proposed changes and any warnings. Wait before applying changes."
  },
  {
    title: "Check menu input in local PIE",
    benefit: "Expected: selected-player focus and contexts, followed by a bounded input delivery report.",
    tools: ["ReadTurboMCPPlayInputState", "EditTurboMCPPlayInput"],
    prompt: "Discover the live TurboMCP schema. Inspect focus and input state in my running local PIE session. Confirm the exact target and test key with me before sending any input. Then send only that bounded input and read the result. Distinguish input delivery from proof that the intended game action happened."
  },
  {
    title: "Understand a failed call",
    benefit: "Expected: the failed request, relevant diagnostics and supported parameter values.",
    tools: ["ReadTurboMCPToolHelp", "ReadTurboMCPCallLog", "ReadTurboMCPEditorLog"],
    prompt: "Inspect the failed TurboMCP call, current tool help and relevant editor log. Explain the specific error and the smallest correction. If an edit may already have run, inspect its outcome before retrying. Do not repeat a possibly completed mutation blindly."
  }
];
let catalogue = null;
function showTool(name) {
  $("#tool-search").value = name;
  $("#tool-group").value = "all";
  renderTools();
  $("#catalogue").scrollIntoView();
  const row = $(".tool-row");
  if (row) {
    row.open = true;
    row.querySelector("summary").focus({ preventScroll: true });
  } else {
    $("#tool-search").focus({ preventScroll: true });
  }
}
recipes.forEach((recipe, index) => {
  const article = element("article");
  article.append(element("span", `0${index + 1}`, "recipe-number"), element("h3", recipe.title), element("p", recipe.benefit));
  const links = element("div", undefined, "tool-links");
  recipe.tools.forEach((name) => {
    const button = element("button", name);
    button.type = "button";
    button.setAttribute("aria-label", `Show ${name} in the catalogue`);
    button.addEventListener("click", () => showTool(name));
    links.append(button);
  });
  const details = element("details");
  details.append(element("summary", "Example prompt"), element("p", recipe.prompt), copyButton("Copy prompt", recipe.prompt, `Copy prompt: ${recipe.title}`));
  article.append(links, details);
  $("#recipes").append(article);
});

function toolRow(tool) {
  const row = element("details", undefined, "tool-row");
  const summary = element("summary");
  const badge = element("span", tool.group, "badge");
  badge.dataset.group = tool.group;
  summary.append(badge, element("span", tool.name, "tool-name"), element("span", tool.summary, "tool-summary"));
  const detail = element("div", undefined, "tool-detail");
  detail.append(element("p", tool.description), element("h4", "Parameters from the source declaration"));
  const params = element("dl", undefined, "param-list");
  tool.parameters.forEach((param) => {
    const wrapper = element("div", undefined, "param");
    const term = element("dt", param.name);
    term.append(element("span", `${param.type} · ${param.required ? "No C++ default" : `C++ default: ${param.default}`}`));
    wrapper.append(term, element("dd", param.description));
    params.append(wrapper);
  });
  detail.append(params);
  if (!tool.parameters.length) detail.append(element("p", "No parameters in this declaration."));
  detail.append(element("h4", "Use it with an agent"));
  const usage = `Discover the live schema and read current tool help for ${tool.toolset}.${tool.name}. Return its required inputs, supported operations and side effects for my task.`;
  detail.append(element("p", usage));
  const actions = element("div", undefined, "tool-actions");
  const source = element("a", "Source and contract ↗");
  source.href = `https://github.com/FloStyle/TurboMCP/blob/main/${catalogue.source}#L${tool.sourceLine}`;
  actions.append(copyButton("Copy tool name", tool.name, `Copy tool name ${tool.name}`), copyButton("Copy help prompt", usage, `Copy help prompt for ${tool.name}`), source);
  detail.append(actions);
  row.append(summary, detail);
  return row;
}
function renderTools() {
  if (!catalogue) return;
  const terms = $("#tool-search").value.trim().toLowerCase().split(/\s+/).filter(Boolean);
  const group = $("#tool-group").value;
  const exact = catalogue.tools.find((tool) => tool.name.toLowerCase() === terms.join(" "));
  const matches = catalogue.tools.filter((tool) => {
    const haystack = `${tool.name} ${tool.description} ${tool.parameters.map((p) => `${p.name} ${p.description}`).join(" ")}`.toLowerCase();
    return (group === "all" || tool.group === group) && (exact ? tool === exact : terms.every((term) => haystack.includes(term)));
  });
  $("#tool-list").replaceChildren(...matches.map(toolRow));
  $("#tool-list").scrollTop = 0;
  if (!matches.length) $("#tool-list").append(element("p", "No matching tools. Try another term or reset the filters.", "empty-result"));
  $("#tool-count").textContent = `${matches.length} of ${catalogue.count} source declarations · Read ${catalogue.counts.Read} / Look ${catalogue.counts.Look} / Preview ${catalogue.counts.Preview} / Edit ${catalogue.counts.Edit}`;
}
$("#tool-search").addEventListener("input", renderTools);
$("#tool-group").addEventListener("change", renderTools);
$("#tool-filters").addEventListener("submit", (event) => event.preventDefault());
$("#tool-filters").addEventListener("reset", () => {
  // Form controls reset after the reset event dispatch.
  setTimeout(renderTools, 0);
});
fetch("tools.json").then((response) => {
  if (!response.ok) throw new Error("Catalogue unavailable");
  return response.json();
}).then((data) => {
  if (data.schema !== 1 || !Array.isArray(data.tools) || data.tools.length !== data.count) throw new Error("Invalid catalogue");
  catalogue = data;
  const linkedTool = new URL(location.href).searchParams.get("tool");
  if (linkedTool && catalogue.tools.some((tool) => tool.name === linkedTool)) {
    showTool(linkedTool);
    return;
  }
  renderTools();
}).catch(() => {
  $("#tool-count").textContent = "The catalogue could not load. Open the JSON download or source header below.";
});
