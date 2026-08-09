import type { Difficulty, Round, Outcome } from './types';

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

export function difficultySummary(d: Difficulty): string {
	if (d.vote_count === 0 || d.mean === null) {
		return 'not rated yet';
	}
	const votes = d.vote_count === 1 ? '1 vote' : `${d.vote_count} votes`;
	return `${d.mean.toFixed(1)} from ${votes}`;
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
export function visibleOutcome(value: {
	outcome_visible: boolean;
	outcome?: string | null;
}): string | null {
	if (!value.outcome_visible) return null;
	return value.outcome ? outcomeLabel(value.outcome as never) : 'Not recorded';
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
