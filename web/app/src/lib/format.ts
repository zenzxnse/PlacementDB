import type { Difficulty, Round, Outcome, OutcomeVisibility } from './types';

const ROUND_LABELS: Record<Round, string> = {
	online_assessment: 'Online assessment',
	aptitude: 'Aptitude',
	coding: 'Coding',
	technical: 'Technical',
	system_design: 'System design',
	behavioral: 'Behavioural',
	managerial: 'Managerial',
	group_discussion: 'Group discussion',
	hr: 'HR',
	other: 'Other'
};

const OUTCOME_LABELS: Record<Outcome, string> = {
	offered: 'Offered',
	rejected: 'Rejected',
	withdrew: 'Withdrew',
	unknown: 'Unknown'
};

export function roundLabel(round: Round | null | undefined): string {
	return round ? ROUND_LABELS[round] : 'Not recorded';
}

export function outcomeLabel(outcome: Outcome | null | undefined): string {
	return outcome ? OUTCOME_LABELS[outcome] : 'Not recorded';
}

/*
 * Difficulty vocabulary, per the 2026-08-11 user decision. Integers 1-5 stay
 * on the wire; the names are presentation and live here so a future rename is
 * one edit. "Easy", "Medium", and "Hard" are retired from every surface.
 *
 * The default level is 3 (Standard). An unrated question scores exactly 3.0
 * through the weighted prior, so difficultyName(3.0) is "Standard" and the
 * summary pairs it with "no ratings yet" so the default is never mistaken for
 * a community placement.
 */
export const DIFFICULTY_NAMES: Readonly<Record<number, string>> = {
	1: 'Fundamentals',
	2: 'Screening',
	3: 'Standard',
	4: 'Advanced',
	5: 'Bar-raiser'
};

/*
 * Derived from the mapping above with no cast and no possibly-undefined
 * lookup: the entries are the source, so a name added or removed there shows
 * up here automatically and the compiler can still see that every name is a
 * string.
 */
export const DIFFICULTY_LEVELS: readonly { value: number; name: string }[] =
	Object.entries(DIFFICULTY_NAMES)
		.map(([key, name]) => ({ value: Number(key), name }))
		.sort((a, b) => a.value - b.value);

/** Maps a weighted mean onto the 1-5 integer scale, clamped and rounded. */
export function difficultyLevel(mean: number): number {
	return Math.min(5, Math.max(1, Math.round(mean)));
}

/** The level name for a weighted mean, read from the single mapping above. */
export function difficultyName(mean: number): string {
	return DIFFICULTY_NAMES[difficultyLevel(mean)] ?? DIFFICULTY_NAMES[3]!;
}

export function difficultySummary(d: Difficulty): string {
	if (d.vote_count === 0) {
		/*
		 * The two facts always travel together: the default level name and
		 * the honest "no ratings yet" disclosure. The mean is 3.0 here but
		 * naming it would imply the community placed it there.
		 */
		return `${DIFFICULTY_NAMES[3]!} \u00b7 no ratings yet`;
	}
	const votes = d.vote_count === 1 ? '1 vote' : `${d.vote_count} votes`;
	return `${difficultyName(d.mean)} \u00b7 ${d.mean.toFixed(1)} from ${votes}`;
}

export function formatDate(iso: string): string {
	const date = new Date(iso);
	return new Intl.DateTimeFormat('en-IN', {
		dateStyle: 'medium',
		timeZone: 'UTC'
	}).format(date);
}

export function pluralize(count: number, singular: string, plural?: string): string {
	return count === 1 ? singular : (plural ?? `${singular}s`);
}

/**
 * Company display name.
 *
 * The schema allows company-agnostic questions, so the contract types company
 * as nullable. Templates render this rather than reaching into a possibly
 * null object.
 */
export function companyName(company: { name: string } | null | undefined): string {
	return company?.name ?? 'Not recorded';
}

/**
 * Outcome for display, honouring the author's visibility choice.
 *
 * Returns null when the outcome is hidden so a template can omit the row
 * entirely. A hidden outcome must never render as "Unknown", which would
 * disclose something about the placement the author withheld.
 */
export function visibleOutcome(value: OutcomeVisibility): string | null {
	/*
	 * The union has no `outcome` property on the hidden branch, so this cannot
	 * read one by accident and the old `as never` cast is gone. The narrowing
	 * is the check.
	 */
	return value.outcome_visible ? outcomeLabel(value.outcome) : null;
}

/**
 * Splits a narrative into paragraphs for rendering.
 *
 * Blank lines only. The API stores and returns one string; this is a display
 * concern and never alters the stored text. Returns the whole string as a
 * single paragraph when it contains no blank line.
 */
export function splitParagraphs(text: string): string[] {
	return text
		.split(/\n\s*\n/)
		.map((part) => part.trim())
		.filter((part) => part.length > 0);
}
