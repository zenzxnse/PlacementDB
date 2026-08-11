import { redirect } from '@sveltejs/kit';
import type { PageServerLoad } from './$types';
import { getMe, loginWithReturn } from '$lib/server/content';

/** Chooser page. Anonymous visitors are sent to log in and returned here. */
export const load: PageServerLoad = async (event) => {
	const me = await getMe(event);
	if (!me) redirect(303, loginWithReturn('/submit'));
	return { me };
};
