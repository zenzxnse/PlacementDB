import type { PageServerLoad } from './$types';
import { getFilterOptions, search } from '$lib/server/content';
import { parsePage, parseQuestionFilters } from '$lib/query';

export const load: PageServerLoad = async (event) => {
	const q = (event.url.searchParams.get('q') ?? '').trim();
	const submitted = event.url.searchParams.has('q');
	const filters = parseQuestionFilters(event.url);
	const page = parsePage(event.url.searchParams.get('page'));
	const options = await getFilterOptions(event);
	const outcome = submitted && q
		? await search(event, { q, page, filters })
		: ({ status: 'ok', results: { items: [], page: 1, per_page: 20, total: 0, total_is_estimate: false, total_pages: 0, next_cursor: null } } as const);
	return { q, filters, submitted, outcome, options };
};
