import { describe, expect, it, vi, beforeEach } from 'vitest';

/**
 * /moderation/reports load and resolve action, against the mocked API.
 *
 * The load gates on can_moderate exactly like the queue and audit pages:
 * anonymous visitors are redirected to login with a return path, and signed-in
 * non-moderators get a 403. The resolve action sends the compare-and-set
 * expected_state the accepted contract requires, so a race between two
 * moderators surfaces as a reload-and-review message rather than a silent
 * overwrite.
 *
 * Mutation-checked: dropping the expected_state field from the request body
 * fails the compare-and-set test; widening the state filter check to accept
 * anything fails the bogus-state test.
 */

const env: Record<string, string> = {};
vi.mock('$env/dynamic/private', () => ({ env }));
vi.mock('$app/environment', () => ({ building: false, dev: false }));

function fakeCookies() {
	const jar = new Map<string, { value: string; options: Record<string, unknown> }>();
	return {
		get: (n: string) => jar.get(n)?.value,
		set: (n: string, v: string, o: Record<string, unknown>) => jar.set(n, { value: v, options: o }),
		delete: (n: string) => jar.delete(n)
	};
}

interface Call {
	url: string;
	init: RequestInit;
}

function json(body: unknown, status = 200, headers: Record<string, string> = {}) {
	return new Response(JSON.stringify(body), {
		status,
		headers: { 'content-type': 'application/json', ...headers }
	});
}

function apiError(status: number, code: string, headers: Record<string, string> = {}) {
	return new Response(
		JSON.stringify({ error: { code, message: 'Safe message.', request_id: 'req-7' } }),
		{ status, headers: { 'content-type': 'application/json', ...headers } }
	);
}

const MODERATOR = {
	public_id: 'u-mod',
	username: 'mod_001',
	display_name: 'Mod One',
	role: 'moderator',
	status: 'active',
	can_submit: true,
	can_moderate: true,
	unread_moderation_count: 2
};

const CONTRIBUTOR = {
	public_id: 'u-user',
	username: 'student_001',
	display_name: 'Student One',
	role: 'user',
	status: 'active',
	can_submit: true,
	can_moderate: false,
	unread_moderation_count: 0
};

const REPORTS = {
	items: [
		{
			public_id: 'r-1',
			target_type: 'comment',
			reason: 'spam',
			details: null,
			state: 'open',
			reporter_label: 'student_001',
			created_at: '2026-01-01T00:00:00Z'
		}
	],
	next_cursor: null
};

function makeEvent(
	search: string,
	fetchImpl: (url: string, init: RequestInit) => Promise<Response>,
	method = 'GET',
	fields?: Record<string, string>
) {
	const calls: Call[] = [];
	return {
		calls,
		event: {
			cookies: fakeCookies(),
			url: new URL(`http://localhost/moderation/reports${search}`),
			request: new Request(`http://localhost/moderation/reports${search}`, {
				method,
				...(fields
					? {
							body: new URLSearchParams(fields),
							headers: { 'content-type': 'application/x-www-form-urlencoded' }
						}
					: {})
			}),
			fetch: (url: string, init: RequestInit) => {
				calls.push({ url, init });
				return fetchImpl(url, init);
			}
		}
	};
}

function routeFetch(
	meResponse: () => Response,
	reportsResponse: () => Response = () => json(REPORTS),
	csrfResponse: () => Response = () => json({ csrf_token: 'token-abc' }),
	mutationResponse?: () => Response
) {
	return (url: string, init: RequestInit): Promise<Response> => {
		if (url.endsWith('/me')) return Promise.resolve(meResponse());
		if (url.includes('/moderation/reports?')) return Promise.resolve(reportsResponse());
		if (url.endsWith('/auth/csrf')) return Promise.resolve(csrfResponse());
		if (mutationResponse && (init?.method ?? 'GET') !== 'GET') {
			return Promise.resolve(mutationResponse());
		}
		return Promise.resolve(json({}));
	};
}

beforeEach(() => {
	for (const k of Object.keys(env)) delete env[k];
	env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
	env.PLACEDB_PUBLIC_ORIGIN = 'https://placedb.example';
	vi.resetModules();
});

async function runLoad(search: string, fetchImpl: (url: string, init: RequestInit) => Promise<Response>) {
	const { load } = await import('./+page.server');
	const { calls, event } = makeEvent(search, fetchImpl);
	try {
		const data = await (load as (e: unknown) => Promise<unknown>)(event);
		return { data, calls, thrown: undefined as undefined | { status: number; location?: string } };
	} catch (error_) {
		return { data: undefined, calls, thrown: error_ as { status: number; location?: string } };
	}
}

describe('reports page load', () => {
	it('redirects an anonymous visitor to login with a return path', async () => {
		const { thrown, calls } = await runLoad(
			'',
			routeFetch(() => json(null))
		);
		expect(thrown?.status).toBe(303);
		expect(thrown?.location).toBe('/login?next=%2Fmoderation%2Freports');
		expect(calls.filter((c) => c.url.includes('/moderation/reports?'))).toHaveLength(0);
	});

	it('refuses a signed-in non-moderator', async () => {
		const { thrown } = await runLoad('', routeFetch(() => json(CONTRIBUTOR)));
		expect(thrown?.status).toBe(403);
	});

	it('loads open reports by default for a moderator', async () => {
		const { data, calls } = await runLoad('', routeFetch(() => json(MODERATOR)));
		const read = calls.find((c) => c.url.includes('/moderation/reports?'));
		expect(read?.url).toBe('http://api.internal/api/v1/moderation/reports?state=open');
		const reports = (data as { reports: { items: { public_id: string }[] } }).reports;
		expect(reports.items[0]?.public_id).toBe('r-1');
	});

	it('passes the requested state and cursor through', async () => {
		const { calls } = await runLoad(
			'?state=resolved&cursor=abc',
			routeFetch(() => json(MODERATOR))
		);
		const read = calls.find((c) => c.url.includes('/moderation/reports?'));
		expect(read?.url).toContain('state=resolved');
		expect(read?.url).toContain('cursor=abc');
	});

	it('rejects a state outside the accepted set', async () => {
		const { thrown } = await runLoad('?state=deleted', routeFetch(() => json(MODERATOR)));
		expect(thrown?.status).toBe(400);
	});
});

describe('resolving a report', () => {
	async function runResolve(
		fields: Record<string, string>,
		mutationResponse?: () => Response
	) {
		const { actions } = await import('./+page.server');
		const { calls, event } = makeEvent('', routeFetch(
			() => json(MODERATOR),
			undefined,
			undefined,
			mutationResponse
		), 'POST', fields);
		/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
		const result = await (actions.resolve as any)(event);
		return { result, calls };
	}

	it('sends the compare-and-set body the contract requires', async () => {
		const { result, calls } = await runResolve({
			_csrf: 'token-abc',
			public_id: 'r-1',
			expected_state: 'open',
			decision: 'resolved',
			reason: 'Confirmed spam. Comment hidden.'
		});
		const post = calls.find((c) => c.init.method === 'POST');
		expect(post?.url).toBe('http://api.internal/api/v1/moderation/reports/r-1/resolve');
		expect(JSON.parse(String(post?.init.body))).toEqual({
			expected_state: 'open',
			decision: 'resolved',
			reason: 'Confirmed spam. Comment hidden.'
		});
		const headers = post?.init.headers as Record<string, string>;
		expect(headers['x-csrf-token']).toBe('token-abc');
		expect((result as { message: string }).message).toContain('recorded');
	});

	it('refuses a missing reason without posting', async () => {
		const { result, calls } = await runResolve({
			_csrf: 'token-abc',
			public_id: 'r-1',
			expected_state: 'open',
			decision: 'dismissed',
			reason: '   '
		});
		expect(calls.filter((c) => c.init.method === 'POST')).toHaveLength(0);
		expect((result as { status: number }).status).toBe(400);
	});

	it('refuses an unknown decision', async () => {
		const { result, calls } = await runResolve({
			_csrf: 'token-abc',
			public_id: 'r-1',
			expected_state: 'open',
			decision: 'escalate',
			reason: 'x'
		});
		expect(calls.filter((c) => c.init.method === 'POST')).toHaveLength(0);
		expect((result as { status: number }).status).toBe(400);
	});

	it('turns a concurrent resolution into reload-and-review guidance', async () => {
		const { result } = await runResolve(
			{
				_csrf: 'token-abc',
				public_id: 'r-1',
				expected_state: 'open',
				decision: 'resolved',
				reason: 'x'
			},
			() => apiError(409, 'CONFLICT')
		);
		const { status, data } = result as { status: number; data: { message: string; failed: boolean } };
		expect(status).toBe(409);
		expect(data.failed).toBe(true);
		expect(data.message).toContain('Reload');
	});

	it('shows the retry window on a rate limit', async () => {
		const { result } = await runResolve(
			{
				_csrf: 'token-abc',
				public_id: 'r-1',
				expected_state: 'open',
				decision: 'dismissed',
				reason: 'x'
			},
			() => apiError(429, 'RATE_LIMITED', { 'retry-after': '20' })
		);
		const { status, data } = result as { status: number; data: { message: string } };
		expect(status).toBe(429);
		expect(data.message).toContain('20');
	});
});
