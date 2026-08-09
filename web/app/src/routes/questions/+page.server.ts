import type { PageServerLoad } from './$types';
import { getFilterOptions, listQuestions } from '$lib/server/content';
import { parseQuestionFilters, parsePage, parseSort } from '$lib/query';

export const load: PageServerLoad = async (event) => {
	const filters = parseQuestionFilters(event.url);
	const sort = parseSort(event.url.searchParams.get('sort'));
	const page = parsePage(event.url.searchParams.get('page'));
	const [result, options] = await Promise.all([
		listQuestions(event, { sort, page, filters }),
		getFilterOptions(event)
	]);
	return { result, filters, sort, options };
};
