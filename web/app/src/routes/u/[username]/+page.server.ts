import { error } from '@sveltejs/kit';
import type { PageServerLoad } from './$types';
import { getMe, getProfile } from '$lib/server/content';

export const load: PageServerLoad = async (event) => {
	const profile = await getProfile(event, event.params.username);
	if (!profile) error(404, 'That profile does not exist.');
	/*
	 * Only the signed-in owner sees the edit affordance. This is presentation
	 * only: the profile action itself is authorized by the API, so hiding the
	 * link grants nothing and showing it leaks nothing.
	 */
	const me = await getMe(event);
	const isSelf = me?.username === profile.username;
	return { profile, isSelf };
};
