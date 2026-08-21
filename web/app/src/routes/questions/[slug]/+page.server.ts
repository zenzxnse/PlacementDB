import { error, fail } from '@sveltejs/kit';
import type { PageServerLoad } from './$types';
import { getQuestion, getMe, listComments, loginWithReturn } from '$lib/server/content';
import { apiCall, ApiError, toFailure } from '$lib/server/api';
import { hideComment, reportComment, reportContent } from '$lib/server/comment-actions';
import { parseCsrf, parseDifficultyVote } from '$lib/wire';
import {
	COMMENT_MAX_LENGTH,
	DIFFICULTY_MAX,
	DIFFICULTY_MIN,
	type CommentPage,
	type DifficultyVoteResult
} from '$lib/types';
import type { Failure } from '$lib/failure';
import type { Actions } from './$types';

export const load: PageServerLoad = async (event) => {
	const question = await getQuestion(event, event.params.slug);
	if (!question) error(404, 'That question does not exist.');
	/*
	 * getMe degrades internally to anonymous; listComments does not. Without
	 * settling it here, a comment-endpoint outage takes down the whole
	 * question page even though the prompt and author render fine. Settling
	 * lets the page show the article and an honest "comments unavailable"
	 * panel instead.
	 */
	const [meResult, commentsResult] = await Promise.allSettled([
		getMe(event),
		listComments(event, 'questions', event.params.slug,
			event.url.searchParams.get('after') ?? undefined)
	]);
	const me = meResult.status === 'fulfilled' ? meResult.value : null;
	let comments: CommentPage = { items: [], next_cursor: null };
	let commentsError: Failure | null = null;
	if (commentsResult.status === 'fulfilled') {
		comments = commentsResult.value;
	} else {
		commentsError = toFailure(commentsResult.reason);
	}
	let csrfToken = '';
	if (me) {
		/*
		 * Issued whenever there is a session, not only when comments loaded.
		 * The difficulty vote needs the same token, and a comment outage must
		 * not silently disable rating.
		 */
		try {
			csrfToken = (await apiCall(event, '/auth/csrf', { parse: parseCsrf })).csrf_token;
		} catch {
			/* Both forms render disabled; the actions refuse without a token. */
		}
	}
	return { question, me, comments, commentsError, csrfToken };
};

export const actions: Actions = {
	/** Post a comment. Plain text only; the API escapes nothing on our behalf. */
	comment: async (event) => {
		const form = await event.request.formData();
		const body = String(form.get('body') ?? '').trim();
		const token = String(form.get('_csrf') ?? '');
		if (token.length === 0) {
			return fail(403, { commentError: 'This form expired. Reload and try again.' });
		}
		if (body.length === 0) {
			return fail(400, { commentError: 'Write something before posting.' });
		}
		if (body.length > COMMENT_MAX_LENGTH) {
			return fail(400, {
				commentError: `Comments are limited to ${COMMENT_MAX_LENGTH} characters.`
			});
		}
		try {
			await apiCall(event, `/questions/by-slug/${encodeURIComponent(event.params.slug)}/comments`, {
				method: 'POST',
				headers: { 'x-csrf-token': token },
				body: { body }
			});
		} catch (error_) {
			return fail(502, { commentError: toFailure(error_).message });
		}
		/* Redirect after post so a reload does not repost the comment. */
		return { commentPosted: true };
	},

	/**
	 * Difficulty vote, per section 4 of the accepted account/submission/
	 * difficulty contract. PUT is idempotent: it creates or changes the single
	 * active vote for this user and question.
	 *
	 * The question's public ID is re-derived from the slug rather than taken
	 * from a hidden field, so a tampered form cannot aim the vote at another
	 * question. The API authorizes it too; this just refuses to be the tool.
	 *
	 * Never automatically retried, per the resilience rules for mutations.
	 */
	vote: async (event) => {
		const form = await event.request.formData();
		const token = String(form.get('_csrf') ?? '');
		const raw = String(form.get('value') ?? '');
		if (token.length === 0) {
			return fail(403, { voteError: 'This form expired. Reload the page and try again.' });
		}
		const value = Number(raw);
		if (!Number.isInteger(value) || value < DIFFICULTY_MIN || value > DIFFICULTY_MAX) {
			return fail(400, {
				voteError: `Choose a difficulty from ${DIFFICULTY_MIN} to ${DIFFICULTY_MAX}.`
			});
		}

		const question = await getQuestion(event, event.params.slug);
		if (!question) error(404, 'That question does not exist.');

		try {
			const result = await apiCall(
				event,
				`/questions/${encodeURIComponent(question.public_id)}/difficulty`,
				{
					method: 'PUT',
					headers: { 'x-csrf-token': token },
					body: { value },
					parse: parseDifficultyVote
				}
			);
			return { difficulty: result.difficulty, myVote: result.my_vote };
		} catch (error_) {
			if (error_ instanceof ApiError) {
				if (error_.status === 401) {
					return fail(401, {
						voteError: 'Your session expired. Log in again to rate this question.',
						voteLoginHref: loginWithReturn(`/questions/${event.params.slug}`)
					});
				}
				if (error_.status === 403) {
					return fail(403, { voteError: 'You cannot rate this question.' });
				}
				if (error_.status === 429) {
					const wait = error_.retryAfterSeconds;
					return fail(429, {
						voteError: wait
							? `Too many ratings. Try again in about ${wait} seconds.`
							: 'Too many ratings. Try again later.'
					});
				}
			}
			/* Everything else keeps only the safe classified message. */
			return fail(502, { voteError: toFailure(error_).message });
		}
	},

	/**
	 * Report a comment, per the accepted comments/moderation contract.
	 * Shared logic lives in comment-actions; this only unwraps the form.
	 */
	report: async (event) => {
		const form = await event.request.formData();
		const result = await reportComment(event, {
			csrf: String(form.get('_csrf') ?? ''),
			commentId: String(form.get('comment_id') ?? ''),
			reason: String(form.get('reason') ?? ''),
			details: String(form.get('details') ?? '').trim()
		});
		return result.kind === 'error' ? fail(400, { reportResult: result }) : { reportResult: result };
	},

	/** Moderator hide, with the required reason. */
	hide: async (event) => {
		const form = await event.request.formData();
		const result = await hideComment(event, {
			csrf: String(form.get('_csrf') ?? ''),
			commentId: String(form.get('comment_id') ?? ''),
			reason: String(form.get('reason') ?? '')
		});
		return result.kind === 'error' ? fail(400, { hideResult: result }) : { hideResult: result };
	},
	reportContent: async (event) => {
		const form = await event.request.formData();
		const question = await getQuestion(event, event.params.slug);
		if (!question) error(404, 'That question does not exist.');
		const result = await reportContent(event, {
			csrf: String(form.get('_csrf') ?? ''), targetType: 'question', publicId: question.public_id,
			reason: String(form.get('reason') ?? ''), details: String(form.get('details') ?? '').trim()
		});
		return result.kind === 'error' ? fail(400, { contentReport: result }) : { contentReport: result };
	}
};
