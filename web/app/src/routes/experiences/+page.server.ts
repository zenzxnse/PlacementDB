import type { PageServerLoad } from './$types';
import { getFilterOptions, listExperiences } from '$lib/server/content';
import { parseExperienceFilters, parsePage } from '$lib/query';

export const load: PageServerLoad = async (event) => {
	const filters = parseExperienceFilters(event.url);
	const page = parsePage(event.url.searchParams.get('page'));
	const [result, options] = await Promise.all([
		listExperiences(event, { page, filters }),
		getFilterOptions(event)
	]);
	return { result, filters, options };
};
