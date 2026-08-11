import { describe, expect, it, vi, beforeEach } from 'vitest';

/**
 * Submission loaders and actions, against the mocked API.
 *
 * Mutation-checked: dropping the anonymous redirect in loadSubmitPage fails
 * the redirect test; returning the draft on success instead of redirecting
 * fails the repost test; retrying the POST fails the no-retry test.
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

const ME = {
	public_id: 'u-1',
	username: 'student_001',
	display_name: 'Student One',
	role: 'user',
	status: 'active',
	can_submit: true,
	can_moderate: false,
	unread_moderation_count: 0
};

const OPTIONS = {
	companies: [{ slug: 'acme', name: 'Acme' }],
	roles: [{ slug: 'software-engineer', name: 'Software engineer' }],
	topics: [{ slug: 'arrays', name: 'Arrays' }],
	years: [2026],
	rounds: ['coding'],
	outcomes: ['offered']
};

function json(body: unknown, status = 200, headers: Record<string, string> = {}) {
	return new Response(JSON.stringify(body), {
		status,
		headers: { 'content-type': 'application/json', ...headers }
	});
}

/** Answers the three GETs a submission page load makes. */
function routeGet(url: string, me: unknown): Response {
	if (url.endsWith('/me')) return json(me);
	if (url.endsWith('/auth/csrf')) return json({ csrf_token: 'token-abc' });
	if (url.endsWith('/meta/filter-options')) return json(OPTIONS);
	throw new Error(`unexpected GET ${url}`);
}

function loadEvent(fetchImpl: (url: string, init: RequestInit) => Promise<Response>) {
	return {
		cookies: fakeCookies(),
		url: new URL('http://localhost/submit/question'),
		request: new Request('http://localhost/submit/question'),
		fetch: fetchImpl
	};
}

/** Runs something that may redirect. Returns the thrown redirect, or undefined. */
async function catchRedirect(run: () => Promise<unknown>) {
	try {
		await run();
		return undefined;
	} catch (thrown) {
		const redirect = thrown as { status: number; location: string };
		if (typeof redirect?.status === 'number' && typeof redirect?.location === 'string') {
			return redirect;
		}
		throw thrown;
	}
}

beforeEach(() => {
	for (const k of Object.keys(env)) delete env[k];
	env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
	env.PLACEDB_PUBLIC_ORIGIN = 'https://placedb.example';
	vi.resetModules();
});

describe('loading a submission form', () => {
	it('sends an anonymous visitor to log in and back again', async () => {
		const { load } = await import('./question/+page.server');
		const redirect = await catchRedirect(() =>
			/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
			(load as any)(loadEvent(async (url) => routeGet(url, null)))
		);
		expect(redirect).toEqual({
			status: 303,
			location: '/login?next=%2Fsubmit%2Fquestion'
		});
	});

	it('returns the token and vocabulary lists for a signed-in submitter', async () => {
		const { load } = await import('./question/+page.server');
		/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
		const data = await (load as any)(loadEvent(async (url) => routeGet(url, ME)));
		expect(data.me.username).toBe('student_001');
		expect(data.csrfToken).toBe('token-abc');
		expect(data.options.topics).toHaveLength(1);
		expect(data.optionsError).toBeNull();
	});

	it('still renders the form when the vocabulary lists are unavailable', async () => {
		const { load } = await import('./question/+page.server');
		/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
		const data = await (load as any)(
			loadEvent(async (url) => {
				if (url.endsWith('/meta/filter-options')) return json({ error: {} }, 503);
				return routeGet(url, ME);
			})
		);
		/* Company, role, and topics are optional, so a form is still useful. */
		expect(data.csrfToken).toBe('token-abc');
		expect(data.options).toBeNull();
		expect(data.optionsError).not.toBeNull();
	});

	it('keeps a token failure from blocking the page', async () => {
		const { load } = await import('./question/+page.server');
		/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
		const data = await (load as any)(
			loadEvent(async (url) => {
				if (url.endsWith('/auth/csrf')) return json({ error: {} }, 503);
				return routeGet(url, ME);
			})
		);
		/* Empty token disables the button and the action refuses the post. */
		expect(data.csrfToken).toBe('');
	});
});

async function postQuestion(
	fields: Record<string, string>,
	respond: () => Promise<Response> = async () =>
		json(
			{ public_id: 'q-1', slug: 'a-title', state: 'pending_review', updated_at: '2026-08-11T00:00:00Z' },
			201
		)
) {
	const { actions } = await import('./question/+page.server');
	const calls: Call[] = [];
	const event = {
		cookies: fakeCookies(),
		url: new URL('http://localhost/submit/question'),
		request: new Request('http://localhost/submit/question', {
			method: 'POST',
			body: new URLSearchParams(fields),
			headers: { 'content-type': 'application/x-www-form-urlencoded' }
		}),
		fetch: (url: string, init: RequestInit) => {
			calls.push({ url, init });
			return respond();
		}
	};
	let result: unknown;
	const redirect = await catchRedirect(async () => {
		/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
		result = await (actions.default as any)(event);
	});
	return { redirect, result, calls };
}

const VALID_QUESTION = {
	_csrf: 'token-abc',
	title: 'Detect a cycle',
	prompt: 'Given a singly linked list, detect whether it contains a cycle.'
};

describe('posting a question draft', () => {
	it('posts to /questions and redirects so a reload cannot repost', async () => {
		const { redirect, calls } = await postQuestion(VALID_QUESTION);
		expect(redirect).toEqual({ status: 303, location: '/submit/question?submitted=1' });
		expect(calls[0]!.url).toBe('http://api.internal/api/v1/questions');
		expect(calls[0]!.init.method).toBe('POST');
		const headers = calls[0]!.init.headers as Record<string, string>;
		expect(headers['x-csrf-token']).toBe('token-abc');
		expect(headers.origin).toBe('https://placedb.example');
		const body = JSON.parse(String(calls[0]!.init.body));
		expect(body.title).toBe('Detect a cycle');
		/* The state is the server's to set; the client never proposes one. */
		expect(body.state).toBeUndefined();
	});

	it('refuses without a CSRF token and never calls the API', async () => {
		const { result, calls } = await postQuestion({ ...VALID_QUESTION, _csrf: '' });
		expect((result as { status: number }).status).toBe(403);
		expect(calls).toHaveLength(0);
	});

	it('returns the typed draft after a validation failure so nothing is lost', async () => {
		const { result, calls } = await postQuestion({ ...VALID_QUESTION, prompt: 'too short' });
		const { status, data } = result as {
			status: number;
			data: { draft: { title: string }; errors: { field: string }[] };
		};
		expect(status).toBe(400);
		expect(data.draft.title).toBe('Detect a cycle');
		expect(data.errors.some((e) => e.field === 'prompt')).toBe(true);
		expect(calls).toHaveLength(0);
	});

	it('does not retry a submission that may already have been accepted', async () => {
		let attempts = 0;
		const { result, calls } = await postQuestion(VALID_QUESTION, async () => {
			attempts += 1;
			throw new TypeError('connection reset');
		});
		expect(attempts).toBe(1);
		expect(calls).toHaveLength(1);
		const { status, data } = result as { status: number; data: { draft: { title: string } } };
		expect(status).toBe(503);
		/* The text survives the outage, so the author can try again. */
		expect(data.draft.title).toBe('Detect a cycle');
	});

	it('maps API field errors onto the matching fields', async () => {
		const { result } = await postQuestion(VALID_QUESTION, async () =>
			json(
				{
					error: {
						code: 'VALIDATION_FAILED',
						message: 'Check the form.',
						request_id: 'req-3',
						details: {
							fields: [{ field: 'topic_slugs', code: 'UNKNOWN_SLUG', message: 'Unknown topic.' }]
						}
					}
				},
				400
			)
		);
		const data = (result as { data: { errors: { field: string; message: string }[] } }).data;
		expect(data.errors).toEqual([{ field: 'topic_slugs', message: 'Unknown topic.' }]);
	});
});

describe('posting an experience draft', () => {
	it('sends rounds in order and omits a hidden outcome', async () => {
		const { actions } = await import('./experience/+page.server');
		const calls: Call[] = [];
		const fields = new URLSearchParams();
		fields.append('_csrf', 'token-abc');
		fields.append('title', 'Acme interview');
		fields.append('narrative', 'The process began with an online assessment and ran three weeks.');
		fields.append('round', 'online_assessment');
		fields.append('round_notes', 'Two coding questions.');
		fields.append('round', '');
		fields.append('round_notes', '');
		fields.append('round', 'hr');
		fields.append('round_notes', '');
		const event = {
			cookies: fakeCookies(),
			url: new URL('http://localhost/submit/experience'),
			request: new Request('http://localhost/submit/experience', {
				method: 'POST',
				body: fields,
				headers: { 'content-type': 'application/x-www-form-urlencoded' }
			}),
			fetch: (url: string, init: RequestInit) => {
				calls.push({ url, init });
				return Promise.resolve(
					json(
						{
							public_id: 'e-1',
							slug: 'acme-interview',
							state: 'pending_review',
							updated_at: '2026-08-11T00:00:00Z'
						},
						201
					)
				);
			}
		};
		/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
		const redirect = await catchRedirect(async () => (actions.default as any)(event));
		expect(redirect).toEqual({ status: 303, location: '/submit/experience?submitted=1' });
		const body = JSON.parse(String(calls[0]!.init.body));
		expect(body.rounds).toEqual([
			{ round: 'online_assessment', notes: 'Two coding questions.' },
			{ round: 'hr', notes: null }
		]);
		expect(body.outcome_visible).toBe(false);
		expect('outcome' in body).toBe(false);
	});
});
