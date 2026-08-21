import { error, fail } from '@sveltejs/kit';
import type { PageServerLoad } from './$types';
import { getExperience, getMe, listComments } from '$lib/server/content';
import { apiCall, toFailure } from '$lib/server/api';
import { hideComment, reportComment, reportContent } from '$lib/server/comment-actions';
import { parseCsrf } from '$lib/wire';
import { COMMENT_MAX_LENGTH, type CommentPage } from '$lib/types';
import type { Failure } from '$lib/failure';
import type { Actions } from './$types';

export const load: PageServerLoad = async (event) => {
	const experience = await getExperience(event, event.params.slug);
	if (!experience) error(404, 'That experience does not exist.');
	/*
	 * getMe degrades internally to anonymous; listComments does not. Settling
	 * it here lets the experience article render even when the comment
	 * endpoint is down, with an honest "comments unavailable" panel.
	 */
	const [meResult, commentsResult] = await Promise.allSettled([
		getMe(event),
		listComments(event, 'experiences', event.params.slug,
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
		 * The report form needs the same token, and a comment outage must
		 * not silently disable reporting.
		 */
		try {
			csrfToken = (await apiCall(event, '/auth/csrf', { parse: parseCsrf })).csrf_token;
		} catch {
			/* Both forms render disabled; the actions refuse without a token. */
		}
	}
	return { experience, me, comments, commentsError, csrfToken };
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
			await apiCall(event, `/experiences/by-slug/${encodeURIComponent(event.params.slug)}/comments`, {
				method: 'POST',
				headers: { 'x-csrf-token': token },
				body: { body }
			});
		} catch (error_) {
			return fail(502, { commentError: toFailure(error_).message });
		}
		/* Redirect after post so a reload does not repost. */
		return { commentPosted: true };
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
		const experience = await getExperience(event, event.params.slug);
		if (!experience) error(404, 'That experience does not exist.');
		const result = await reportContent(event, {
			csrf: String(form.get('_csrf') ?? ''), targetType: 'experience', publicId: experience.public_id,
			reason: String(form.get('reason') ?? ''), details: String(form.get('details') ?? '').trim()
		});
		return result.kind === 'error' ? fail(400, { contentReport: result }) : { contentReport: result };
	}
};
