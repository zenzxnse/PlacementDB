import {
	FIXTURE_AS_OF,
	fixtureExperiences,
	fixtureQuestions
} from './fixtures';
import type {
	Company,
	Experience,
	ExperienceFilters,
	FilterOptions,
	Outcome,
	Page,
	Question,
	QuestionFilters,
	SearchOutcome,
	Sort,
	Topic
} from '$lib/types';

export const PER_PAGE = 20;
export const MAX_PAGE = 200;

const AS_OF = Date.parse(FIXTURE_AS_OF);
const DAY_MS = 86_400_000;

function matchesCompany(value: string, company: Company): boolean {
	return value === company.slug || value.toLowerCase() === company.name.toLowerCase();
}

function questionMatchesFilters(q: Question, f: QuestionFilters): boolean {
	if (f.company.length > 0 && !f.company.some((c) => matchesCompany(c, q.company))) return false;
	if (f.role.length > 0 && !f.role.some((r) => r.toLowerCase() === q.role.toLowerCase()))
		return false;
	if (f.topic.length > 0 && !f.topic.some((t) => q.topics.some((qt) => qt.slug === t)))
		return false;
	if (f.year.length > 0 && !f.year.some((y) => y === String(q.source_year))) return false;
	if (f.difficulty.length > 0) {
		const rounded = q.difficulty.mean === null ? null : Math.round(q.difficulty.mean);
		if (rounded === null || !f.difficulty.some((d) => d === String(rounded))) return false;
	}
	return true;
}

function experienceMatchesFilters(e: Experience, f: ExperienceFilters): boolean {
	if (f.company.length > 0 && !f.company.some((c) => matchesCompany(c, e.company))) return false;
	if (f.role.length > 0 && !f.role.some((r) => r.toLowerCase() === e.role.toLowerCase()))
		return false;
	if (f.year.length > 0 && !f.year.some((y) => y === String(e.source_year))) return false;
	if (f.outcome.length > 0 && !f.outcome.some((o) => o === e.outcome)) return false;
	return true;
}

function paginate<T>(all: T[], page: number): Page<T> {
	const total = all.length;
	const total_pages = Math.max(1, Math.ceil(total / PER_PAGE));
	const clamped = Math.min(Math.max(1, page), MAX_PAGE);
	const start = (clamped - 1) * PER_PAGE;
	return {
		items: all.slice(start, start + PER_PAGE),
		page: clamped,
		per_page: PER_PAGE,
		total,
		total_pages
	};
}

function byNew<T extends { published_at: string; public_id: string }>(a: T, b: T): number {
	const time = Date.parse(b.published_at) - Date.parse(a.published_at);
	if (time !== 0) return time;
	return a.public_id < b.public_id ? -1 : 1;
}

function topScore(q: Question): number {
	const ageDays = (AS_OF - Date.parse(q.published_at)) / DAY_MS;
	return q.difficulty.vote_count * 2 + Math.max(0, 30 - ageDays);
}

function hotScore(q: Question): number {
	const ageDays = (AS_OF - Date.parse(q.published_at)) / DAY_MS;
	const activity = q.difficulty.vote_count + 1;
	return activity / Math.pow(ageDays + 2, 1.5);
}

export function sortQuestions(questions: Question[], sort: Sort): Question[] {
	const copy = [...questions];
	switch (sort) {
		case 'new':
			return copy.sort(byNew);
		case 'top':
			return copy.sort(
				(a, b) => topScore(b) - topScore(a) || byNew(a, b)
			);
		case 'hot':
			return copy.sort(
				(a, b) => hotScore(b) - hotScore(a) || byNew(a, b)
			);
	}
}

export function listQuestions(params: {
	sort: Sort;
	page: number;
	filters: QuestionFilters;
}): Page<Question> {
	const filtered = fixtureQuestions.filter((q) => questionMatchesFilters(q, params.filters));
	const sorted = sortQuestions(filtered, params.sort);
	return paginate(sorted, params.page);
}

export function getQuestionBySlug(slug: string): Question | null {
	return fixtureQuestions.find((q) => q.slug === slug) ?? null;
}

export function listExperiences(params: {
	page: number;
	filters: ExperienceFilters;
}): Page<Experience> {
	const filtered = fixtureExperiences.filter((e) =>
		experienceMatchesFilters(e, params.filters)
	);
	const sorted = [...filtered].sort(byNew);
	return paginate(sorted, params.page);
}

export function getExperienceBySlug(slug: string): Experience | null {
	return fixtureExperiences.find((e) => e.slug === slug) ?? null;
}

export function getHomeData(): {
	hot: Question[];
	fresh: Question[];
	recent: Experience[];
} {
	return {
		hot: sortQuestions(fixtureQuestions, 'hot').slice(0, 5),
		fresh: sortQuestions(fixtureQuestions, 'new').slice(0, 5),
		recent: [...fixtureExperiences].sort(byNew).slice(0, 4)
	};
}

export function search(params: {
	q: string;
	page: number;
	filters: QuestionFilters;
}): SearchOutcome {
	const terms = params.q
		.toLowerCase()
		.split(/\s+/)
		.filter((t) => t.length > 0);

	const questionHits = fixtureQuestions
		.filter((q) => questionMatchesFilters(q, params.filters))
		.filter((q) => {
			if (terms.length === 0) return false;
			const haystack = [q.title, q.prompt, q.company.name, q.role, ...q.topics.map((t) => t.name)]
				.join(' ')
				.toLowerCase();
			return terms.every((t) => haystack.includes(t));
		});

	const experienceHits = fixtureExperiences.filter((e) => {
		if (terms.length === 0) return false;
		const haystack = [e.title, e.summary, ...e.narrative, e.company.name, e.role]
			.join(' ')
			.toLowerCase();
		return terms.every((t) => haystack.includes(t));
	});

	const hits = [
		...questionHits.map((q) => ({
			kind: 'question' as const,
			title: q.title,
			url: `/questions/${q.slug}`,
			snippet: snippet(q.prompt, terms),
			company: q.company,
			year: q.source_year,
			difficulty: q.difficulty
		})),
		...experienceHits.map((e) => ({
			kind: 'experience' as const,
			title: e.title,
			url: `/experiences/${e.slug}`,
			snippet: snippet(e.summary, terms),
			company: e.company,
			year: e.source_year,
			difficulty: null
		}))
	];

	hits.sort((a, b) => a.url.localeCompare(b.url));

	return { status: 'ok', results: paginate(hits, params.page) };
}

function snippet(text: string, terms: string[]): string {
	if (terms.length === 0) return text.slice(0, 160);
	const lower = text.toLowerCase();
	let best = 0;
	for (const t of terms) {
		const idx = lower.indexOf(t);
		if (idx >= 0) {
			best = idx;
			break;
		}
	}
	const start = Math.max(0, best - 40);
	const raw = text.slice(start, start + 180);
	const prefix = start > 0 ? '...' : '';
	const suffix = start + 180 < text.length ? '...' : '';
	return `${prefix}${raw.trim()}${suffix}`;
}

export function getFilterOptions(): FilterOptions {
	const companies = new Map<string, Company>();
	const roles = new Set<string>();
	const topics = new Map<string, Topic>();
	const years = new Set<number>();
	const outcomes = new Set<Outcome>();

	for (const q of fixtureQuestions) {
		companies.set(q.company.slug, q.company);
		roles.add(q.role);
		years.add(q.source_year);
		for (const t of q.topics) topics.set(t.slug, t);
	}
	for (const e of fixtureExperiences) {
		companies.set(e.company.slug, e.company);
		roles.add(e.role);
		years.add(e.source_year);
		if (e.outcome !== null) outcomes.add(e.outcome);
	}

	return {
		companies: [...companies.values()].sort((a, b) => a.name.localeCompare(b.name)),
		roles: [...roles].sort(),
		topics: [...topics.values()].sort((a, b) => a.name.localeCompare(b.name)),
		years: [...years].sort((a, b) => b - a),
		outcomes: [...outcomes]
	};
}
