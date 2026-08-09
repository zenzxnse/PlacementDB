import { error, fail } from '@sveltejs/kit';
import type { PageServerLoad } from './$types';
import { getQuestion, getMe, listComments } from '$lib/server/content';
import { apiCall, toFailure } from '$lib/server/api';
import { COMMENT_MAX_LENGTH } from '$lib/types';
import type { Actions } from './$types';

export const load: PageServerLoad = async (event) => {
	const question = await getQuestion(event, event.params.slug);
	if (!question) error(404, 'That question does not exist.');
	const [me, comments] = await Promise.all([
		getMe(event),
		listComments(event, 'questions', event.params.slug,
			event.url.searchParams.get('after') ?? undefined)
	]);
	let csrfToken = '';
	if (me) {
		try {
			csrfToken = (await apiCall<{ csrf_token: string }>(event, '/auth/csrf')).csrf_token;
		} catch {
			/* Comment form renders disabled; the action refuses without a token. */
		}
	}
	return { question, me, comments, csrfToken };
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
	}
};
