import type { PageServerLoad } from './$types';
import { listLookupCounts } from '$lib/server/content';

export const load: PageServerLoad = async (event) => ({
	result: await listLookupCounts(event, 'topics')
});
