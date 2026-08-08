import type { ExperienceFilters, QuestionFilters, Sort } from './types';

const SORTS: Sort[] = ['hot', 'new', 'top'];

export function parseSort(value: string | null, fallback: Sort = 'hot'): Sort {
	return value !== null && (SORTS as string[]).includes(value) ? (value as Sort) : fallback;
}

export function parsePage(value: string | null): number {
	if (value === null) return 1;
	const n = Number.parseInt(value, 10);
	if (!Number.isFinite(n) || n < 1) return 1;
	return n;
}

export function parseRepeated(url: URL, key: string): string[] {
	return url.searchParams
		.getAll(key)
		.map((v) => v.trim())
		.filter((v) => v.length > 0);
}

export function parseQuestionFilters(url: URL): QuestionFilters {
	return {
		company: parseRepeated(url, 'company'),
		role: parseRepeated(url, 'role'),
		topic: parseRepeated(url, 'topic'),
		year: parseRepeated(url, 'year'),
		difficulty: parseRepeated(url, 'difficulty')
	};
}

export function parseExperienceFilters(url: URL): ExperienceFilters {
	return {
		company: parseRepeated(url, 'company'),
		role: parseRepeated(url, 'role'),
		year: parseRepeated(url, 'year'),
		outcome: parseRepeated(url, 'outcome')
	};
}

export function hasActiveFilters(f: QuestionFilters | ExperienceFilters): boolean {
	return Object.values(f).some((values) => values.length > 0);
}

export function buildQuestionQuery(params: {
	sort?: Sort;
	page?: number;
	filters?: QuestionFilters;
}): string {
	const url = new URL('http://x/');
	if (params.sort && params.sort !== 'hot') url.searchParams.set('sort', params.sort);
	appendFilters(url, params.filters);
	if (params.page && params.page > 1) url.searchParams.set('page', String(params.page));
	const qs = url.searchParams.toString();
	return qs.length > 0 ? `?${qs}` : '';
}

function appendFilters(url: URL, filters?: QuestionFilters | ExperienceFilters): void {
	if (!filters) return;
	for (const [key, values] of Object.entries(filters)) {
		for (const v of values) url.searchParams.append(key, v);
	}
}

export function buildSearchQuery(params: {
	q: string;
	page?: number;
	filters?: QuestionFilters;
}): string {
	const url = new URL('http://x/');
	if (params.q.trim().length > 0) url.searchParams.set('q', params.q.trim());
	appendFilters(url, params.filters);
	if (params.page && params.page > 1) url.searchParams.set('page', String(params.page));
	const qs = url.searchParams.toString();
	return qs.length > 0 ? `?${qs}` : '';
}

export function buildExperienceQuery(params: {
	page?: number;
	filters?: ExperienceFilters;
}): string {
	const url = new URL('http://x/');
	appendFilters(url, params.filters);
	if (params.page && params.page > 1) url.searchParams.set('page', String(params.page));
	const qs = url.searchParams.toString();
	return qs.length > 0 ? `?${qs}` : '';
}

