import { describe, expect, it, vi, beforeEach } from 'vitest';
import {
	WireError,
	parseCommentPage,
	parseCsrf,
	parseDifficultyVote,
	parseExperience,
	parseFilterOptions,
	parseLookupCountPage,
	parseMe,
	parseQuestion,
	parseQuestionPage
} from './wire';

describe('lookup count wire contract', () => {
	it('accepts bounded integer count rows and rejects malformed counts', () => {
		const parsed = parseLookupCountPage({
			items: [{ slug: 'arrays', name: 'Arrays', question_count: 12, experience_count: 0 }]
		});
		expect(parsed.items).toHaveLength(1);
		expect(parsed.items.at(0)?.question_count).toBe(12);
		expect(() =>
			parseLookupCountPage({
				items: [{ slug: 'arrays', name: 'Arrays', question_count: 'many', experience_count: 0 }]
			})
		).toThrow(WireError);
	});
});

/* Hoisted: vi.mock runs before anything else in the module, so it lives here. */
const env: Record<string, string> = {};
vi.mock('$env/dynamic/private', () => ({ env }));
vi.mock('$app/environment', () => ({ building: false, dev: false }));

/**
 * Runtime validation at the API boundary.
 *
 * Two things are being proven. First, that a response which does not match the
 * wire type is rejected at the seam instead of flowing on as a value the app
 * believes. Second, that the rejection is not over-eager: unknown additive
 * fields are the normal way this contract evolves and must keep working.
 */

const VALID_QUESTION = {
	public_id: 'q-1',
	slug: 'detect-a-cycle',
	title: 'Detect a cycle',
	company: { slug: 'acme', name: 'Acme' },
	role: null,
	round: 'coding',
	source_year: 2026,
	topics: [{ slug: 'arrays', name: 'Arrays' }],
	difficulty: { mean: 3.4, vote_count: 8 },
	published_at: '2026-01-01T00:00:00Z',
	prompt: 'Given a singly linked list, detect whether it contains a cycle.',
	answer_guidance: null,
	author: { username: 'student_001', display_name: 'Student One' }
};

const VALID_EXPERIENCE = {
	public_id: 'e-1',
	slug: 'acme-interview',
	title: 'Acme interview',
	company: null,
	role: null,
	source_year: 2026,
	outcome_visible: false,
	author: null,
	published_at: '2026-01-01T00:00:00Z',
	narrative: 'It went like this.',
	rounds: [{ ordinal: 1, round: 'hr', notes: null }]
};

describe('accepting good responses', () => {
	it('parses a complete question', () => {
		const question = parseQuestion(VALID_QUESTION);
		expect(question.slug).toBe('detect-a-cycle');
		expect(question.difficulty.mean).toBe(3.4);
		expect(question.author?.username).toBe('student_001');
	});

	it('keeps working when the backend adds a field', () => {
		/*
		 * The wire contract is additively versioned. Rejecting unknown fields
		 * would turn every backend addition into a site outage, so only
		 * declared fields are checked.
		 */
		const question = parseQuestion({ ...VALID_QUESTION, moderation_note: 'new field' });
		expect(question.title).toBe('Detect a cycle');
	});

	it('treats a missing optional list as empty rather than failing', () => {
		const { topics, ...withoutTopics } = VALID_QUESTION;
		void topics;
		expect(parseQuestion(withoutTopics).topics).toEqual([]);
	});

	it('parses an anonymous /me as null', () => {
		expect(parseMe(null)).toBeNull();
	});
});

describe('rejecting bad responses', () => {
	it('rejects a null difficulty mean, which the prior makes impossible', () => {
		/*
		 * Zero votes scores exactly 3.0. A null here is an un-updated backend,
		 * and letting it through would render an unrated question as whatever
		 * null coerces to.
		 */
		expect(() =>
			parseQuestion({ ...VALID_QUESTION, difficulty: { mean: null, vote_count: 0 } })
		).toThrow(WireError);
	});

	it('rejects a non-finite number', () => {
		/* NaN is not JSON, but a lenient upstream parser can still produce it. */
		expect(() =>
			parseQuestion({ ...VALID_QUESTION, difficulty: { mean: NaN, vote_count: 0 } })
		).toThrow(WireError);
	});

	it('rejects a round outside the accepted ten-value set', () => {
		expect(() => parseQuestion({ ...VALID_QUESTION, round: 'whiteboard' })).toThrow(WireError);
	});

	it('rejects a string where a number belongs', () => {
		expect(() => parseQuestion({ ...VALID_QUESTION, source_year: '2026' })).toThrow(WireError);
	});

	it('names the path without quoting the value', () => {
		try {
			parseQuestion({ ...VALID_QUESTION, difficulty: { mean: 'high', vote_count: 0 } });
			expect.unreachable('should have thrown');
		} catch (error) {
			expect(error).toBeInstanceOf(WireError);
			const wire = error as WireError;
			expect(wire.path).toBe('question.difficulty.mean');
			/* The offending value never appears; a body can hold other people's text. */
			expect(wire.message).not.toContain('high');
		}
	});

	it('rejects a page whose items are not a list', () => {
		expect(() =>
			parseQuestionPage({
				items: 'nope',
				page: 1,
				per_page: 20,
				total: 0,
				total_is_estimate: false,
				total_pages: 1,
				next_cursor: null
			})
		).toThrow(WireError);
	});

	it('rejects an empty CSRF token, which every form treats as no token', () => {
		expect(() => parseCsrf({ csrf_token: '' })).toThrow(WireError);
	});

	it('rejects a vote outside the 1 to 5 scale', () => {
		expect(() =>
			parseDifficultyVote({ difficulty: { mean: 3, vote_count: 1 }, my_vote: 9 })
		).toThrow(WireError);
	});

	it('rejects a /me without its capability booleans', () => {
		/*
		 * Defaulting them would either hide controls from people who have the
		 * capability or offer controls the API then refuses. Failing is right.
		 */
		const { can_submit, ...rest } = {
			public_id: 'u-1',
			username: 'student_001',
			display_name: 'Student One',
			role: 'user',
			status: 'active',
			can_submit: true,
			can_moderate: false,
			unread_moderation_count: 0
		};
		void can_submit;
		expect(() => parseMe(rest)).toThrow(WireError);
	});

	it('rejects an unknown role name', () => {
		expect(() =>
			parseMe({
				public_id: 'u-1',
				username: 'a',
				display_name: 'A',
				role: 'superuser',
				status: 'active',
				can_submit: true,
				can_moderate: true,
				unread_moderation_count: 0
			})
		).toThrow(WireError);
	});

	it('rejects filter options carrying a retired difficulty label', () => {
		expect(() =>
			parseFilterOptions({
				companies: [],
				roles: [],
				topics: [],
				years: [],
				rounds: ['easy'],
				outcomes: []
			})
		).toThrow(WireError);
	});

	it('rejects a comment page whose items are not comments', () => {
		expect(() => parseCommentPage({ items: [{ body: 'hi' }], next_cursor: null })).toThrow(
			WireError
		);
	});
});

describe('the outcome visibility rule', () => {
	it('drops an outcome that arrives with visibility false', () => {
		/*
		 * A backend bug or a stale cache could send both. Keeping the value
		 * would render something the author withheld, so the parser removes it
		 * rather than trusting the sender.
		 */
		const experience = parseExperience({ ...VALID_EXPERIENCE, outcome: 'offered' });
		expect(experience.outcome_visible).toBe(false);
		expect('outcome' in experience).toBe(false);
	});

	it('requires an outcome when visibility is true', () => {
		expect(() => parseExperience({ ...VALID_EXPERIENCE, outcome_visible: true })).toThrow(
			WireError
		);
	});

	it('keeps a visible outcome', () => {
		const experience = parseExperience({
			...VALID_EXPERIENCE,
			outcome_visible: true,
			outcome: 'withdrew'
		});
		expect(experience.outcome_visible).toBe(true);
		expect(experience.outcome_visible && experience.outcome).toBe('withdrew');
	});
});

/**
 * The seam itself: a mismatch must arrive as ApiMalformed, the same honest
 * failure a non-JSON body already takes, and must not leak the response body
 * into anything a page can render.
 */
describe('the API client turns a wire mismatch into ApiMalformed', () => {
	beforeEach(() => {
		for (const k of Object.keys(env)) delete env[k];
		env.PLACEDB_API_BASE = 'http://api.internal/api/v1';
		vi.resetModules();
	});

	function makeEvent(body: unknown) {
		return {
			cookies: {
				get: () => undefined,
				set: () => undefined,
				delete: () => undefined
			},
			request: new Request('http://localhost/'),
			fetch: async () =>
				new Response(JSON.stringify(body), {
					status: 200,
					headers: { 'content-type': 'application/json' }
				})
		};
	}

	it('raises ApiMalformed when a 200 body does not match the wire type', async () => {
		const { apiCall, ApiMalformed } = await import('./server/api');
		await expect(
			apiCall(makeEvent({ ...VALID_QUESTION, difficulty: null }) as never, '/questions/x', {
				parse: parseQuestion
			})
		).rejects.toThrow(ApiMalformed);
	});

	it('classifies that failure as malformed, not as validation or success', async () => {
		const { apiCall, toFailure } = await import('./server/api');
		try {
			await apiCall(makeEvent({ nonsense: true }) as never, '/me', { parse: parseMe });
			expect.unreachable('should have thrown');
		} catch (error) {
			const failure = toFailure(error);
			expect(failure.kind).toBe('malformed');
			/* Nothing from the body reaches the message a page renders. */
			expect(failure.message).not.toContain('nonsense');
		}
	});

	it('still accepts a matching response', async () => {
		const { apiCall } = await import('./server/api');
		const question = await apiCall(makeEvent(VALID_QUESTION) as never, '/questions/x', {
			parse: parseQuestion
		});
		expect(question.title).toBe('Detect a cycle');
	});
});
