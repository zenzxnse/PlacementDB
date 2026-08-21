import { describe, expect, it, vi, beforeEach } from 'vitest';

/**
 * Comment report and moderator hide actions on the question page, against the
 * mocked API. The experience page runs the identical helpers through the same
 * wire paths, so its wiring is covered by the shared code plus its own
 * svelte-check types.
 *
 * Mutation-checked: removing the reason allowlist check fails the bogus-reason
 * test; dropping the requireModerator call in hide fails the non-moderator
 * test; reading the hide reason untrimmed fails the whitespace test.
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
		JSON.stringify({ error: { code, message: 'Safe message.', request_id: 'req-9' } }),
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
	unread_moderation_count: 0
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

async function runAction(
	name: 'report' | 'hide',
	fields: Record<string, string>,
	responses: { me?: Response; mutation?: Response } = {}
) {
	const { actions } = await import('./+page.server');
	const calls: Call[] = [];
	const event = {
		cookies: fakeCookies(),
		params: { slug: 'detect-a-cycle' },
		url: new URL('http://localhost/questions/detect-a-cycle'),
		request: new Request('http://localhost/questions/detect-a-cycle', {
			method: 'POST',
			body: new URLSearchParams(fields),
			headers: { 'content-type': 'application/x-www-form-urlencoded' }
		}),
		fetch: (url: string, init: RequestInit) => {
			calls.push({ url, init });
			if ((init?.method ?? 'GET') === 'GET') {
				return Promise.resolve(responses.me ?? json(MODERATOR));
			}
			if (responses.mutation) return Promise.resolve(responses.mutation);
			return Promise.resolve(json({ public_id: 'c-1', state: 'open' }));
		}
	};
	/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
	const result = await (actions[name] as any)(event);
	return { result, calls };
}

beforeEach(() => {
	for (const k of Object.keys(env)) delete env[k];
	env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
	env.PLACEDB_PUBLIC_ORIGIN = 'https://placedb.example';
	vi.resetModules();
});

describe('reporting a comment', () => {
	it('posts the reason and omits blank details', async () => {
		const { result, calls } = await runAction('report', {
			_csrf: 'token-abc',
			comment_id: 'c-1',
			reason: 'spam',
			details: '   '
		});
		const post = calls.find((c) => c.init.method === 'POST');
		expect(post?.url).toBe('http://api.internal/api/v1/comments/c-1/reports');
		expect(JSON.parse(String(post?.init.body))).toEqual({ reason: 'spam' });
		const headers = post?.init.headers as Record<string, string>;
		expect(headers['x-csrf-token']).toBe('token-abc');
		expect(headers.origin).toBe('https://placedb.example');
		expect((result as { reportResult: { kind: string } }).reportResult.kind).toBe('reported');
	});

	it('sends trimmed non-empty details', async () => {
		const { calls } = await runAction('report', {
			_csrf: 'token-abc',
			comment_id: 'c-1',
			reason: 'incorrect',
			details: '  wrong company  '
		});
		const post = calls.find((c) => c.init.method === 'POST');
		expect(JSON.parse(String(post?.init.body))).toEqual({
			reason: 'incorrect',
			details: 'wrong company'
		});
	});

	it('refuses a reason outside the accepted set without calling the API', async () => {
		const { result, calls } = await runAction('report', {
			_csrf: 'token-abc',
			comment_id: 'c-1',
			reason: 'i-dislike-it'
		});
		expect(calls).toHaveLength(0);
		expect((result as { status: number }).status).toBe(400);
	});

	it('refuses to send a report without a CSRF token', async () => {
		const { result, calls } = await runAction('report', {
			comment_id: 'c-1',
			reason: 'spam'
		});
		expect(calls).toHaveLength(0);
		expect((result as { status: number }).status).toBe(400);
	});

	it('offers a return path when the session expired', async () => {
		const { result } = await runAction(
			'report',
			{ _csrf: 'token-abc', comment_id: 'c-1', reason: 'spam' },
			{ mutation: apiError(401, 'AUTH_REQUIRED') }
		);
		const data = (result as { data: { reportResult: { message: string; loginHref?: string } } })
			.data.reportResult;
		expect(data.message).toContain('session expired');
		expect(data.loginHref).toBe(
			'/login?next=%2Fquestions%2Fdetect-a-cycle%23comments-heading'
		);
	});

	it('explains a duplicate open report', async () => {
		const { result } = await runAction(
			'report',
			{ _csrf: 'token-abc', comment_id: 'c-1', reason: 'spam' },
			{ mutation: apiError(409, 'CONFLICT') }
		);
		const data = (result as { data: { reportResult: { message: string } } }).data.reportResult;
		expect(data.message).toContain('already have an open report');
	});

	it('shows the retry window on a rate limit', async () => {
		const { result } = await runAction(
			'report',
			{ _csrf: 'token-abc', comment_id: 'c-1', reason: 'spam' },
			{ mutation: apiError(429, 'RATE_LIMITED', { 'retry-after': '30' }) }
		);
		const data = (result as { data: { reportResult: { message: string } } }).data.reportResult;
		expect(data.message).toContain('30');
	});
});

describe('hiding a comment as a moderator', () => {
	it('checks the moderator gate, then posts the trimmed reason', async () => {
		const { result, calls } = await runAction('hide', {
			_csrf: 'token-abc',
			comment_id: 'c-1',
			reason: '  off-topic thread  '
		});
		const get = calls.find((c) => (c.init.method ?? 'GET') === 'GET');
		expect(get?.url).toBe('http://api.internal/api/v1/me');
		const post = calls.find((c) => c.init.method === 'POST');
		expect(post?.url).toBe('http://api.internal/api/v1/moderation/comments/c-1/hide');
		expect(JSON.parse(String(post?.init.body))).toEqual({ reason: 'off-topic thread' });
		expect((result as { hideResult: { kind: string } }).hideResult.kind).toBe('hidden');
	});

	it('refuses an empty reason without posting', async () => {
		const { result, calls } = await runAction('hide', {
			_csrf: 'token-abc',
			comment_id: 'c-1',
			reason: '   '
		});
		expect(calls.filter((c) => c.init.method === 'POST')).toHaveLength(0);
		expect((result as { status: number }).status).toBe(400);
	});

	it('rejects a non-moderator before any mutation', async () => {
		await expect(
			runAction('hide', { _csrf: 'token-abc', comment_id: 'c-1', reason: 'x' }, { me: json(CONTRIBUTOR) })
		).rejects.toThrow();
	});

	it('explains a concurrent hide conflict', async () => {
		const { result } = await runAction(
			'hide',
			{ _csrf: 'token-abc', comment_id: 'c-1', reason: 'spam' },
			{ mutation: apiError(409, 'CONFLICT') }
		);
		const data = (result as { data: { hideResult: { message: string } } }).data.hideResult;
		expect(data.message).toContain('already hidden');
	});
});
