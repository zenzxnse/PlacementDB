import { describe, expect, it } from 'vitest';
import {
	DIFFICULTY_LEVELS,
	DIFFICULTY_NAMES,
	difficultyLevel,
	difficultyName,
	difficultySummary
} from './format';

/**
 * Difficulty vocabulary, per the 2026-08-11 lead decision.
 *
 * These tests exist because the vocabulary is the kind of thing that drifts
 * back: a component hardcodes "Medium", a filter keeps an old label, or the
 * zero-vote case starts presenting the prior as a community placement. Each
 * assertion below fails if one of those happens.
 *
 * Mutation-checked: changing 3 to 'Moderate' in DIFFICULTY_NAMES fails the
 * name-mapping and zero-vote tests and nothing else; removing the
 * "no ratings yet" clause fails only the zero-vote disclosure test.
 */

describe('the five level names', () => {
	it('maps 1 through 5 to the accepted names', () => {
		expect(DIFFICULTY_NAMES[1]).toBe('Fundamentals');
		expect(DIFFICULTY_NAMES[2]).toBe('Screening');
		expect(DIFFICULTY_NAMES[3]).toBe('Standard');
		expect(DIFFICULTY_NAMES[4]).toBe('Advanced');
		expect(DIFFICULTY_NAMES[5]).toBe('Bar-raiser');
	});

	it('retires easy, medium, and hard from the vocabulary', () => {
		const names = Object.values(DIFFICULTY_NAMES).map((n) => n.toLowerCase());
		expect(names).not.toContain('easy');
		expect(names).not.toContain('medium');
		expect(names).not.toContain('hard');
	});

	it('exposes exactly five levels in ascending order for form controls', () => {
		expect(DIFFICULTY_LEVELS.map((l) => l.value)).toEqual([1, 2, 3, 4, 5]);
		expect(DIFFICULTY_LEVELS.map((l) => l.name)).toEqual([
			'Fundamentals',
			'Screening',
			'Standard',
			'Advanced',
			'Bar-raiser'
		]);
	});
});

describe('mapping a weighted mean onto a level', () => {
	it('rounds to the nearest integer level', () => {
		expect(difficultyLevel(3.4)).toBe(3);
		expect(difficultyLevel(3.6)).toBe(4);
		expect(difficultyName(4.2)).toBe('Advanced');
	});

	it('clamps out-of-range means rather than returning nothing', () => {
		/* The wire says 1-5; a bad upstream value must not blank the label. */
		expect(difficultyLevel(0)).toBe(1);
		expect(difficultyLevel(9)).toBe(5);
		expect(difficultyName(-2)).toBe('Fundamentals');
		expect(difficultyName(99)).toBe('Bar-raiser');
	});
});

describe('the summary line', () => {
	it('pairs Standard with the no-ratings disclosure at zero votes', () => {
		/*
		 * The two facts always travel together. Showing "Standard" alone would
		 * present the prior of 3 as though the community placed it there.
		 */
		const summary = difficultySummary({ mean: 3, vote_count: 0 });
		expect(summary).toContain('Standard');
		expect(summary).toContain('no ratings yet');
	});

	it('never shows a numeric mean when there are no votes', () => {
		expect(difficultySummary({ mean: 3, vote_count: 0 })).not.toContain('3.0');
	});

	it('shows the name, the mean, and the vote count once votes exist', () => {
		const summary = difficultySummary({ mean: 4.25, vote_count: 8 });
		expect(summary).toContain('Advanced');
		expect(summary).toContain('4.3');
		expect(summary).toContain('8 votes');
		expect(summary).not.toContain('no ratings yet');
	});

	it('uses the singular for one vote', () => {
		expect(difficultySummary({ mean: 2, vote_count: 1 })).toContain('1 vote');
		expect(difficultySummary({ mean: 2, vote_count: 1 })).not.toContain('1 votes');
	});
});
