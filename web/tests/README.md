# PlacementDB web tests

Synthetic browser fixtures and a limited WCAG structural checker for the
prototype view layer. No Node, no npm, no JavaScript, no application build.

## Layout

- `fixtures/*.html`: synthetic rendered HTML samples mirroring the CSP template
  output for home, questions list, question detail, login, moderation detail,
  search unavailable, and error pages. No real student identities.
- `fixtures/view_models.json`: the synthetic view-model payloads that produced
  those samples, documenting the GLM view-model contract request to Claude.
- `a11y_check.py`: implemented fixture-only structural checks. Python 3,
  stdlib only. It is not a WCAG conformance audit or a CSP renderer.

## Running the checker

From the repository root:

```
python3 web/tests/a11y_check.py
```

The checker runs sequentially with one worker. It validates:

- one `<main>` landmark and a "Skip to main content" link pointing at `#main`;
- one `<h1>` per page and no skipped heading levels;
- `<html lang>` and a non-empty `<title>`;
- every form control has a programmatic label via `for`/`id` or a wrapping
  `<label>`;
- error summaries use `role="alert"`;
- tables use `<th>` cells;
- status messages use `aria-live`;
- the login fixture has `autocomplete` attributes (accessible authentication);
- the moderation fixture has a hidden `expected_state` and a `required`
  reason field;
- `main.css` has `:focus-visible`, `prefers-reduced-motion`, a 320 px
  responsive media query, minimum target size, and color contrast tokens that
  meet WCAG 2.2 AA against their backgrounds.

Exit status is non-zero on any implemented finding. Target size, reflow, focus
obstruction, computed contrast, CSP/fixture drift, and full WCAG conformance
remain browser or generated-view checks after Codex authorizes the relevant
isolated target.

## Scope

These fixtures mirror GLM's CSP templates by hand. Drift is possible until the
real controller-to-view wiring exists. When Claude publishes the stable
view-model contract, update `view_models.json` to match accepted key names.
