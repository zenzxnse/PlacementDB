import { fail } from '@sveltejs/kit';
import type { Actions, PageServerLoad } from './$types';
import { apiCall, toFailure } from '$lib/server/api';
import { moderationQueue, requireModerator } from '$lib/server/moderation';
import { parseCsrf } from '$lib/wire';

export const load: PageServerLoad = async (event) => {
	const me = await requireModerator(event);
	const [queue, csrf] = await Promise.all([
		moderationQueue(event, event.url.searchParams.get('cursor') ?? undefined),
		apiCall<{ csrf_token: string }>(event, '/auth/csrf', { parse: parseCsrf })
	]);
	return { me, queue, csrfToken: csrf.csrf_token };
};

export const actions: Actions = {
	default: async (event) => {
		await requireModerator(event);
		const form = await event.request.formData();
		const publicId = String(form.get('public_id') ?? '');
		const targetType = String(form.get('target_type') ?? '');
		const action = String(form.get('decision') ?? '');
		const reason = String(form.get('reason') ?? '').trim();
		const csrf = String(form.get('_csrf') ?? '');
		if (!publicId || !['question', 'experience'].includes(targetType) || !['approve', 'request_changes', 'reject'].includes(action) || !reason || !csrf)
			return fail(400, { message: 'Choose an action and provide a reason.' });
		try {
			await apiCall(event, `/moderation/items/${encodeURIComponent(publicId)}/action`, {
				method: 'POST', headers: { 'x-csrf-token': csrf },
				body: { target_type: targetType, action, expected_state: 'pending_review', reason }
			});
			return { message: 'Moderation decision recorded.' };
		} catch (cause) { return fail(409, { message: toFailure(cause).message }); }
	}
};
