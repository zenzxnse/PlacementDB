import type { PageServerLoad } from './$types';
import { getFilterOptions, listExperiences } from '$lib/server/data';
import { parseExperienceFilters, parsePage } from '$lib/query';

export const load: PageServerLoad = ({ url }) => {
	const page = parsePage(url.searchParams.get('page'));
	const filters = parseExperienceFilters(url);

	return {
		result: listExperiences({ page, filters }),
		filters,
		options: getFilterOptions()
	};
};
