import type { PageServerLoad } from './$types';
import { getFilterOptions, search } from '$lib/server/data';
import { parsePage, parseQuestionFilters } from '$lib/query';

export const load: PageServerLoad = ({ url }) => {
	const q = (url.searchParams.get('q') ?? '').trim();
	const filters = parseQuestionFilters(url);
	const page = parsePage(url.searchParams.get('page'));

	const submitted = url.searchParams.has('q');
	const outcome = submitted ? search({ q, page, filters }) : null;

	return { q, filters, submitted, outcome, options: getFilterOptions() };
};
