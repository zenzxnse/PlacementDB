import { ROUND_VALUES, type Outcome, type Round } from './types';

/**
 * Submission field rules, mirrored from
 * adr/codex/account-submission-difficulty-contract-2026-08-11.md sections 2
 * and 3.
 *
 * The API is authoritative. These checks exist so an obviously invalid draft
 * comes back with a field-level message instead of a round trip and a generic
 * refusal, and so the limits appear in exactly one place rather than in each
 * template. Anything the API rejects that passes here still surfaces through
 * the normal field-error mapping.
 *
 * Pure functions with no SvelteKit or fetch dependency, so they are testable
 * on their own.
 */

export const TITLE_MIN = 1;
export const TITLE_MAX = 200;
export const PROMPT_MIN = 20;
export const PROMPT_MAX = 8000;
export const GUIDANCE_MAX = 8000;
export const NARRATIVE_MIN = 20;
export const NARRATIVE_MAX = 20000;
export const NOTES_MAX = 8000;
export const TOPICS_MAX = 12;
export const ROUNDS_MAX = 20;
export const YEAR_MIN = 2000;

/** Source year accepts 2000 through next year, per the contract. */
export function yearMax(now: Date = new Date()): number {
	return now.getUTCFullYear() + 1;
}

export interface FieldError {
	field: string;
	message: string;
}

export interface QuestionDraft {
	title: string;
	prompt: string;
	answer_guidance: string | null;
	company_slug: string | null;
	job_role_slug: string | null;
	round: Round | null;
	source_year: number | null;
	topic_slugs: string[];
}

export interface ExperienceRoundDraft {
	round: Round;
	notes: string | null;
}

export interface ExperienceDraft {
	title: string;
	narrative: string;
	company_slug: string | null;
	job_role_slug: string | null;
	source_year: number | null;
	outcome_visible: boolean;
	outcome?: Outcome;
	anonymous: boolean;
	rounds: ExperienceRoundDraft[];
}

const OUTCOME_VALUES: readonly Outcome[] = ['offered', 'rejected', 'withdrew', 'unknown'];

function isRound(value: string): value is Round {
	return (ROUND_VALUES as readonly string[]).includes(value);
}

function isOutcome(value: string): value is Outcome {
	return (OUTCOME_VALUES as readonly string[]).includes(value);
}

/** Empty string means "not recorded", which the contract sends as null. */
function optional(value: string): string | null {
	const trimmed = value.trim();
	return trimmed.length === 0 ? null : trimmed;
}

function checkTitle(title: string, errors: FieldError[]): void {
	if (title.length < TITLE_MIN || title.length > TITLE_MAX) {
		errors.push({ field: 'title', message: `Give a title of 1 to ${TITLE_MAX} characters.` });
	}
}

function parseYear(raw: string, errors: FieldError[], now?: Date): number | null {
	const value = raw.trim();
	if (value.length === 0) return null;
	const year = Number(value);
	const max = yearMax(now);
	if (!Number.isInteger(year) || year < YEAR_MIN || year > max) {
		errors.push({
			field: 'source_year',
			message: `Enter a year between ${YEAR_MIN} and ${max}, or leave it blank.`
		});
		return null;
	}
	return year;
}

export function buildQuestionDraft(
	form: {
		get(name: string): FormDataEntryValue | null;
		getAll(name: string): FormDataEntryValue[];
	},
	now?: Date
): { draft: QuestionDraft; errors: FieldError[] } {
	const errors: FieldError[] = [];
	const title = String(form.get('title') ?? '').trim();
	const prompt = String(form.get('prompt') ?? '').trim();
	const guidance = String(form.get('answer_guidance') ?? '');
	const round = String(form.get('round') ?? '').trim();
	/* Duplicate checkbox values would fail the API's uniqueness rule. */
	const topics = [...new Set(form.getAll('topic_slugs').map((t) => String(t).trim()))].filter(
		(t) => t.length > 0
	);

	checkTitle(title, errors);
	if (prompt.length < PROMPT_MIN || prompt.length > PROMPT_MAX) {
		errors.push({
			field: 'prompt',
			message: `The question needs ${PROMPT_MIN} to ${PROMPT_MAX} characters. Write it the way it was asked.`
		});
	}
	if (guidance.trim().length > GUIDANCE_MAX) {
		errors.push({
			field: 'answer_guidance',
			message: `Answer guidance is limited to ${GUIDANCE_MAX} characters.`
		});
	}
	if (round.length > 0 && !isRound(round)) {
		errors.push({ field: 'round', message: 'Choose a round from the list.' });
	}
	if (topics.length > TOPICS_MAX) {
		errors.push({ field: 'topic_slugs', message: `Choose at most ${TOPICS_MAX} topics.` });
	}
	const sourceYear = parseYear(String(form.get('source_year') ?? ''), errors, now);

	return {
		draft: {
			title,
			prompt,
			answer_guidance: optional(guidance),
			company_slug: optional(String(form.get('company_slug') ?? '')),
			job_role_slug: optional(String(form.get('job_role_slug') ?? '')),
			round: round.length > 0 && isRound(round) ? round : null,
			source_year: sourceYear,
			topic_slugs: topics
		},
		errors
	};
}

export function buildExperienceDraft(
	form: {
		get(name: string): FormDataEntryValue | null;
		getAll(name: string): FormDataEntryValue[];
	},
	now?: Date
): { draft: ExperienceDraft; errors: FieldError[] } {
	const errors: FieldError[] = [];
	const title = String(form.get('title') ?? '').trim();
	const narrative = String(form.get('narrative') ?? '').trim();
	const outcomeVisible = form.get('outcome_visible') !== null;
	const outcomeRaw = String(form.get('outcome') ?? '').trim();

	checkTitle(title, errors);
	if (narrative.length < NARRATIVE_MIN || narrative.length > NARRATIVE_MAX) {
		errors.push({
			field: 'narrative',
			message: `Write ${NARRATIVE_MIN} to ${NARRATIVE_MAX} characters about how it went.`
		});
	}

	/*
	 * Rounds come from a fixed set of slots, because adding a row needs
	 * JavaScript and this form has to work without it. Blank slots are dropped
	 * and the remaining order is the request order, which the API turns into
	 * contiguous ordinals; the client never sends an ordinal.
	 */
	const rounds: ExperienceRoundDraft[] = [];
	const roundValues = form.getAll('round').map((r) => String(r).trim());
	const noteValues = form.getAll('round_notes').map((n) => String(n));
	for (let i = 0; i < roundValues.length; i += 1) {
		/* Indexing inside the loop bound is safe, but the type does not know it. */
		const value = roundValues[i] ?? '';
		const notes = noteValues[i] ?? '';
		if (value.length === 0) {
			if (notes.trim().length > 0) {
				errors.push({
					field: `round_${i}`,
					message: 'Choose a round for that step, or clear its notes.'
				});
			}
			continue;
		}
		if (!isRound(value)) {
			errors.push({ field: `round_${i}`, message: 'Choose a round from the list.' });
			continue;
		}
		if (notes.trim().length > NOTES_MAX) {
			errors.push({
				field: `round_${i}`,
				message: `Notes for a round are limited to ${NOTES_MAX} characters.`
			});
			continue;
		}
		rounds.push({ round: value, notes: optional(notes) });
	}
	if (rounds.length > ROUNDS_MAX) {
		errors.push({ field: 'rounds', message: `Describe at most ${ROUNDS_MAX} rounds.` });
	}

	/*
	 * When the outcome is withheld the field is omitted entirely rather than
	 * sent as null. A null outcome would render as "Unknown" somewhere down the
	 * line, which discloses something the author chose to withhold.
	 */
	let outcome: Outcome | undefined;
	if (outcomeVisible) {
		if (!isOutcome(outcomeRaw)) {
			errors.push({ field: 'outcome', message: 'Choose the outcome, or hide it instead.' });
		} else {
			outcome = outcomeRaw;
		}
	}

	const sourceYear = parseYear(String(form.get('source_year') ?? ''), errors, now);

	const draft: ExperienceDraft = {
		title,
		narrative,
		company_slug: optional(String(form.get('company_slug') ?? '')),
		job_role_slug: optional(String(form.get('job_role_slug') ?? '')),
		source_year: sourceYear,
		outcome_visible: outcomeVisible,
		anonymous: form.get('anonymous') !== null,
		rounds
	};
	if (outcome !== undefined) draft.outcome = outcome;
	return { draft, errors };
}
