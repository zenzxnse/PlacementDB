import { describe, expect, it, vi, beforeEach } from 'vitest';

/**
 * Registration action, against the mocked API.
 *
 * The properties worth protecting here are the ones that are easy to break by
 * being helpful: preserving the password after a failure, and turning the
 * deliberately vague 409 into a friendly "that username is taken", which is a
 * user-enumeration oracle.
 *
 * Mutation-checked: attaching the DUPLICATE message to the `username` field
 * fails the enumeration tests and nothing else; adding `password` to the
 * preserved object fails the password test alone.
 */

const env: Record<string, string> = {};
vi.mock('$env/dynamic/private', () => ({ env }));
vi.mock('$app/environment', () => ({ building: false, dev: false }));

function fakeCookies() {
	const jar = new Map<string, { value: string; options: Record<string, unknown> }>();
	return {
		jar,
		get: (n: string) => jar.get(n)?.value,
		set: (n: string, v: string, o: Record<string, unknown>) => jar.set(n, { value: v, options: o }),
		delete: (n: string) => jar.delete(n)
	};
}

interface Call {
	url: string;
	init: RequestInit;
}

function makeEvent(fields: Record<string, string>, fetchImpl: (url: string, init: RequestInit) => Promise<Response>) {
	const calls: Call[] = [];
	const body = new URLSearchParams(fields);
	return {
		calls,
		event: {
			cookies: fakeCookies(),
			url: new URL('http://localhost/register'),
			request: new Request('http://localhost/register', {
				method: 'POST',
				body,
				headers: { 'content-type': 'application/x-www-form-urlencoded' }
			}),
			fetch: (url: string, init: RequestInit) => {
				calls.push({ url, init });
				return fetchImpl(url, init);
			}
		}
	};
}

function json(body: unknown, status = 200, headers: Record<string, string> = {}) {
	return new Response(JSON.stringify(body), {
		status,
		headers: { 'content-type': 'application/json', ...headers }
	});
}

function apiError(status: number, code: string, headers: Record<string, string> = {}) {
	return new Response(
		JSON.stringify({ error: { code, message: 'Safe message.', request_id: 'req-1' } }),
		{ status, headers: { 'content-type': 'application/json', ...headers } }
	);
}

const VALID = {
	username: 'student_001',
	email: 'student@example.edu',
	display_name: 'Student One',
	password: 'correct horse battery staple',
	_csrf: 'token-abc'
};

beforeEach(() => {
	for (const k of Object.keys(env)) delete env[k];
	env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
	env.PLACEDB_PUBLIC_ORIGIN = 'https://placedb.example';
	vi.resetModules();
});

async function runAction(fields: Record<string, string>, fetchImpl: (url: string, init: RequestInit) => Promise<Response>) {
	const { actions } = await import('./+page.server');
	const { calls, event } = makeEvent(fields, fetchImpl);
	try {
		/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
		const result = await (actions.default as any)(event);
		return { result, calls, redirected: undefined as undefined | { status: number; location: string } };
	} catch (thrown) {
		const redirect = thrown as { status: number; location: string };
		if (typeof redirect?.status === 'number' && typeof redirect?.location === 'string') {
			return { result: undefined, calls, redirected: redirect };
		}
		throw thrown;
	}
}

describe('registration success', () => {
	it('posts the contract fields with the token in the header and the body', async () => {
		const { calls, redirected } = await runAction(VALID, async () =>
			json({ username: 'student_001' }, 201)
		);
		expect(redirected).toEqual({ status: 303, location: '/' });
		expect(calls).toHaveLength(1);
		expect(calls[0]!.url).toBe('http://api.internal/api/v1/auth/register');
		expect(calls[0]!.init.method).toBe('POST');
		const headers = calls[0]!.init.headers as Record<string, string>;
		expect(headers['x-csrf-token']).toBe('token-abc');
		/* Exact configured public Origin on every mutation. */
		expect(headers.origin).toBe('https://placedb.example');
		expect(JSON.parse(String(calls[0]!.init.body))).toEqual({
			username: 'student_001',
			email: 'student@example.edu',
			display_name: 'Student One',
			password: 'correct horse battery staple',
			csrf_token: 'token-abc'
		});
	});
});

describe('registration refusals', () => {
	it('never preserves the password after a failure', async () => {
		const { result } = await runAction(VALID, async () => apiError(409, 'DUPLICATE'));
		const data = (result as { data: Record<string, unknown> }).data;
		expect(JSON.stringify(data)).not.toContain('correct horse battery staple');
		/* The safe identity fields do come back, so the form is not emptied. */
		expect(data.username).toBe('student_001');
		expect(data.email).toBe('student@example.edu');
	});

	it('answers a duplicate identifier without naming which one collided', async () => {
		const { result } = await runAction(VALID, async () => apiError(409, 'DUPLICATE'));
		const { status, data } = result as {
			status: number;
			data: { errors: { field: string; message: string }[] };
		};
		expect(status).toBe(409);
		expect(data.errors).toHaveLength(1);
		/*
		 * Form-level only. A field-attached message would point at the username
		 * or the email box and tell a stranger which identifier exists.
		 */
		expect(data.errors[0]!.field).toBe('form');
		const message = data.errors[0]!.message.toLowerCase();
		expect(message).not.toContain('username');
		expect(message).not.toContain('email');
		expect(message).not.toContain('taken');
		expect(message).not.toContain('exists');
	});

	it('refuses to post without a CSRF token', async () => {
		const { result, calls } = await runAction({ ...VALID, _csrf: '' }, async () => {
			throw new Error('the API must not be called');
		});
		expect((result as { status: number }).status).toBe(403);
		expect(calls).toHaveLength(0);
	});

	it('rejects a malformed username before spending an Argon2id slot', async () => {
		const { result, calls } = await runAction({ ...VALID, username: '1bad name' }, async () => {
			throw new Error('the API must not be called');
		});
		const data = (result as { data: { errors: { field: string }[] } }).data;
		expect(data.errors.some((e) => e.field === 'username')).toBe(true);
		expect(calls).toHaveLength(0);
	});

	it('rejects a password under the twelve-character floor locally', async () => {
		const { result, calls } = await runAction({ ...VALID, password: 'short' }, async () => {
			throw new Error('the API must not be called');
		});
		const data = (result as { data: { errors: { field: string }[] } }).data;
		expect(data.errors.some((e) => e.field === 'password')).toBe(true);
		expect(calls).toHaveLength(0);
	});

	it('surfaces the retry window on a rate limit', async () => {
		const { result } = await runAction(VALID, async () =>
			apiError(429, 'RATE_LIMITED', { 'retry-after': '90' })
		);
		const { status, data } = result as {
			status: number;
			data: { errors: { message: string }[] };
		};
		expect(status).toBe(429);
		expect(data.errors[0]!.message).toContain('90');
	});

	it('explains an outage without retrying the mutation', async () => {
		let attempts = 0;
		const { result, calls } = await runAction(VALID, async () => {
			attempts += 1;
			throw new TypeError('connection refused');
		});
		expect(attempts).toBe(1);
		expect(calls).toHaveLength(1);
		expect((result as { status: number }).status).toBe(503);
	});
});
