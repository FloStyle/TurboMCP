"use strict";

// Reading and presentation share the same static HTML chapters. No hidden
// slide copy, remote renderer or server-side routing is needed for deep links.
const chapters = [...document.querySelectorAll(".chapter")];
const chapterLinks = [...document.querySelectorAll("#chapter-links [data-chapter]")];
const headings = [...document.querySelectorAll("[data-group-heading]")];
const search = document.querySelector("#chapter-search");
const presentToggle = document.querySelector("#present-toggle");
const controls = document.querySelector(".presentation-controls");
const previous = document.querySelector("#previous-chapter");
const next = document.querySelector("#next-chapter");
const notice = document.querySelector("#guide-notice");
let active = 0;
let presenting = false;
let noticeTimer;
let printState = null;
let navigationScrollY;

function tell(message) {
  notice.textContent = message;
  clearTimeout(noticeTimer);
  noticeTimer = setTimeout(() => { notice.textContent = ""; }, 5000);
}
function indexFromHash() {
  let id;
  try { id = decodeURIComponent(location.hash.slice(1)); } catch { return -1; }
  return chapters.findIndex((chapter) => chapter.id === id);
}
function updateNavigation() {
  chapterLinks.forEach((item, index) => {
    const link = item.querySelector("a");
    if (index === active) link.setAttribute("aria-current", "location");
    else link.removeAttribute("aria-current");
  });
  document.querySelector("#chapter-progress").textContent = `${active + 1} / ${chapters.length} · ${chapters[active].dataset.group}`;
  previous.disabled = active === 0;
  next.disabled = active === chapters.length - 1;
}
function render() {
  const terms = search.value.trim().toLowerCase().split(/\s+/).filter(Boolean);
  let count = 0;
  const matchingGroups = new Set();
  chapters.forEach((chapter, index) => {
    const match = terms.every((term) => `${chapter.textContent} ${chapter.dataset.group}`.toLowerCase().includes(term));
    chapter.hidden = presenting ? index !== active : !match;
    chapterLinks[index].hidden = !match;
    if (match) {
      count++;
      matchingGroups.add(chapter.dataset.group);
    }
  });
  headings.forEach((heading) => { heading.hidden = !matchingGroups.has(heading.dataset.groupHeading); });
  document.querySelector("#no-chapters").hidden = count > 0 || presenting;
  document.querySelector("#search-status").textContent = terms.length ? `${count} matching sections` : "";
  updateNavigation();
}
function writeLocation(push) {
  const url = new URL(location.href);
  url.hash = chapters[active].id;
  if (presenting) url.searchParams.set("view", "present");
  else url.searchParams.delete("view");
  if (url.href !== location.href) history[push ? "pushState" : "replaceState"](null, "", url);
}
function scrollToSelection(target) {
  // Keep the selected chapter through observer updates from this jump.
  target.scrollIntoView({ block: "start", behavior: "instant" });
  navigationScrollY = window.scrollY;
}
function choose(index, { focus = true, scroll = true, push = true } = {}) {
  active = Math.max(0, Math.min(chapters.length - 1, index));
  if (search.value) search.value = "";
  render();
  writeLocation(push);
  if (scroll) scrollToSelection(chapters[active]);
  if (focus) chapters[active].querySelector("h2").focus({ preventScroll: true });
}
function setMode(value, { push = false, focus = true, updateUrl = true } = {}) {
  presenting = value;
  search.value = "";
  document.body.classList.toggle("presenting", value);
  presentToggle.textContent = value ? "Exit presentation" : "Present";
  presentToggle.setAttribute("aria-pressed", String(value));
  controls.hidden = !value;
  render();
  if (updateUrl) writeLocation(push);
  if (focus) {
    scrollToSelection(chapters[active]);
    chapters[active].querySelector("h2").focus({ preventScroll: true });
  }
}
function syncLocation() {
  const index = indexFromHash();
  active = index >= 0 ? index : 0;
  setMode(new URL(location.href).searchParams.get("view") === "present", { focus: false, updateUrl: false });
  scrollToSelection(index >= 0 ? chapters[active] : document.querySelector("#guide-main"));
}

presentToggle.hidden = false;
document.querySelector("#print-guide").hidden = false;
document.querySelector(".chapter-search").hidden = false;
presentToggle.addEventListener("click", () => setMode(!presenting, { push: true }));
previous.addEventListener("click", () => choose(active - 1));
next.addEventListener("click", () => choose(active + 1));
search.addEventListener("input", render);
search.addEventListener("keydown", (event) => {
  if (event.key === "Escape") { search.value = ""; render(); }
});
document.querySelectorAll('a[href^="#"]').forEach((link) => {
  const index = chapters.findIndex((chapter) => `#${chapter.id}` === link.getAttribute("href"));
  if (index < 0) return;
  link.addEventListener("click", (event) => {
    if (event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
    event.preventDefault();
    choose(index);
  });
});
window.addEventListener("hashchange", syncLocation);
window.addEventListener("popstate", syncLocation);
document.addEventListener("keydown", (event) => {
  if (event.altKey || event.ctrlKey || event.metaKey || event.defaultPrevented) return;
  if (event.target.closest("input,textarea,select,[contenteditable=true]")) return;
  if (event.key === "Escape" && presenting) {
    event.preventDefault(); setMode(false, { push: true }); presentToggle.focus(); return;
  }
  if (event.target.closest("button,a,summary")) return;
  if (event.key.toLowerCase() === "p") {
    event.preventDefault(); setMode(!presenting, { push: true });
  } else if (presenting && ["ArrowRight", "PageDown", "ArrowLeft", "PageUp", "Home", "End"].includes(event.key)) {
    event.preventDefault();
    if (event.key === "Home") choose(0);
    else if (event.key === "End") choose(chapters.length - 1);
    else choose(active + (["ArrowRight", "PageDown"].includes(event.key) ? 1 : -1));
  }
});

document.querySelectorAll("[data-request]").forEach((request) => {
  const button = document.createElement("button");
  button.type = "button";
  button.className = "copy copy-prompt";
  button.textContent = "Copy request";
  button.setAttribute("aria-label", `Copy request for ${request.closest(".request").querySelector("figcaption code").textContent}`);
  button.addEventListener("click", async () => {
    try { await navigator.clipboard.writeText(request.textContent); tell("Request copied."); }
    catch { tell("Clipboard unavailable. Select and copy the request text."); }
  });
  request.after(button);
});

// Printed output includes every chapter and expanded detail, even when the
// reader is currently filtering or presenting just one chapter.
window.addEventListener("beforeprint", () => {
  if (printState) return;
  const details = [...document.querySelectorAll(".chapter details")];
  printState = { details, open: details.map((detail) => detail.open) };
  details.forEach((detail) => { detail.open = true; });
  document.body.classList.add("printing");
});
window.addEventListener("afterprint", () => {
  if (!printState) return;
  printState.details.forEach((detail, index) => { detail.open = printState.open[index]; });
  printState = null;
  document.body.classList.remove("printing");
  render();
});
document.querySelector("#print-guide").addEventListener("click", () => window.print());

// Update the sidebar indicator as the reader scrolls. Do not continually
// rewrite history: a shared URL changes only after explicit chapter selection.
if ("IntersectionObserver" in window) {
  const observer = new IntersectionObserver((entries) => {
    if (presenting || printState || search.value || window.scrollY === navigationScrollY) return;
    navigationScrollY = undefined;
    const visible = entries.filter((entry) => entry.isIntersecting);
    if (visible.length) {
      active = chapters.indexOf(visible[0].target);
      updateNavigation();
    }
  }, { rootMargin: "-100px 0px -65% 0px", threshold: 0 });
  chapters.forEach((chapter) => observer.observe(chapter));
}
const initialIndex = indexFromHash();
if (initialIndex >= 0) active = initialIndex;
if (new URL(location.href).searchParams.get("view") === "present") setMode(true, { focus: false });
else render();
if (initialIndex >= 0) scrollToSelection(chapters[active]);
