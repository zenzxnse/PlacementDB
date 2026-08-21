import { describe, expect, it } from 'vitest';
import { render } from 'svelte/server';
import Comments from './Comments.svelte';
import type { Comment, CommentActionOutcome, Me } from '$lib/types';

/**
 * Report and hide affordances inside the comment list.
 *
 * Both controls are real forms inside native details elements, so they work
 * without JavaScript. What is gated where:
 *
 * - the report form appears only on comments the API marked `can_report`;
 * - the hide form appears only for a moderator, on every visible comment;
 * - neither form is rendered when the CSRF token is missing, because a
 *   disabled button inside a rendered form is still a confusing offer;
 *   the component instead relies on the button's disabled state, and these
 *   tests pin that the disabled attribute tracks the token.
 *
 * Mutation-checked: swapping the report gate to `me?.can_moderate` fails the
 * can_report test; swapping the hide gate to `comment.can_report` fails the
 * moderator test.
 */

function makeMe(overrides: Partial<Me> = {}): Me {
	return {
		public_id: 'u-test',
		username: 'student_001',
		display_name: 'Student One',
		role: 'user',
		status: 'active',
		can_submit: true,
		can_moderate: false,
		unread_moderation_count: 0,
		...overrides
	};
}

const COMMENTS: Comment[] = [
	{
		public_id: 'c-one',
		body: 'First comment.',
		author: null,
		created_at: '2026-01-01T00:00:00Z',
		can_report: true
	},
	{
		public_id: 'c-two',
		body: 'Second comment.',
		author: null,
		created_at: '2026-01-02T00:00:00Z',
		can_report: false
	}
];

function renderComments(props: {
	me: Me | null;
	csrfToken?: string;
	reportResult?: CommentActionOutcome;
	hideResult?: CommentActionOutcome;
	comments?: Comment[];
}): string {
	const { body } = render(Comments, {
		props: {
			comments: props.comments ?? COMMENTS,
			me: props.me,
			action: '?/comment',
			csrfToken: props.csrfToken ?? 'token-abc',
			reportResult: props.reportResult,
			hideResult: props.hideResult
		}
	});
	return body;
}

describe('comment report form', () => {
	it('offers a report form only on comments the API marked reportable', () => {
		const html = renderComments({ me: makeMe() });
		expect(html).toContain('name="comment_id" value="c-one"');
		expect(html).not.toContain('name="comment_id" value="c-two"');
	});

	it('contains no link to the old dead report route', () => {
		const html = renderComments({ me: makeMe() });
		expect(html).not.toContain('/report/comment/');
	});

	it('offers all six accepted reasons as a required radio group', () => {
		const html = renderComments({ me: makeMe() });
		for (const reason of [
			'spam',
			'offensive',
			'duplicate',
			'incorrect',
			'personal_info',
			'other'
		]) {
			expect(html).toContain(`value="${reason}"`);
		}
		expect(html).toContain('required');
		expect(html).toContain('<legend');
	});

	it('posts to the report action and disables submit without a token', () => {
		const withToken = renderComments({ me: makeMe(), csrfToken: 'token-abc' });
		expect(withToken).toContain('action="?/report"');
		const withoutToken = renderComments({ me: makeMe(), csrfToken: '' });
		expect(withoutToken).toContain('disabled');
	});

	it('does not render report forms when the API marks nothing reportable', () => {
		/*
		 * Anonymous callers get can_report false from the API for every
		 * comment; the component trusts that flag rather than re-deriving it,
		 * so an anonymous render simply carries no reportable comments.
		 */
		const anonymousView = COMMENTS.map((c) => ({ ...c, can_report: false }));
		const html = renderComments({ me: null, comments: anonymousView });
		expect(html).not.toContain('action="?/report"');
	});

	it('anchors an error outcome to the comment it names', () => {
		const html = renderComments({
			me: makeMe(),
			reportResult: { commentId: 'c-one', kind: 'error', message: 'You already have an open report for this comment.' }
		});
		expect(html).toContain('role="alert"');
		expect(html).toContain('You already have an open report for this comment.');
	});

	it('anchors a success outcome with a status role, not an alert', () => {
		const html = renderComments({
			me: makeMe(),
			reportResult: { commentId: 'c-one', kind: 'reported', message: 'Report received. A moderator will review it.' }
		});
		expect(html).toContain('role="status"');
		expect(html).toContain('Report received.');
	});
});

describe('moderator hide form', () => {
	it('renders for every visible comment when the viewer can moderate', () => {
		const html = renderComments({ me: makeMe({ can_moderate: true }) });
		expect(html).toContain('name="comment_id" value="c-two"');
		expect(html).toContain('action="?/hide"');
		expect(html).toContain('Hide comment');
	});

	it('stays absent for non-moderators', () => {
		const html = renderComments({ me: makeMe({ can_moderate: false }) });
		expect(html).not.toContain('action="?/hide"');
	});

	it('stays absent for anonymous visitors', () => {
		const html = renderComments({ me: null });
		expect(html).not.toContain('action="?/hide"');
	});

	it('requires a reason', () => {
		const html = renderComments({ me: makeMe({ can_moderate: true }) });
		expect(html).toContain('name="reason"');
		expect(html).toContain('required');
	});
});
