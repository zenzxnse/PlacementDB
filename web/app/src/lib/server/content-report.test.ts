import { beforeEach, describe, expect, it, vi } from 'vitest';

const env: Record<string, string> = {};
vi.mock('$env/dynamic/private', () => ({ env }));
vi.mock('$app/environment', () => ({ building: false, dev: false }));

function cookies() {
	return { get: () => undefined, getAll: () => [], set: () => {}, delete: () => {}, serialize: () => '' };
}

beforeEach(() => {
	env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
	env.PLACEDB_PUBLIC_ORIGIN = 'https://placedb.example';
	vi.resetModules();
});

describe('content reporting', () => {
	it('posts the server-owned target shape with Origin and CSRF', async () => {
		const calls: Array<{ url: string; init: RequestInit }> = [];
		const { reportContent } = await import('./comment-actions');
		const result = await reportContent({
			cookies: cookies(),
			request: new Request('https://placedb.example/questions/x'),
			url: new URL('https://placedb.example/questions/x'),
			fetch: async (input: URL | RequestInfo, init?: RequestInit) => {
				calls.push({ url: String(input), init: init ?? {} });
				return new Response(JSON.stringify({ public_id: 'r', state: 'open', created_at: 'x' }), { status: 201, headers: { 'content-type': 'application/json' } });
			}
		}, { csrf: 'token', targetType: 'question', publicId: 'q-id', reason: 'incorrect', details: '' });
		expect(result.kind).toBe('reported');
		expect(calls[0]?.url).toBe('http://api.internal/api/v1/reports');
		expect(JSON.parse(String(calls[0]?.init.body))).toEqual({ target_type: 'question', public_id: 'q-id', reason: 'incorrect' });
		expect((calls[0]?.init.headers as Record<string, string>)['x-csrf-token']).toBe('token');
		expect((calls[0]?.init.headers as Record<string, string>).origin).toBe('https://placedb.example');
	});

	it('refuses an invalid reason before calling upstream', async () => {
		const fetch = vi.fn(async (_input: URL | RequestInfo, _init?: RequestInit) => new Response());
		const { reportContent } = await import('./comment-actions');
		const result = await reportContent({ cookies: cookies(), request: new Request('https://placedb.example/questions/x'), url: new URL('https://placedb.example/questions/x'), fetch },
			{ csrf: 'token', targetType: 'question', publicId: 'q-id', reason: 'dislike', details: '' });
		expect(result.kind).toBe('error');
		expect(fetch).not.toHaveBeenCalled();
	});
});
