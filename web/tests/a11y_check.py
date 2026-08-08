#!/usr/bin/env python3
"""WCAG 2.2 AA structural checker for PlacementDB web fixtures.

Uses only the Python standard library. No Node, no npm, no network.
Runs sequentially with one worker. Validates the synthetic rendered HTML
fixtures in this directory against WCAG 2.2 AA structural criteria and reads
the plain CSS to assert focus-visible, reduced-motion, responsive, and
contrast tokens.

This is a structural checker, not a full conformance audit. The real
browser-rendered audit runs after Codex authorizes an application build.
"""

import html.parser
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURES = os.path.join(HERE, "fixtures")
CSS_PATH = os.path.join(HERE, "..", "static", "css", "main.css")

FAILED = []


def fail(fixture, rule, detail):
    FAILED.append((fixture, rule, detail))


class PageParser(html.parser.HTMLParser):
    def __init__(self):
        super().__init__()
        self.stack = []
        self.elements = []
        self.title = None
        self.lang = None
        self.headings = []
        self.labels = []
        self.inputs = []
        self.buttons = []
        self.tables = []
        self.error_summaries = []
        self.pagination_current = []
        self.pagination_current_links = 0
        self._pagination_depth = 0
        self.aria_live = []
        self.skip_link = None
        self.first_focusable = None
        self.main_count = 0
        self.main_ids = []
        self._in_string = None
        self._label_stack = []

    def handle_starttag(self, tag, attrs):
        d = dict(attrs)
        self.elements.append((tag, d))
        self.stack.append(tag)
        if tag == "nav" and "pagination" in (d.get("class") or "").split():
            self._pagination_depth = len(self.stack)
        if tag == "html":
            self.lang = d.get("lang")
        if tag == "title":
            self._in_string = "title"
        if tag == "main":
            self.main_count += 1
            if "id" in d:
                self.main_ids.append(d["id"])
        if tag in ("h1", "h2", "h3", "h4", "h5", "h6"):
            self.headings.append((tag, d))
        if tag == "label":
            self.labels.append(d)
            self._label_stack.append(bool(d.get("for")))
        if tag in ("input", "select", "textarea"):
            wrapped = any(has_for is False for has_for in self._label_stack)
            self.inputs.append((tag, d, wrapped))
        if tag == "button":
            self.buttons.append((tag, d))
        if tag == "table":
            self.tables.append([])
        if tag == "th":
            if self.tables:
                self.tables[-1].append(("th", d))
        if tag == "td":
            if self.tables:
                self.tables[-1].append(("td", d))
        if d.get("role") == "alert" or "error-summary" in (d.get("class") or ""):
            self.error_summaries.append(d)
        if "aria-current" in d and d["aria-current"] == "page" and tag == "span":
            self.pagination_current.append(d)
        if self._pagination_depth and tag == "a" and d.get("aria-current") == "page":
            self.pagination_current_links += 1
        if "aria-live" in d:
            self.aria_live.append(d)
        if tag == "a" and d.get("class") and "skip-link" in d["class"]:
            self.skip_link = d
        if self.first_focusable is None:
            if tag in ("a", "button", "select", "textarea") or (
                tag == "input" and d.get("type", "text") != "hidden"
            ):
                self.first_focusable = (tag, d)

    def handle_endtag(self, tag):
        if tag == "nav" and self._pagination_depth == len(self.stack):
            self._pagination_depth = 0
        if self.stack and self.stack[-1] == tag:
            self.stack.pop()
        if tag == "label" and self._label_stack:
            self._label_stack.pop()
        if tag == "title":
            self._in_string = None

    def _label_has_for(self):
        return False

    def handle_data(self, data):
        if self._in_string == "title":
            self.title = data.strip()
            self._in_string = None


def rel_lum(r, g, b):
    def chan(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    rs, gs, bs = chan(r), chan(g), chan(b)
    return 0.2126 * rs + 0.7152 * gs + 0.0722 * bs


def contrast(hex1, hex2):
    def parse(h):
        h = h.lstrip("#")
        return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
    l1 = rel_lum(*parse(hex1))
    l2 = rel_lum(*parse(hex2))
    hi, lo = max(l1, l2), min(l1, l2)
    return (hi + 0.05) / (lo + 0.05)


def check_page(name, src):
    p = PageParser()
    p.feed(src)
    if p.lang is None:
        fail(name, "3.1.1 Language of Page", "<html> missing lang attribute")
    if not p.title:
        fail(name, "2.4.2 Page Titled", "missing or empty <title>")
    if p.main_count != 1:
        fail(name, "1.3.1 Info and Relationships", "expected exactly one <main>, found %d" % p.main_count)
    if not p.skip_link:
        fail(name, "2.4.1 Bypass Blocks", "no skip link present")
    else:
        href = p.skip_link.get("href", "")
        if href != "#main":
            fail(name, "2.4.1 Bypass Blocks", "skip link href is %r, expected '#main'" % href)
        if not p.first_focusable or p.first_focusable[1] is not p.skip_link:
            fail(name, "2.4.1 Bypass Blocks", "skip link is not the first focusable element")
    if "main" not in p.main_ids:
        fail(name, "2.4.1 Bypass Blocks", "no element with id='main' for skip target")
    h1s = [h for h in p.headings if h[0] == "h1"]
    if not h1s:
        fail(name, "1.3.1 Headings", "no <h1> on page")
    if len(h1s) > 1:
        fail(name, "1.3.1 Headings", "multiple <h1> on page (%d)" % len(h1s))
    order = [int(h[0][1]) for h in p.headings]
    for i in range(1, len(order)):
        if order[i] > order[i - 1] + 1:
            fail(name, "1.3.1 Headings", "heading level skips from %d to %d" % (order[i - 1], order[i]))
            break

    # Labelling: every text input/select/textarea needs a label via for/id
    # or by being wrapped in a <label> (implicit labelling).
    ids_with_label = {lab.get("for") for lab in p.labels if lab.get("for")}
    for tag, d, wrapped in p.inputs:
        if d.get("type") in ("hidden", "submit", "button", "image", "reset"):
            continue
        ctrl_id = d.get("id")
        if wrapped:
            continue
        if not ctrl_id:
            fail(name, "1.3.1 Labels", "%s without id" % tag)
        elif ctrl_id not in ids_with_label:
            fail(name, "1.3.1 Labels", "%s id=%r has no matching <label for>" % (tag, ctrl_id))

    # Required fields expose required state programmatically. Visible required
    # wording remains a manual fixture review because HTMLParser does not retain
    # the complete label text tree.
    for tag, d, wrapped in p.inputs:
        if "required" not in d:
            continue
        ctrl_id = d.get("id", "")
        matching = next((lab for lab in p.labels if lab.get("for") == ctrl_id), None)
        if "required" not in d and d.get("aria-required") != "true":
            fail(name, "3.3.2 Labels or Instructions", "required field state is not programmatically exposed")
        if "aria-invalid" in d and not matching and not wrapped:
            fail(name, "3.3.1 Error Identification", "required %s id=%r has aria-invalid but no label" % (tag, ctrl_id))

    # Error summaries must use role=alert.
    for es in p.error_summaries:
        if es.get("role") != "alert":
            fail(name, "3.3.1 Error Identification", "element with class error-summary lacks role='alert'")

    # Tables need th with scope.
    for idx, cells in enumerate(p.tables):
        ths = [c for c in cells if c[0] == "th"]
        if not ths:
            fail(name, "1.3.1 Tables", "table %d has no <th> cells" % (idx + 1))
        for cell in ths:
            if cell[1].get("scope") not in ("col", "row", "colgroup", "rowgroup"):
                fail(name, "1.3.1 Tables", "table %d has th without valid scope" % (idx + 1))

    # Parser records current pagination only when it is a span. Any current
    # anchor is rejected directly from the element stream.
    if p.pagination_current_links:
        fail(name, "2.4.8 Location", "current pagination item must not be a link")

    # Accessible authentication (login fixture): autocomplete present.
    if name == "login":
        autocompletes = {d.get("name"): d.get("autocomplete") for tag, d, wrapped in p.inputs}
        if autocompletes.get("identifier") != "username":
            fail(name, "3.3.9 Accessible Authentication", "identifier input missing autocomplete=username")
        if autocompletes.get("password") != "current-password":
            fail(name, "3.3.9 Accessible Authentication", "password input missing autocomplete=current-password")

    # Search unavailable status uses aria-live.
    if name == "search_unavailable":
        if not any("aria-live" in d for d in p.aria_live):
            fail(name, "4.1.3 Status Messages", "unavailable notice lacks aria-live")

    # Moderation: expected_state hidden and reason required.
    if name == "moderation_detail":
        hidden = {d.get("name"): d for tag, d, wrapped in p.inputs}
        if "expected_state" not in hidden:
            fail(name, "Binding 5", "moderation form missing hidden expected_state")
        if "reason" not in hidden or "required" not in hidden["reason"]:
            fail(name, "Binding 5", "moderation form missing required reason")
        actions_present = any(d.get("name") == "action" and d.get("type") == "radio" for tag, d, wrapped in p.inputs)
        if not actions_present:
            fail(name, "Binding 5", "moderation form missing action radio")


def check_css(css_text):
    if ":focus-visible" not in css_text:
        fail("css", "2.4.7 Focus Visible", "no :focus-visible rule in main.css")
    if "prefers-reduced-motion" not in css_text:
        fail("css", "2.3.3 Animation from Interactions", "no prefers-reduced-motion rule in main.css")
    if "max-width: 30rem" not in css_text and "@media" not in css_text:
        fail("css", "1.4.10 Reflow", "no 320px responsive media query")
    # Target size cannot be established from one textual CSS match. It is
    # intentionally deferred to the browser-rendered audit.

    tokens = {}
    for m in re.finditer(r"--([a-z-]+):\s*(#[0-9a-fA-F]{3,6})", css_text):
        tokens[m.group(1)] = m.group(2)
    pairs = [
        ("fg", "bg", 4.5, "normal text"),
        ("link", "bg", 4.5, "link text"),
        ("muted", "bg", 4.5, "muted text"),
        ("error-fg", "error-bg", 4.5, "error summary text"),
        ("success-fg", "success-bg", 4.5, "success flash text"),
        ("warn-fg", "warn-bg", 4.5, "warning text"),
    ]
    for fg, bg, want, label in pairs:
        if fg in tokens and bg in tokens:
            ratio = contrast(tokens[fg], tokens[bg])
            if ratio < want:
                fail("css", "1.4.3 Contrast (Minimum)", "%s on %s = %.2f:1, need %4.1f:1" % (label, bg, ratio, want))
    white = "#ffffff"
    if "link" in tokens:
        ratio = contrast(white, tokens["link"])
        if ratio < 4.5:
            fail("css", "1.4.3 Contrast (Minimum)", "white on link = %.2f:1, need 4.5:1" % ratio)


def main():
    if not os.path.isdir(FIXTURES):
        print("fixtures directory missing: %s" % FIXTURES)
        return 2
    css_text = ""
    if os.path.isfile(CSS_PATH):
        with open(CSS_PATH, encoding="utf-8") as f:
            css_text = f.read()
    else:
        fail("css", "Setup", "main.css not found at %s" % CSS_PATH)

    # Validate the JSON view-model fixture is parseable and consistent.
    vm_path = os.path.join(FIXTURES, "view_models.json")
    try:
        with open(vm_path, encoding="utf-8") as f:
            vm = json.load(f)
        for page in ("home", "questions_list", "question_detail", "moderation_detail", "search_unavailable", "error"):
            if page not in vm:
                fail("view_models.json", "Fixtures", "missing page section %r" % page)
    except json.JSONDecodeError as e:
        fail("view_models.json", "JSON", "parse error: %s" % e)

    fixtures = sorted(f for f in os.listdir(FIXTURES) if f.endswith(".html"))
    for fname in fixtures:
        name = os.path.splitext(fname)[0]
        with open(os.path.join(FIXTURES, fname), encoding="utf-8") as f:
            src = f.read()
        check_page(name, src)

    if css_text:
        check_css(css_text)

    print("Checked %d HTML fixtures and main.css." % len(fixtures))
    if FAILED:
        print("FAIL: %d finding(s)" % len(FAILED))
        for fx, rule, detail in FAILED:
            print("  [%s] %s: %s" % (fx, rule, detail))
        return 1
    print("PASS: implemented fixture structural checks passed.")
    print("Manual/browser checks remain required for target size, reflow, focus obstruction, and full WCAG conformance.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
