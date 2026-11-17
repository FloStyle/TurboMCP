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
    title: "Find the relevant block",
    description: "Frame the whole graph. The four comment groups help you locate the Request_0_0 block.",
    caption: "The whole graph shows four authored groups. Individual pin labels are too small at this scale.",
    alt: "Whole-graph capture with four vertically stacked comment groups.",
    limit: "Use the overview to locate a block, then choose a closer capture to read its pins.",
    mode: "graph", size: "1600x900", target: "", width: 1600, height: 900
  },
  focus: {
    title: "Follow Then 0",
    description: "Find the Sequence’s exact node ID with NodeFind. Pass that ID to Capture in node mode to read the pin labels.",
    caption: "The white wire from Then 0 reaches the bind node. The red wire identifies Destroyed_0_0 as its handler.",
    alt: "A Sequence node connects Then 0 to Bind Event to On Destroyed and Then 1 to a Branch.",
    limit: "The crop shows nearby pins. Some downstream nodes extend beyond the frame.",
    mode: "node", size: "1200x800", target: "<sequence-node-id>", width: 1200, height: 800
  },
  neighbours: {
    title: "See what Sequence reaches",
    description: "Use neighbours mode with Size set to 1. The tool selects the immediate neighbours and chooses the image dimensions.",
    caption: "Request_0_0 feeds Sequence. Then 0 reaches the bind node, and Then 1 through Then 3 lead toward Branch nodes.",
    alt: "One-hop capture showing the Sequence input and its bind and branch destinations.",
    limit: "One hop shows the immediate graph connections. This image does not establish which branches ran.",
    mode: "neighbours", size: "1", target: "<sequence-node-id>", width: 1038, height: 1058
  }
};
const captureButtons = [...document.querySelectorAll("[data-capture]")];
const captureRequest = $("#capture-request code");
function showCapture(key) {
  const capture = captures[key];
  const source = `assets/graph-${key}.png`;
  const image = $("#capture-image");
  image.src = source;
  image.alt = capture.alt;
  image.width = capture.width;
  image.height = capture.height;
  $("#capture-link").href = source;
  $("#capture-title").textContent = capture.title;
  $("#capture-description").textContent = capture.description;
  $("#capture-caption").textContent = capture.caption;
  $("#capture-limit").textContent = capture.limit;
  captureRequest.textContent = JSON.stringify({
    toolset_name: "TurboMCP.TurboMCPLookToolset",
    tool_name: "LookTurboMCPCapture",
    arguments: {
      Asset: "<blueprint-object-path>",
      GraphName: "VisionEvents_0",
      Mode: capture.mode,
      Target: capture.target,
      Size: capture.size
    }
  }, null, 2);
  for (const button of captureButtons) {
    button.setAttribute("aria-pressed", String(button.dataset.capture === key));
  }
}
$(".capture-tabs").hidden = false;
$("#copy-capture").hidden = false;
for (const button of captureButtons) {
  button.addEventListener("click", () => showCapture(button.dataset.capture));
}
$("#copy-capture").addEventListener("click", () => copyText(captureRequest.textContent));
for (const button of document.querySelectorAll("[data-copy-target]")) {
  button.hidden = false;
  button.addEventListener("click", () => copyText($(button.dataset.copyTarget).textContent));
}
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
  const usage = JSON.stringify({
    toolset_name: "TurboMCP.TurboMCPReadToolset",
    tool_name: "ReadTurboMCPToolSchema",
    arguments: { Toolset: tool.toolset, Tool: tool.name }
  }, null, 2);
  const actions = element("div", undefined, "tool-actions");
  const source = element("a", "Source and contract ↗");
  source.href = `https://github.com/FloStyle/TurboMCP/blob/main/${catalogue.source}#L${tool.sourceLine}`;
  actions.append(copyButton("Copy tool name", tool.name, `Copy tool name ${tool.name}`), copyButton("Copy schema request", usage, `Copy schema request for ${tool.name}`), source);
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
