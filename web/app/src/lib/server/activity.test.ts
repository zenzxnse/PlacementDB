import { describe, expect, it } from 'vitest';
import { parseReportPage, parseSubmissionPage, parseVotePage } from './activity';

describe('account activity wire parsing', () => {
	it('parses each accepted collection and a hidden vote target', () => {
		expect(parseSubmissionPage({ items: [{ kind: 'question', public_id: 'q', slug: 's', title: 'T', state: 'draft', updated_at: '2026-08-21T00:00:00Z' }], next_cursor: null }).items[0]?.kind).toBe('question');
		expect(parseVotePage({ items: [{ value: 3, updated_at: '2026-08-21T00:00:00Z', target: null }], next_cursor: null }).items[0]?.target).toBeNull();
		expect(parseReportPage({ items: [{ public_id: 'r', target_type: 'user', reason: 'spam', details: null, state: 'open', created_at: '2026-08-21T00:00:00Z' }], next_cursor: null }).items[0]?.state).toBe('open');
	});

	it('rejects invalid unions, values, and cursors', () => {
		expect(() => parseSubmissionPage({ items: [{ kind: 'post' }], next_cursor: null })).toThrow();
		expect(() => parseVotePage({ items: [{ value: 9, updated_at: 'x', target: null }], next_cursor: null })).toThrow();
		expect(() => parseReportPage({ items: [{ public_id: 'r', target_type: 'sql', reason: 'spam', details: null, state: 'open', created_at: 'x' }], next_cursor: null })).toThrow();
		expect(() => parseReportPage({ items: [], next_cursor: 42 })).toThrow();
	});
});
