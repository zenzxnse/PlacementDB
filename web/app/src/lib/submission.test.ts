import { describe, expect, it } from 'vitest';
import { buildExperienceDraft, buildQuestionDraft, yearMax } from './submission';

/**
 * Draft construction from form data.
 *
 * These are pure functions, so the tests are about shape rather than transport:
 * what reaches the API, what is dropped, and the two rules that carry a privacy
 * consequence — a withheld outcome is omitted rather than nulled, and ordinals
 * are never sent by the client.
 *
 * Mutation-checked: sending `outcome: null` when hidden fails the omission
 * test; keeping blank round slots fails the slot test; emitting `ordinal` fails
 * the ordinal test.
 */

function form(entries: [string, string][]) {
	const data = new URLSearchParams();
	for (const [k, v] of entries) data.append(k, v);
	return {
		get: (name: string) => data.get(name),
		getAll: (name: string) => data.getAll(name)
	};
}

const LONG_PROMPT = 'Given a singly linked list, detect whether it contains a cycle.';
const LONG_NARRATIVE = 'The process began with an online assessment and ran for three weeks.';

describe('question drafts', () => {
	it('sends the editable fields and nothing the server derives', () => {
		const { draft, errors } = buildQuestionDraft(
			form([
				['title', 'Detect a cycle'],
				['prompt', LONG_PROMPT],
				['company_slug', 'acme'],
				['job_role_slug', 'software-engineer'],
				['round', 'coding'],
				['source_year', '2026'],
				['topic_slugs', 'linked-lists'],
				['topic_slugs', 'algorithms']
			])
		);
		expect(errors).toEqual([]);
		expect(draft).toEqual({
			title: 'Detect a cycle',
			prompt: LONG_PROMPT,
			answer_guidance: null,
			company_slug: 'acme',
			job_role_slug: 'software-engineer',
			round: 'coding',
			source_year: 2026,
			topic_slugs: ['linked-lists', 'algorithms']
		});
		/* Author, slug, state, IDs, and timestamps are the server's. */
		expect(Object.keys(draft)).not.toContain('state');
		expect(Object.keys(draft)).not.toContain('author');
	});

	it('turns blank optional fields into null rather than empty strings', () => {
		const { draft } = buildQuestionDraft(
			form([
				['title', 'A title'],
				['prompt', LONG_PROMPT],
				['company_slug', ''],
				['job_role_slug', ''],
				['round', ''],
				['source_year', ''],
				['answer_guidance', '   ']
			])
		);
		expect(draft.company_slug).toBeNull();
		expect(draft.job_role_slug).toBeNull();
		expect(draft.round).toBeNull();
		expect(draft.source_year).toBeNull();
		expect(draft.answer_guidance).toBeNull();
	});

	it('rejects a prompt under the twenty-character floor', () => {
		const { errors } = buildQuestionDraft(
			form([
				['title', 'A title'],
				['prompt', 'too short']
			])
		);
		expect(errors.some((e) => e.field === 'prompt')).toBe(true);
	});

	it('rejects a round outside the accepted ten-value set', () => {
		const { errors, draft } = buildQuestionDraft(
			form([
				['title', 'A title'],
				['prompt', LONG_PROMPT],
				['round', 'whiteboard']
			])
		);
		expect(errors.some((e) => e.field === 'round')).toBe(true);
		expect(draft.round).toBeNull();
	});

	it('rejects a year outside 2000 through next year', () => {
		const next = yearMax();
		for (const bad of ['1999', String(next + 1), 'twenty']) {
			const { errors } = buildQuestionDraft(
				form([
					['title', 'A title'],
					['prompt', LONG_PROMPT],
					['source_year', bad]
				])
			);
			expect(errors.some((e) => e.field === 'source_year')).toBe(true);
		}
	});

	it('drops duplicate topic checkboxes, which the API would refuse', () => {
		const { draft } = buildQuestionDraft(
			form([
				['title', 'A title'],
				['prompt', LONG_PROMPT],
				['topic_slugs', 'arrays'],
				['topic_slugs', 'arrays']
			])
		);
		expect(draft.topic_slugs).toEqual(['arrays']);
	});
});

describe('experience drafts', () => {
	const base: [string, string][] = [
		['title', 'Acme interview'],
		['narrative', LONG_NARRATIVE]
	];

	it('omits the outcome entirely when the author hides it', () => {
		/*
		 * Omitted, not null. A null outcome renders as "Unknown" downstream,
		 * which discloses something the author chose to withhold.
		 */
		const { draft, errors } = buildExperienceDraft(
			form([...base, ['outcome', 'offered']])
		);
		expect(errors).toEqual([]);
		expect(draft.outcome_visible).toBe(false);
		expect('outcome' in draft).toBe(false);
	});

	it('requires an outcome when the author chooses to show one', () => {
		const { errors } = buildExperienceDraft(form([...base, ['outcome_visible', 'on']]));
		expect(errors.some((e) => e.field === 'outcome')).toBe(true);
	});

	it('keeps a shown outcome, including unknown', () => {
		const { draft } = buildExperienceDraft(
			form([...base, ['outcome_visible', 'on'], ['outcome', 'unknown']])
		);
		expect(draft.outcome).toBe('unknown');
	});

	it('drops blank round slots and keeps the remaining order', () => {
		const { draft, errors } = buildExperienceDraft(
			form([
				...base,
				['round', 'online_assessment'],
				['round_notes', 'Two coding questions.'],
				['round', ''],
				['round_notes', ''],
				['round', 'hr'],
				['round_notes', '']
			])
		);
		expect(errors).toEqual([]);
		expect(draft.rounds).toEqual([
			{ round: 'online_assessment', notes: 'Two coding questions.' },
			{ round: 'hr', notes: null }
		]);
	});

	it('never sends an ordinal, which the API assigns from request order', () => {
		const { draft } = buildExperienceDraft(
			form([...base, ['round', 'coding'], ['round_notes', '']])
		);
		expect(Object.keys(draft.rounds[0]!)).toEqual(['round', 'notes']);
	});

	it('flags notes left on a slot with no round chosen', () => {
		/* Silently dropping them would lose text the author typed. */
		const { errors } = buildExperienceDraft(
			form([...base, ['round', ''], ['round_notes', 'They asked about my project.']])
		);
		expect(errors.some((e) => e.field === 'round_0')).toBe(true);
	});

	it('rejects a narrative under the twenty-character floor', () => {
		const { errors } = buildExperienceDraft(
			form([
				['title', 'Acme interview'],
				['narrative', 'went fine']
			])
		);
		expect(errors.some((e) => e.field === 'narrative')).toBe(true);
	});

	it('carries the anonymity choice through unchanged', () => {
		expect(buildExperienceDraft(form(base)).draft.anonymous).toBe(false);
		expect(buildExperienceDraft(form([...base, ['anonymous', 'on']])).draft.anonymous).toBe(true);
	});
});
