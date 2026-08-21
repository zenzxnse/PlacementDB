import { error, fail } from '@sveltejs/kit';
import type { Actions, PageServerLoad } from './$types';
import { apiCall, ApiError, toFailure } from '$lib/server/api';
import { isReportState, moderationReports, requireModerator } from '$lib/server/moderation';
import { parseCsrf } from '$lib/wire';
import { REPORT_REASON_MAX_LENGTH, REPORT_STATE_VALUES } from '$lib/types';

/**
 * Moderator report management, against the accepted comments/moderation
 * contract: GET /moderation/reports with a state filter and cursor, and
 * POST /moderation/reports/{public_id}/resolve with a compare-and-set on the
 * expected state so two moderators cannot silently overwrite each other.
 *
 * The page defaults to open reports, the queue a moderator actually works
 * from; the other states stay reachable through a plain GET form so the
 * filter works without JavaScript.
 */
export const load: PageServerLoad = async (event) => {
	await requireModerator(event);
	const requested = event.url.searchParams.get('state') ?? 'open';
	if (!isReportState(requested)) {
		error(400, 'That report state does not exist.');
	}
	const [reports, csrf] = await Promise.all([
		moderationReports(event, requested, event.url.searchParams.get('cursor') ?? undefined),
		apiCall<{ csrf_token: string }>(event, '/auth/csrf', { parse: parseCsrf }).catch(() => null)
	]);
	return { state: requested, reports, csrfToken: csrf?.csrf_token ?? '' };
};

export const actions: Actions = {
	resolve: async (event) => {
		await requireModerator(event);
		const form = await event.request.formData();
		const publicId = String(form.get('public_id') ?? '');
		const expectedState = String(form.get('expected_state') ?? '');
		const decision = String(form.get('decision') ?? '');
		const reason = String(form.get('reason') ?? '').trim();
		const csrf = String(form.get('_csrf') ?? '');
		if (!publicId || !csrf) {
			return fail(403, { message: 'This form expired. Reload the page and try again.', failed: true });
		}
		if (
			!(REPORT_STATE_VALUES as readonly string[]).includes(expectedState) ||
			!['resolved', 'dismissed'].includes(decision)
		) {
			return fail(400, { message: 'Choose a valid outcome for the report.', failed: true });
		}
		if (reason.length === 0) {
			return fail(400, { message: 'A reason is required for every report decision.', failed: true });
		}
		if (reason.length > REPORT_REASON_MAX_LENGTH) {
			return fail(400, {
				message: `The reason is limited to ${REPORT_REASON_MAX_LENGTH} characters.`,
				failed: true
			});
		}
		try {
			await apiCall(
				event,
				`/moderation/reports/${encodeURIComponent(publicId)}/resolve`,
				{
					method: 'POST',
					headers: { 'x-csrf-token': csrf },
					body: { expected_state: expectedState, decision, reason }
				}
			);
			return { message: 'Report decision recorded.' };
		} catch (error_) {
			if (error_ instanceof ApiError && error_.status === 409) {
				return fail(409, {
					message:
						'Another moderator already moved this report. Reload to see its current state before deciding again.',
					failed: true
				});
			}
			if (error_ instanceof ApiError && error_.status === 429) {
				const wait = error_.retryAfterSeconds;
				return fail(429, {
					message: wait
						? `Too many moderation actions. Try again in about ${wait} seconds.`
						: 'Too many moderation actions. Try again later.',
					failed: true
				});
			}
			return fail(502, { message: toFailure(error_).message, failed: true });
		}
	}
};
