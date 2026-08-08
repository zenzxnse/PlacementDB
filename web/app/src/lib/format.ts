import type { Difficulty, Round, Outcome } from './types';

const ROUND_LABELS: Record<Round, string> = {
	online_assessment: 'Online assessment',
	technical: 'Technical',
	system_design: 'System design',
	hr: 'HR',
	managerial: 'Managerial',
	other: 'Other'
};

const OUTCOME_LABELS: Record<Outcome, string> = {
	offered: 'Offered',
	rejected: 'Rejected',
	withdrew: 'Withdrew',
	unknown: 'Unknown'
};

export function roundLabel(round: Round): string {
	return ROUND_LABELS[round];
}

export function outcomeLabel(outcome: Outcome): string {
	return OUTCOME_LABELS[outcome];
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
