import { describe, expect, it, vi, beforeEach } from 'vitest';

/**
 * Difficulty vote action, against the mocked API.
 *
 * Mutation-checked: reading the public ID from a hidden form field instead of
 * re-deriving it from the slug fails the tampering test; dropping the integer
 * range check fails the out-of-range tests; adding a retry to the PUT fails
 * the no-retry test.
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

const QUESTION = {
	public_id: 'q-public-id',
	slug: 'detect-a-cycle',
	title: 'Detect a cycle',
	company: null,
	role: null,
	round: 'coding',
	source_year: 2026,
	topics: [],
	difficulty: { mean: 3, vote_count: 0 },
	published_at: '2026-01-01T00:00:00Z',
	prompt: 'Given a singly linked list, detect whether it contains a cycle.',
	answer_guidance: null,
	author: null
};

function json(body: unknown, status = 200, headers: Record<string, string> = {}) {
	return new Response(JSON.stringify(body), {
		status,
		headers: { 'content-type': 'application/json', ...headers }
	});
}

function apiError(status: number, code: string, headers: Record<string, string> = {}) {
	return new Response(
		JSON.stringify({ error: { code, message: 'Safe message.', request_id: 'req-2' } }),
		{ status, headers: { 'content-type': 'application/json', ...headers } }
	);
}

async function runVote(
	fields: Record<string, string>,
	putResponse: () => Promise<Response> = async () =>
		json({ difficulty: { mean: 3.4, vote_count: 8 }, my_vote: 4 })
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
			if ((init?.method ?? 'GET') === 'GET') return Promise.resolve(json(QUESTION));
			return putResponse();
		}
	};
	/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
	const result = await (actions.vote as any)(event);
	return { result, calls };
}

beforeEach(() => {
	for (const k of Object.keys(env)) delete env[k];
	env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
	env.PLACEDB_PUBLIC_ORIGIN = 'https://placedb.example';
	vi.resetModules();
});

describe('casting a vote', () => {
	it('PUTs the value to the question resolved from the slug', async () => {
		const { result, calls } = await runVote({ _csrf: 'token-abc', value: '4' });
		const put = calls.find((c) => c.init.method === 'PUT');
		expect(put?.url).toBe('http://api.internal/api/v1/questions/q-public-id/difficulty');
		expect(JSON.parse(String(put?.init.body))).toEqual({ value: 4 });
		const headers = put?.init.headers as Record<string, string>;
		expect(headers['x-csrf-token']).toBe('token-abc');
		expect(headers.origin).toBe('https://placedb.example');
		expect(result).toEqual({ difficulty: { mean: 3.4, vote_count: 8 }, myVote: 4 });
	});

	it('ignores a public ID supplied by the form', async () => {
		/*
		 * A tampered hidden field must not aim the vote at another question.
		 * The ID comes from the slug lookup, never from the submitted body.
		 */
		const { calls } = await runVote({
			_csrf: 'token-abc',
			value: '4',
			public_id: 'someone-elses-question'
		});
		const put = calls.find((c) => c.init.method === 'PUT');
		expect(put?.url).toContain('/questions/q-public-id/difficulty');
		expect(put?.url).not.toContain('someone-elses-question');
	});

	it('does not retry the mutation when the API is down', async () => {
		let attempts = 0;
		const { result, calls } = await runVote({ _csrf: 'token-abc', value: '3' }, async () => {
			attempts += 1;
			throw new TypeError('connection reset');
		});
		expect(attempts).toBe(1);
		expect(calls.filter((c) => c.init.method === 'PUT')).toHaveLength(1);
		expect((result as { status: number }).status).toBe(502);
	});
});

describe('vote refusals', () => {
	it('refuses without a CSRF token and never calls the API', async () => {
		const { actions } = await import('./+page.server');
		const calls: Call[] = [];
		const event = {
			cookies: fakeCookies(),
			params: { slug: 'detect-a-cycle' },
			url: new URL('http://localhost/questions/detect-a-cycle'),
			request: new Request('http://localhost/questions/detect-a-cycle', {
				method: 'POST',
				body: new URLSearchParams({ value: '4' }),
				headers: { 'content-type': 'application/x-www-form-urlencoded' }
			}),
			fetch: (url: string, init: RequestInit) => {
				calls.push({ url, init });
				return Promise.resolve(json({}));
			}
		};
		/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
		const result = await (actions.vote as any)(event);
		expect((result as { status: number }).status).toBe(403);
		expect(calls).toHaveLength(0);
	});

	it.each(['0', '6', '3.5', 'four', ''])('rejects %s as a difficulty value', async (value) => {
		const { result, calls } = await runVote({ _csrf: 'token-abc', value });
		expect((result as { status: number }).status).toBe(400);
		expect(calls).toHaveLength(0);
	});

	it('offers a return path when the session expired', async () => {
		const { result } = await runVote({ _csrf: 'token-abc', value: '4' }, async () =>
			apiError(401, 'AUTH_REQUIRED')
		);
		const { status, data } = result as {
			status: number;
			data: { voteError: string; voteLoginHref: string };
		};
		expect(status).toBe(401);
		expect(data.voteLoginHref).toBe('/login?next=%2Fquestions%2Fdetect-a-cycle');
	});

	it('states the refusal when the API forbids the vote', async () => {
		/* The author of an anonymous question is invisible to the page. */
		const { result } = await runVote({ _csrf: 'token-abc', value: '4' }, async () =>
			apiError(403, 'FORBIDDEN')
		);
		const { status, data } = result as { status: number; data: { voteError: string } };
		expect(status).toBe(403);
		expect(data.voteError).toBe('You cannot rate this question.');
	});

	it('shows the retry window on a rate limit', async () => {
		const { result } = await runVote({ _csrf: 'token-abc', value: '4' }, async () =>
			apiError(429, 'RATE_LIMITED', { 'retry-after': '45' })
		);
		const { status, data } = result as { status: number; data: { voteError: string } };
		expect(status).toBe(429);
		expect(data.voteError).toContain('45');
	});
});
