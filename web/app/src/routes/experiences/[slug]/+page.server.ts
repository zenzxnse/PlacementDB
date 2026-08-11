import { error, fail } from '@sveltejs/kit';
import type { PageServerLoad } from './$types';
import { getExperience, getMe, listComments } from '$lib/server/content';
import { apiCall, toFailure } from '$lib/server/api';
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
	if (me && !commentsError) {
		try {
			csrfToken = (await apiCall(event, '/auth/csrf', { parse: parseCsrf })).csrf_token;
		} catch {
			/* Comment form renders disabled; the action refuses without a token. */
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
		/* Redirect after post so a reload does not repost the comment. */
		return { commentPosted: true };
	}
};
