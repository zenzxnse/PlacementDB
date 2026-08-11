import type { PageServerLoad } from './$types';
import { moderationAudit, requireModerator } from '$lib/server/moderation';
export const load: PageServerLoad = async (event) => {
	await requireModerator(event);
	return { audit: await moderationAudit(event, event.url.searchParams.get('cursor') ?? undefined) };
};
