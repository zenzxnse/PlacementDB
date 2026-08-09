export const ssr = true;
// The v1 UI is deliberately server rendered. Disabling hydration keeps basic
// browsing and forms functional without inline bootstrap scripts and lets the
// gateway enforce `script-src 'self'` without unsafe-inline or per-response
// nonce coordination.
export const csr = false;
export const prerender = false;
