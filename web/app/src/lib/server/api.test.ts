import { describe, expect, it, vi, beforeEach } from 'vitest';

/**
 * Mocked-Drogon tests for the server API boundary.
 *
 * No network and no database. Each test supplies a fake fetch and a fake
 * cookie jar, so these assert the behavior the contract requires rather than
 * that a live backend happens to agree.
 */

const env: Record<string, string> = {};
vi.mock('$env/dynamic/private', () => ({ env }));
vi.mock('$app/environment', () => ({ building: false, dev: false }));

interface CookieRecord {
	value: string;
	options: Record<string, unknown>;
}

function fakeCookies(initial: Record<string, string> = {}) {
	const jar = new Map<string, CookieRecord>();
	for (const [k, v] of Object.entries(initial)) jar.set(k, { value: v, options: {} });
	return {
		jar,
		get: (name: string) => jar.get(name)?.value,
		set: (name: string, value: string, options: Record<string, unknown>) => {
			jar.set(name, { value, options });
		},
		delete: (name: string) => jar.delete(name)
	};
}

function makeEvent(
	fetchImpl: (url: string, init: RequestInit) => Promise<Response>,
	cookies = fakeCookies(),
	requestHeaders: Record<string, string> = {}
) {
	const calls: { url: string; init: RequestInit }[] = [];
	return {
		calls,
		cookies,
		event: {
			cookies,
			request: new Request('http://localhost/page', { headers: requestHeaders }),
			fetch: (url: string, init: RequestInit) => {
				calls.push({ url, init });
				return fetchImpl(url, init);
			}
		}
	};
}

function json(body: unknown, init: ResponseInit = {}) {
	return new Response(JSON.stringify(body), {
		status: 200,
		headers: { 'content-type': 'application/json' },
		...init
	});
}

beforeEach(() => {
	for (const key of Object.keys(env)) delete env[key];
	env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
	env.PLACEDB_PUBLIC_ORIGIN = 'https://placedb.example';
	vi.resetModules();
});

describe('session cookie forwarding', () => {
	it('forwards only the four Drogon cookies to the API', async () => {
		const { apiCall } = await import('./api');
		const cookies = fakeCookies({
			'__Host-placedb_session': 'sess-value',
			'__Host-placedb_login_csrf': 'csrf-value',
			unrelated_analytics: 'should-not-travel'
		});
		const { event, calls } = makeEvent(async () => json({ ok: true }), cookies);

		await apiCall(event as never, '/me');

		const sent = String((calls[0].init.headers as Record<string, string>).cookie);
		expect(sent).toContain('__Host-placedb_session=sess-value');
		expect(sent).toContain('__Host-placedb_login_csrf=csrf-value');
		expect(sent).not.toContain('unrelated_analytics');
	});

	it('replays Set-Cookie from the API with its attributes intact', async () => {
		const { apiCall } = await import('./api');
		const cookies = fakeCookies();
		const { event } = makeEvent(
			async () =>
				json(
					{ ok: true },
					{
						headers: {
							'content-type': 'application/json',
							'set-cookie':
								'__Host-placedb_session=new-token; Path=/; Max-Age=2592000; HttpOnly; Secure; SameSite=Lax'
						}
					}
				),
			cookies
		);

		await apiCall(event as never, '/auth/login', { method: 'POST' });

		const stored = cookies.jar.get('__Host-placedb_session');
		expect(stored?.value).toBe('new-token');
		expect(stored?.options.httpOnly).toBe(true);
		expect(stored?.options.secure).toBe(true);
		expect(stored?.options.sameSite).toBe('lax');
		expect(stored?.options.maxAge).toBe(2592000);
	});

	it('drops a Set-Cookie the API is not supposed to set', async () => {
		const { apiCall } = await import('./api');
		const cookies = fakeCookies();
		const { event } = makeEvent(
			async () =>
				json(
					{ ok: true },
					{
						headers: {
							'content-type': 'application/json',
							'set-cookie': 'evil_tracking=1; Path=/'
						}
					}
				),
			cookies
		);

		await apiCall(event as never, '/me');

		expect(cookies.jar.has('evil_tracking')).toBe(false);
	});
});

describe('origin on mutations', () => {
	it('sends the exact configured public origin on a mutation', async () => {
		const { apiCall } = await import('./api');
		const { event, calls } = makeEvent(async () => json({ ok: true }));

		await apiCall(event as never, '/auth/logout', { method: 'POST' });

		expect((calls[0].init.headers as Record<string, string>).origin).toBe(
			'https://placedb.example'
		);
	});

	it('does not send an origin on a read', async () => {
		const { apiCall } = await import('./api');
		const { event, calls } = makeEvent(async () => json({ ok: true }));

		await apiCall(event as never, '/questions');

		expect((calls[0].init.headers as Record<string, string>).origin).toBeUndefined();
	});
});

describe('error handling', () => {
	it('surfaces field errors from the accepted envelope', async () => {
		const { apiCall, ApiError } = await import('./api');
		const { event } = makeEvent(
			async () =>
				json(
					{
						error: {
							code: 'INVALID_CREDENTIALS',
							message: 'Those details did not match.',
							request_id: 'req-1',
							details: { fields: [{ field: 'identity', code: 'BAD', message: 'No match.' }] }
						}
					},
					{ status: 401 }
				)
		);

		await expect(apiCall(event as never, '/auth/login', { method: 'POST' })).rejects.toThrow(
			ApiError
		);
		try {
			await apiCall(event as never, '/auth/login', { method: 'POST' });
		} catch (error) {
			const api = error as InstanceType<typeof ApiError>;
			expect(api.status).toBe(401);
			expect(api.code).toBe('INVALID_CREDENTIALS');
			expect(api.requestId).toBe('req-1');
			expect(api.fieldMessage('identity')).toBe('No match.');
		}
	});

	it('reports an outage as ApiUnavailable, distinct from a refusal', async () => {
		const { apiCall, ApiUnavailable } = await import('./api');
		const { event } = makeEvent(async () => {
			throw new Error('ECONNREFUSED');
		});

		await expect(apiCall(event as never, '/questions')).rejects.toThrow(ApiUnavailable);
	});
});
