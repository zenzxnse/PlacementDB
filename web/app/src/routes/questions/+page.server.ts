import type { PageServerLoad } from './$types';
import { getFilterOptions, listQuestions } from '$lib/server/data';
import { parsePage, parseQuestionFilters, parseSort } from '$lib/query';

export const load: PageServerLoad = ({ url }) => {
	const sort = parseSort(url.searchParams.get('sort'));
	const page = parsePage(url.searchParams.get('page'));
	const filters = parseQuestionFilters(url);

	return {
		result: listQuestions({ sort, page, filters }),
		filters,
		sort,
		options: getFilterOptions()
	};
};
