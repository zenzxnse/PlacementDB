import { describe, expect, it, vi, beforeEach } from 'vitest';
import { companyName, roundLabel, splitParagraphs, visibleOutcome } from '$lib/format';

/**
 * Contract behavior that is not about transport: what the API layer returns
 * for /me, what production refuses to do with fixtures, and how the accepted
 * nullable and visibility rules render.
 */

const env: Record<string, string> = {};
vi.mock('$env/dynamic/private', () => ({ env }));

function fakeCookies() {
	const jar = new Map<string, { value: string; options: Record<string, unknown> }>();
	return {
		jar,
		get: (n: string) => jar.get(n)?.value,
		set: (n: string, v: string, o: Record<string, unknown>) => jar.set(n, { value: v, options: o }),
		delete: (n: string) => jar.delete(n)
	};
}

function makeEvent(fetchImpl: () => Promise<Response>) {
	return {
		cookies: fakeCookies(),
		request: new Request('http://localhost/'),
		fetch: fetchImpl
	};
}

function json(body: unknown, status = 200) {
	return new Response(JSON.stringify(body), {
		status,
		headers: { 'content-type': 'application/json' }
	});
}

beforeEach(() => {
	for (const k of Object.keys(env)) delete env[k];
	env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
	vi.resetModules();
});

describe('production fixture refusal', () => {
	it('refuses to start when fixtures are enabled outside development', async () => {
		vi.doMock('$app/environment', () => ({ building: false, dev: false }));
		env.PLACEDB_USE_FIXTURES = 'true';
		const { assertConfigured } = await import('./config');
		expect(() => assertConfigured()).toThrow(/Refusing to serve synthetic data/);
	});

	it('does not enable fixtures in production even when the flag is set', async () => {
		vi.doMock('$app/environment', () => ({ building: false, dev: false }));
		env.PLACEDB_USE_FIXTURES = 'true';
		const { USE_FIXTURES } = await import('./config');
		expect(USE_FIXTURES).toBe(false);
	});

	it('rejects a non-absolute API base', async () => {
		vi.doMock('$app/environment', () => ({ building: false, dev: false }));
		env.PLACEDB_API_BASE = '/api/v1';
		const { assertConfigured } = await import('./config');
		expect(() => assertConfigured()).toThrow(/absolute http or https URL/);
	});
});

describe('/me', () => {
	it('returns the user when the API supplies one', async () => {
		vi.doMock('$app/environment', () => ({ building: false, dev: false }));
		const { getMe } = await import('./content');
		const me = await getMe(
			makeEvent(async () =>
				json({
					public_id: 'u1',
					username: 'student_001',
					display_name: 'Student One',
					role: 'user',
					status: 'active',
					can_submit: true,
					can_moderate: false,
					unread_moderation_count: 0
				})
			) as never
		);
		expect(me?.username).toBe('student_001');
		expect(me?.can_moderate).toBe(false);
	});

	it('treats an outage as anonymous rather than failing the page', async () => {
		vi.doMock('$app/environment', () => ({ building: false, dev: false }));
		const { getMe } = await import('./content');
		const me = await getMe(
			makeEvent(async () => {
				throw new Error('ECONNREFUSED');
			}) as never
		);
		/* Authorization lives in the API, so an anonymous header grants nothing. */
		expect(me).toBeNull();
	});

	it('treats 401 as anonymous', async () => {
		vi.doMock('$app/environment', () => ({ building: false, dev: false }));
		const { getMe } = await import('./content');
		const me = await getMe(makeEvent(async () => json({ error: {} }, 401)) as never);
		expect(me).toBeNull();
	});
});

describe('search degradation', () => {
	it('degrades to unavailable rather than serving anything stale', async () => {
		vi.doMock('$app/environment', () => ({ building: false, dev: false }));
		const { search } = await import('./content');
		const outcome = await search(
			makeEvent(async () =>
				json({ error: { code: 'SEARCH_UNAVAILABLE', message: 'down', request_id: 'r' } }, 503)
			) as never,
			{ q: 'hash', page: 1, filters: { company: [], role: [], topic: [], year: [], difficulty: [] } }
		);
		expect(outcome.status).toBe('unavailable');
	});
});

describe('accepted rendering rules', () => {
	it('omits a hidden outcome entirely instead of labelling it', () => {
		/*
		 * The critical privacy rule. Returning any string here, including
		 * "Unknown" or "Hidden", tells a reader something about the placement
		 * the author chose to withhold.
		 */
		expect(visibleOutcome({ outcome_visible: false })).toBeNull();
		/*
		 * The old form of this test also passed `{ outcome_visible: false,
		 * outcome: 'offered' }`, which the union now makes a compile error: the
		 * hidden branch has no `outcome` to read. The runtime half of that
		 * guarantee moved to the parser test, which proves an `outcome`
		 * arriving from a misbehaving backend alongside visibility false is
		 * dropped rather than rendered.
		 */
	});

	it('shows a visible outcome', () => {
		expect(visibleOutcome({ outcome_visible: true, outcome: 'offered' })).toBe('Offered');
	});

	it('labels an explicitly unknown outcome, which is a real choice', () => {
		/* Distinct from hidden: the author said so rather than withholding it. */
		expect(visibleOutcome({ outcome_visible: true, outcome: 'unknown' })).toBe('Unknown');
	});

	it('labels all ten accepted rounds and tolerates null', () => {
		const rounds = [
			'online_assessment',
			'aptitude',
			'coding',
			'technical',
			'system_design',
			'behavioral',
			'managerial',
			'group_discussion',
			'hr',
			'other'
		] as const;
		for (const round of rounds) {
			expect(roundLabel(round)).not.toBe('');
			expect(roundLabel(round)).not.toBe('Not recorded');
		}
		expect(roundLabel(null)).toBe('Not recorded');
	});

	it('renders a null company without throwing', () => {
		expect(companyName(null)).toBe('Not recorded');
		expect(companyName({ name: 'Acme Corp' })).toBe('Acme Corp');
	});

	it('splits a narrative on blank lines only, preserving single newlines', () => {
		const narrative = 'First para line one.\nStill first para.\n\nSecond para.';
		const parts = splitParagraphs(narrative);
		expect(parts).toHaveLength(2);
		expect(parts[0]).toContain('Still first para.');
		expect(parts[1]).toBe('Second para.');
	});

	it('returns a single paragraph when there is no blank line', () => {
		expect(splitParagraphs('One line only.')).toEqual(['One line only.']);
	});
});
