import type { PageServerLoad } from './$types';
import { listExperiences, listQuestions } from '$lib/server/content';
import { toFailure } from '$lib/server/api';
import type { Failure } from '$lib/failure';
import type { ExperienceSummary, QuestionSummary } from '$lib/types';

const EMPTY_QUESTION_FILTERS = { company: [], role: [], topic: [], year: [], difficulty: [] };
const EMPTY_EXPERIENCE_FILTERS = { company: [], role: [], year: [], outcome: [] };

/**
 * A section either has rows or a failure, never both and never a silent empty
 * list. Turning a failure into `[]` would render "nothing here yet" for an
 * outage, which is a lie the operator cannot see.
 */
export interface Section<T> {
	items: T[];
	failure: Failure | null;
}

function settle<T>(result: PromiseSettledResult<T[]>, limit: number): Section<T> {
	if (result.status === 'fulfilled') {
		return { items: result.value.slice(0, limit), failure: null };
	}
	return { items: [], failure: toFailure(result.reason) };
}

export const load: PageServerLoad = async (event) => {
	/*
	 * allSettled, not all. One unavailable surface must not erase the sections
	 * that answered. A reader can still browse hot questions while experiences
	 * are down.
	 */
	const [hot, fresh, recent] = await Promise.allSettled([
		listQuestions(event, { sort: 'hot', page: 1, filters: EMPTY_QUESTION_FILTERS }).then(
			(p) => p.items
		),
		listQuestions(event, { sort: 'new', page: 1, filters: EMPTY_QUESTION_FILTERS }).then(
			(p) => p.items
		),
		listExperiences(event, { page: 1, filters: EMPTY_EXPERIENCE_FILTERS }).then((p) => p.items)
	]);

	return {
		hot: settle<QuestionSummary>(hot, 5),
		fresh: settle<QuestionSummary>(fresh, 5),
		recent: settle<ExperienceSummary>(recent, 4)
	};
};
