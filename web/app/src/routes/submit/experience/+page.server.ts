import { fail, redirect } from '@sveltejs/kit';
import type { Actions, PageServerLoad } from './$types';
import { loadSubmitPage, postDraft } from '$lib/server/submit';
import { buildExperienceDraft } from '$lib/submission';

/**
 * Experience submission, per section 3 of the accepted contract.
 *
 * Rounds are sent in request order and the API assigns contiguous ordinals, so
 * nothing here computes one. When the author hides the outcome, the field is
 * omitted rather than sent as null.
 */

export const load: PageServerLoad = async (event) => {
	const page = await loadSubmitPage(event, '/submit/experience');
	return { ...page, submitted: event.url.searchParams.get('submitted') === '1' };
};

export const actions: Actions = {
	default: async (event) => {
		const form = await event.request.formData();
		const token = String(form.get('_csrf') ?? '');
		const { draft, errors } = buildExperienceDraft(form);

		if (token.length === 0) {
			return fail(403, {
				draft,
				errors: [
					{ field: 'form', message: 'This form expired. Reload the page and submit again.' }
				]
			});
		}
		if (errors.length > 0) {
			return fail(400, { draft, errors });
		}

		const outcome = await postDraft(event, '/experiences', draft, token);
		if (!outcome.ok) {
			return fail(outcome.status, { draft, errors: outcome.errors });
		}

		redirect(303, '/submit/experience?submitted=1');
	}
};
