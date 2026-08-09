import { env } from '$env/dynamic/private';
import { building, dev } from '$app/environment';

/**
 * Server-only configuration.
 *
 * Imported through $env/dynamic/private so nothing here can reach a client
 * bundle. Values are read at request time rather than build time, so a
 * deployment can change them without rebuilding.
 */

/**
 * The public origin this site is served on, sent as `Origin` on every mutation.
 *
 * Drogon checks it against its configured edge origin. Because the immediate
 * client is SvelteKit rather than a browser, the value must be the public
 * Envoy origin, not the loopback address SvelteKit happens to bind.
 */
export const PUBLIC_ORIGIN = env.PLACEDB_PUBLIC_ORIGIN ?? env.ORIGIN ?? '';

/** Internal base URL for Drogon. Loopback in every current topology. */
export const API_BASE = env.PLACEDB_API_BASE ?? 'http://127.0.0.1:8080/api/v1';

/**
 * Fixture fallback is opt-in and development-only.
 *
 * Two independent conditions must hold: the flag is set, and SvelteKit reports
 * a dev build. Requiring both means a production deployment cannot serve
 * fixtures even if the flag leaks into its environment, which is the failure
 * that would quietly show synthetic placement data to real students.
 */
export const USE_FIXTURES = dev && env.PLACEDB_USE_FIXTURES === 'true';

/**
 * Connect and read timeouts.
 *
 * Browse is short because a slow page is worse than an error page the user can
 * retry. Login is longer because Argon2id deliberately costs about 100 ms per
 * attempt and the bounded executor may queue behind other logins.
 */
export const READ_TIMEOUT_MS = Number(env.PLACEDB_API_TIMEOUT_MS ?? '8000');
export const LOGIN_TIMEOUT_MS = Number(env.PLACEDB_LOGIN_TIMEOUT_MS ?? '15000');

/**
 * Fails the process at startup when production is misconfigured.
 *
 * Called from hooks.server.ts. Skipped during `building`, where no request is
 * served and env vars are legitimately absent.
 */
export function assertConfigured(): void {
	if (building) return;
	if (!dev && env.PLACEDB_USE_FIXTURES === 'true') {
		throw new Error(
			'PLACEDB_USE_FIXTURES is set outside development. Refusing to serve synthetic data as real content.'
		);
	}
	if (!API_BASE.startsWith('http://') && !API_BASE.startsWith('https://')) {
		throw new Error('PLACEDB_API_BASE must be an absolute http or https URL.');
	}
}
