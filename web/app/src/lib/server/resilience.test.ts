import { describe, expect, it, vi, beforeEach } from 'vitest';
import {
	escapeLinkFor,
	kindForStatus,
	parseRetryAfter,
	safeRetryTarget,
	type FailureKind
} from '$lib/failure';

/**
 * Resilience behavior: classification, bounded retry, malformed responses, and
 * the guarantee that no production failure path serves fixtures.
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

function makeEvent(fetchImpl: (url: string, init: RequestInit) => Promise<Response>) {
	const calls: { url: string; init: RequestInit }[] = [];
	return {
		calls,
		event: {
			cookies: fakeCookies(),
			request: new Request('http://localhost/'),
			fetch: (url: string, init: RequestInit) => {
				calls.push({ url, init });
				return fetchImpl(url, init);
			}
		}
	};
}

function apiError(status: number, code: string, extra: Record<string, string> = {}) {
	return new Response(
		JSON.stringify({ error: { code, message: 'Safe message.', request_id: 'req-9' } }),
		{ status, headers: { 'content-type': 'application/json', ...extra } }
	);
}

beforeEach(() => {
	for (const k of Object.keys(env)) delete env[k];
	env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
	env.PLACEDB_PUBLIC_ORIGIN = 'https://placedb.example';
	vi.resetModules();
});

describe('status classification', () => {
	const cases: [number, FailureKind][] = [
		[400, 'validation'],
		[401, 'session_expired'],
		[403, 'forbidden'],
		[404, 'not_found'],
		[409, 'conflict'],
		[413, 'too_large'],
		[415, 'unsupported_media'],
		[429, 'rate_limited'],
		[500, 'dependency'],
		[503, 'dependency']
	];
	for (const [status, kind] of cases) {
		it(`maps ${status} to ${kind}`, () => {
			expect(kindForStatus(status)).toBe(kind);
		});
	}

	it('treats an unrecognised status as a service problem, not user error', () => {
		/* Guessing "validation" would tell people to edit input that was fine. */
		expect(kindForStatus(418)).toBe('dependency');
	});
});

describe('failure detail preservation', () => {
	it('keeps the safe message and request ID, and never a raw body', async () => {
		const { apiCall, toFailure } = await import('./api');
		const { event } = makeEvent(async () => apiError(403, 'FORBIDDEN'));
		try {
			await apiCall(event as never, '/moderation/queue');
			expect.unreachable();
		} catch (error) {
			const failure = toFailure(error);
			expect(failure.kind).toBe('forbidden');
			expect(failure.message).toBe('Safe message.');
			expect(failure.requestId).toBe('req-9');
			expect(failure.serviceProblem).toBe(false);
		}
	});

	it('carries Retry-After guidance on 429', async () => {
		const { apiCall, toFailure } = await import('./api');
		const { event } = makeEvent(async () =>
			apiError(429, 'RATE_LIMITED', { 'retry-after': '30' })
		);
		try {
			await apiCall(event as never, '/questions');
			expect.unreachable();
		} catch (error) {
			const failure = toFailure(error);
			expect(failure.kind).toBe('rate_limited');
			expect(failure.retryAfterSeconds).toBe(30);
		}
	});

	it('ignores a nonsense Retry-After rather than trusting it', () => {
		expect(parseRetryAfter('not-a-number')).toBeUndefined();
		expect(parseRetryAfter('-5')).toBeUndefined();
		expect(parseRetryAfter('99999')).toBe(3600);
	});
});

describe('malformed responses are never success', () => {
	it('rejects a 200 that is not JSON', async () => {
		const { apiCall, ApiMalformed } = await import('./api');
		const { event } = makeEvent(
			async () =>
				new Response('<html>gateway error</html>', {
					status: 200,
					headers: { 'content-type': 'text/html' }
				})
		);
		await expect(apiCall(event as never, '/questions')).rejects.toThrow(ApiMalformed);
	});

	it('rejects a 200 with a JSON content type but broken body', async () => {
		const { apiCall, ApiMalformed } = await import('./api');
		const { event } = makeEvent(
			async () =>
				new Response('{"items": [', {
					status: 200,
					headers: { 'content-type': 'application/json' }
				})
		);
		await expect(apiCall(event as never, '/questions')).rejects.toThrow(ApiMalformed);
	});

	it('classifies malformed as a service problem, not user error', async () => {
		const { apiCall, toFailure } = await import('./api');
		const { event } = makeEvent(
			async () => new Response('nope', { status: 200, headers: { 'content-type': 'text/plain' } })
		);
		try {
			await apiCall(event as never, '/questions');
			expect.unreachable();
		} catch (error) {
			const failure = toFailure(error);
			expect(failure.kind).toBe('malformed');
			expect(failure.serviceProblem).toBe(true);
			/* The internal detail must not reach the user-facing message. */
			expect(failure.message).not.toContain('nope');
		}
	});
});

describe('bounded retry', () => {
	it('retries an idempotent GET once on 503', async () => {
		const { apiCall } = await import('./api');
		let attempts = 0;
		const { event } = makeEvent(async () => {
			attempts += 1;
			if (attempts === 1) return apiError(503, 'SERVICE_UNAVAILABLE');
			return new Response(JSON.stringify({ ok: true }), {
				status: 200,
				headers: { 'content-type': 'application/json' }
			});
		});
		await apiCall(event as never, '/questions');
		expect(attempts).toBe(2);
	});

	it('never retries a mutation, even on 503', async () => {
		const { apiCall } = await import('./api');
		let attempts = 0;
		const { event } = makeEvent(async () => {
			attempts += 1;
			return apiError(503, 'SERVICE_UNAVAILABLE');
		});
		await expect(apiCall(event as never, '/auth/login', { method: 'POST' })).rejects.toThrow();
		/*
		 * A mutation that timed out may already have applied. Repeating a vote,
		 * submission, or moderation action could double-apply it.
		 */
		expect(attempts).toBe(1);
	});

	it('retries at most once, never in a loop', async () => {
		const { apiCall } = await import('./api');
		let attempts = 0;
		const { event } = makeEvent(async () => {
			attempts += 1;
			return apiError(503, 'SERVICE_UNAVAILABLE');
		});
		await expect(apiCall(event as never, '/questions')).rejects.toThrow();
		expect(attempts).toBe(2);
	});

	it('does not retry a 404, which will not change', async () => {
		const { apiCall } = await import('./api');
		let attempts = 0;
		const { event } = makeEvent(async () => {
			attempts += 1;
			return apiError(404, 'NOT_FOUND');
		});
		await expect(apiCall(event as never, '/questions/by-slug/nope')).rejects.toThrow();
		expect(attempts).toBe(1);
	});
});

describe('safe navigation targets', () => {
	it('preserves filters and query on a retry link', () => {
		const url = new URL('https://placedb.example/questions?sort=new&topic=arrays&page=3');
		expect(safeRetryTarget(url)).toBe('/questions?sort=new&topic=arrays&page=3');
	});

	it('refuses a protocol-relative path that could leave the site', () => {
		const url = new URL('https://placedb.example//evil.example/path');
		expect(safeRetryTarget(url)).toBe('/');
	});

	it('offers a relevant escape link per section', () => {
		expect(escapeLinkFor('/questions/abc').href).toBe('/questions');
		expect(escapeLinkFor('/experiences/abc').href).toBe('/experiences');
		expect(escapeLinkFor('/search').href).toBe('/questions');
		expect(escapeLinkFor('/anything-else').href).toBe('/');
	});
});

describe('no failure path serves fixtures in production', () => {
	it('propagates the failure instead of substituting fixture rows', async () => {
		const { listQuestions } = await import('./content');
		const { event } = makeEvent(async () => {
			throw new Error('ECONNREFUSED');
		});
		/*
		 * The guarantee that matters: an outage surfaces as a failure, never as
		 * synthetic placement content that looks real.
		 */
		await expect(
			listQuestions(event as never, {
				sort: 'hot',
				page: 1,
				filters: { company: [], role: [], topic: [], year: [], difficulty: [] }
			})
		).rejects.toThrow();
	});

	it('does not turn a 403 into an empty list', async () => {
		const { listQuestions } = await import('./content');
		const { event } = makeEvent(async () => apiError(403, 'FORBIDDEN'));
		await expect(
			listQuestions(event as never, {
				sort: 'hot',
				page: 1,
				filters: { company: [], role: [], topic: [], year: [], difficulty: [] }
			})
		).rejects.toThrow();
	});
});
