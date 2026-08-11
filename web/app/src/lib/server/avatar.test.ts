import { describe, expect, it, vi, beforeEach } from 'vitest';
import { DEFAULT_AVATAR_URL } from '$lib/types';

/**
 * Multipart avatar contract.
 *
 * The rule these tests exist to protect: SvelteKit must not set Content-Type
 * on an upload. fetch has to generate it so the header carries the multipart
 * boundary it actually encoded. A hand-written header has no boundary and the
 * upstream parser rejects the body, which is easy to introduce and invisible
 * until a real Drogon is on the other end.
 */

const env: Record<string, string> = {};
vi.mock('$env/dynamic/private', () => ({ env }));
vi.mock('$app/environment', () => ({ building: false, dev: false }));

function fakeCookies(initial: Record<string, string> = {}) {
	const jar = new Map<string, { value: string; options: Record<string, unknown> }>();
	for (const [k, v] of Object.entries(initial)) jar.set(k, { value: v, options: {} });
	return {
		jar,
		get: (n: string) => jar.get(n)?.value,
		set: (n: string, v: string, o: Record<string, unknown>) => jar.set(n, { value: v, options: o }),
		delete: (n: string) => jar.delete(n)
	};
}

function makeEvent(fetchImpl: (url: string, init: RequestInit) => Promise<Response>) {
	const calls: { url: string; init: RequestInit }[] = [];
	return {
		calls,
		event: {
			cookies: fakeCookies({ '__Host-placedb_session': 'sess' }),
			request: new Request('http://localhost/account/profile'),
			fetch: (url: string, init: RequestInit) => {
				calls.push({ url, init });
				return fetchImpl(url, init);
			}
		}
	};
}

function json(body: unknown, status = 200) {
	return new Response(JSON.stringify(body), {
		status,
		headers: { 'content-type': 'application/json' }
	});
}

function pngFile(bytes = 64): File {
	/* A PNG magic-byte prefix; upstream decodes for real, this is just shape. */
	const header = new Uint8Array([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
	const body = new Uint8Array(Math.max(0, bytes - header.length));
	return new File([header, body], 'holiday-photo.png', { type: 'image/png' });
}

beforeEach(() => {
	for (const k of Object.keys(env)) delete env[k];
	env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
	env.PLACEDB_PUBLIC_ORIGIN = 'https://placedb.example';
	vi.resetModules();
});

describe('apiUpload multipart contract', () => {
	it('never sets Content-Type, so fetch generates the boundary', async () => {
		const { apiUpload } = await import('./api');
		const { event, calls } = makeEvent(async () => json({ avatar_url: '/x.webp' }));
		const form = new FormData();
		form.append('avatar', pngFile(), 'avatar');

		await apiUpload(event as never, '/me/avatar', form);

		const headers = calls[0]!.init.headers as Record<string, string>;
		const keys = Object.keys(headers).map((k) => k.toLowerCase());
		expect(keys).not.toContain('content-type');
	});

	it('sends the FormData body untouched', async () => {
		const { apiUpload } = await import('./api');
		const { event, calls } = makeEvent(async () => json({ avatar_url: '/x.webp' }));
		const form = new FormData();
		form.append('avatar', pngFile(), 'avatar');

		await apiUpload(event as never, '/me/avatar', form);

		expect(calls[0]!.init.body).toBeInstanceOf(FormData);
		expect(calls[0]!.init.method).toBe('POST');
	});

	it('sends CSRF token, exact origin, and the forwarded session', async () => {
		const { apiUpload } = await import('./api');
		const { event, calls } = makeEvent(async () => json({ avatar_url: '/x.webp' }));
		const form = new FormData();
		form.append('avatar', pngFile(), 'avatar');

		await apiUpload(event as never, '/me/avatar', form, { csrfToken: 'tok-123' });

		const headers = calls[0]!.init.headers as Record<string, string>;
		expect(headers['x-csrf-token']).toBe('tok-123');
		expect(headers.origin).toBe('https://placedb.example');
		expect(headers.cookie).toContain('__Host-placedb_session=sess');
	});

	it('maps each documented failure to its classification', async () => {
		const { apiUpload, toFailure } = await import('./api');
		const cases: [number, string, string][] = [
			[400, 'VALIDATION_FAILED', 'validation'],
			[401, 'AUTH_REQUIRED', 'session_expired'],
			[403, 'CSRF_FAILED', 'forbidden'],
			[413, 'PAYLOAD_TOO_LARGE', 'too_large'],
			[415, 'UNSUPPORTED_MEDIA_TYPE', 'unsupported_media'],
			[503, 'SERVICE_UNAVAILABLE', 'dependency']
		];
		for (const [status, code, kind] of cases) {
			const { event } = makeEvent(async () =>
				json({ error: { code, message: 'Safe.', request_id: 'r' } }, status)
			);
			const form = new FormData();
			form.append('avatar', pngFile(), 'avatar');
			try {
				await apiUpload(event as never, '/me/avatar', form);
				expect.unreachable(`expected ${status} to throw`);
			} catch (error) {
				expect(toFailure(error).kind).toBe(kind);
			}
		}
	});

	it('rejects a non-JSON success body rather than trusting it', async () => {
		const { apiUpload, ApiMalformed } = await import('./api');
		const { event } = makeEvent(
			async () => new Response('ok', { status: 200, headers: { 'content-type': 'text/plain' } })
		);
		const form = new FormData();
		form.append('avatar', pngFile(), 'avatar');
		await expect(apiUpload(event as never, '/me/avatar', form)).rejects.toThrow(ApiMalformed);
	});
});

describe('avatar removal', () => {
	it('sends DELETE with the CSRF token and returns the default URL', async () => {
		const { apiCall } = await import('./api');
		const { event, calls } = makeEvent(async () => json({ avatar_url: DEFAULT_AVATAR_URL }));

		const { parseAvatar } = await import('$lib/wire');
		const result = await apiCall(event as never, '/me/avatar', {
			method: 'DELETE',
			headers: { 'x-csrf-token': 'tok' },
			parse: parseAvatar
		});

		expect(calls[0]!.init.method).toBe('DELETE');
		expect((calls[0]!.init.headers as Record<string, string>)['x-csrf-token']).toBe('tok');
		expect(result.avatar_url).toBe(DEFAULT_AVATAR_URL);
	});

	it('never retries the delete, because it is a mutation', async () => {
		const { apiCall } = await import('./api');
		let attempts = 0;
		const { event } = makeEvent(async () => {
			attempts += 1;
			return json({ error: { code: 'SERVICE_UNAVAILABLE', message: 'x', request_id: 'r' } }, 503);
		});
		await expect(
			apiCall(event as never, '/me/avatar', { method: 'DELETE' })
		).rejects.toThrow();
		expect(attempts).toBe(1);
	});
});

describe('avatar fallback', () => {
	it('uses the vendored default when no URL is supplied', () => {
		/* Mirrors Avatar.svelte's resolution, which the component applies before
		 * the browser ever requests an image. */
		const resolve = (src: string | null | undefined) =>
			src && src.trim() ? src : DEFAULT_AVATAR_URL;
		expect(resolve(null)).toBe(DEFAULT_AVATAR_URL);
		expect(resolve('')).toBe(DEFAULT_AVATAR_URL);
		expect(resolve('   ')).toBe(DEFAULT_AVATAR_URL);
		expect(resolve('/api/v1/avatars/abc.webp')).toBe('/api/v1/avatars/abc.webp');
	});
});
