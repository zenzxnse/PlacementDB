import { redirect } from '@sveltejs/kit';
import type { RequestEvent } from '@sveltejs/kit';
import { apiCall, ApiError, ApiUnavailable, toFailure } from './api';
import { getFilterOptions, getMe, loginWithReturn } from './content';
import type { FilterOptions, Me } from '$lib/types';
import type { Failure } from '$lib/failure';
import { parseCsrf, parseSubmitReceipt, type SubmitReceipt } from '$lib/wire';

/**
 * Shared server work for the two submission forms.
 *
 * Both forms need the same three things: a session that may submit, a
 * synchronizer token, and the vocabulary lists the API will validate the slugs
 * against. Keeping it here means the question and experience routes differ
 * only in their fields.
 */

type Event = Pick<RequestEvent, 'cookies' | 'fetch' | 'request' | 'url'>;

export interface SubmitPageData {
	me: Me;
	csrfToken: string;
	options: FilterOptions | null;
	/** Set when the vocabulary lists could not be loaded. Form still renders. */
	optionsError: Failure | null;
}

/**
 * Loads a submission form, or redirects an anonymous visitor to log in.
 *
 * `can_submit` is the gate, never a role string. A suspended account has the
 * capability false, so it lands on the same explanation rather than a form
 * whose post the API will refuse.
 */
export async function loadSubmitPage(event: Event, returnPath: string): Promise<SubmitPageData> {
	const me = await getMe(event);
	if (!me) redirect(303, loginWithReturn(returnPath));

	const [csrfResult, optionsResult] = await Promise.allSettled([
		apiCall(event, '/auth/csrf', { parse: parseCsrf }),
		getFilterOptions(event)
	]);

	return {
		me,
		csrfToken: csrfResult.status === 'fulfilled' ? csrfResult.value.csrf_token : '',
		options: optionsResult.status === 'fulfilled' ? optionsResult.value : null,
		optionsError: optionsResult.status === 'fulfilled' ? null : toFailure(optionsResult.reason)
	};
}

/* The receipt shape and its validator both live in $lib/wire. */
export type { SubmitReceipt };

export type SubmitOutcome =
	| { ok: true; receipt: SubmitReceipt }
	| { ok: false; status: number; errors: { field: string; message: string }[] };

/**
 * Posts a draft and maps every failure onto field or form messages.
 *
 * Never retried. A submission that timed out may well have been accepted, and
 * reposting it would create a duplicate for a moderator to clean up.
 */
export async function postDraft(
	event: Pick<RequestEvent, 'cookies' | 'fetch' | 'request'>,
	path: string,
	body: unknown,
	csrfToken: string
): Promise<SubmitOutcome> {
	try {
		const receipt = await apiCall(event, path, {
			method: 'POST',
			headers: { 'x-csrf-token': csrfToken },
			body,
			parse: parseSubmitReceipt
		});
		return { ok: true, receipt };
	} catch (error) {
		if (error instanceof ApiUnavailable) {
			return {
				ok: false,
				status: 503,
				errors: [
					{
						field: 'form',
						message:
							'Submissions are unavailable right now. Your text is still here; try again shortly.'
					}
				]
			};
		}
		if (error instanceof ApiError) {
			if (error.status === 401) {
				return {
					ok: false,
					status: 401,
					errors: [
						{
							field: 'form',
							message: 'Your session expired. Log in again in another tab, then submit once more.'
						}
					]
				};
			}
			if (error.status === 429) {
				const wait = error.retryAfterSeconds;
				return {
					ok: false,
					status: 429,
					errors: [
						{
							field: 'form',
							message: wait
								? `You have submitted several drafts already. Try again in about ${wait} seconds.`
								: 'You have submitted several drafts already. Try again later.'
						}
					]
				};
			}
			const fields = error.fields.length
				? error.fields.map((f) => ({ field: f.field, message: f.message }))
				: [{ field: 'form', message: error.message }];
			return { ok: false, status: error.status, errors: fields };
		}
		return { ok: false, status: 502, errors: [{ field: 'form', message: toFailure(error).message }] };
	}
}
