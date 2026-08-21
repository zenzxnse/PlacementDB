import { error, redirect } from '@sveltejs/kit';
import type { PageServerLoad } from './$types';
import { getMe, loginWithReturn } from '$lib/server/content';
import { myReports, mySubmissions, myVotes } from '$lib/server/activity';

export const load: PageServerLoad = async (event) => {
	const me = await getMe(event);
	if (!me) redirect(303, loginWithReturn(event.url.pathname + event.url.search));
	const section = event.url.searchParams.get('section') ?? 'submissions';
	const cursor = event.url.searchParams.get('cursor') ?? undefined;
	if (section === 'submissions') return { section, page: await mySubmissions(event, cursor) };
	if (section === 'votes') return { section, page: await myVotes(event, cursor) };
	if (section === 'reports') return { section, page: await myReports(event, cursor) };
	error(400, 'Choose a valid activity section.');
};
