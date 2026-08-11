import { fail, redirect } from '@sveltejs/kit';
import type { Actions, PageServerLoad } from './$types';
import { loadSubmitPage, postDraft } from '$lib/server/submit';
import { buildQuestionDraft } from '$lib/submission';

/**
 * Question submission, per section 2 of the accepted contract.
 *
 * The server derives author, slug, IDs, timestamps, and state, so this form
 * sends only the editable fields. A successful post lands in `pending_review`:
 * nothing a user submits is publicly visible without a moderation decision.
 */

export const load: PageServerLoad = async (event) => {
	const page = await loadSubmitPage(event, '/submit/question');
	return { ...page, submitted: event.url.searchParams.get('submitted') === '1' };
};

export const actions: Actions = {
	default: async (event) => {
		const form = await event.request.formData();
		const token = String(form.get('_csrf') ?? '');
		const { draft, errors } = buildQuestionDraft(form);

		if (token.length === 0) {
			return fail(403, {
				draft,
				errors: [
					{ field: 'form', message: 'This form expired. Reload the page and submit again.' }
				]
			});
		}
		if (errors.length > 0) {
			/* The draft goes back so nothing typed is lost. */
			return fail(400, { draft, errors });
		}

		const outcome = await postDraft(event, '/questions', draft, token);
		if (!outcome.ok) {
			return fail(outcome.status, { draft, errors: outcome.errors });
		}

		/*
		 * Redirect after success so a reload cannot repost the draft. The
		 * receipt is not carried into the URL: there is no route yet that can
		 * show a pending question to its author, so a link to it would 404.
		 */
		redirect(303, '/submit/question?submitted=1');
	}
};
