import type { RequestEvent } from '@sveltejs/kit';
import { apiCall, ApiError, ApiUnavailable } from './api';
import { USE_FIXTURES } from './config';
import {
	parseCommentPage,
	parseExperience,
	parseExperiencePage,
	parseFilterOptions,
	parseMe,
	parseLookupCountPage,
	parseProfile,
	parseQuestion,
	parseQuestionPage,
	parseSearchPage
} from '$lib/wire';
import * as fixtures from './data';
import type {
	Experience,
	ExperienceFilters,
	FilterOptions,
	Me,
	Page,
	Question,
	QuestionFilters,
	QuestionSummary,
	ExperienceSummary,
	SearchOutcome,
	Sort,
	PublicProfile,
	CommentPage
	, LookupCountPage
} from '$lib/types';
import { DEFAULT_AVATAR_URL } from '$lib/types';

/**
 * Resource layer between loaders and the API.
 *
 * Every function here either returns real API data or fails. The fixture path
 * is taken only when USE_FIXTURES is on, which requires a dev build, and it is
 * chosen up front rather than as a catch handler.
 *
 * Falling back to fixtures after an API error is deliberately not done. It
 * would show synthetic placement content that looks real, at exactly the
 * moment the operator most needs to see that the API is down.
 */

type Event = Pick<RequestEvent, 'cookies' | 'fetch' | 'request'>;

export async function listLookupCounts(
	event: Event,
	target: 'topics' | 'companies'
): Promise<LookupCountPage> {
	if (USE_FIXTURES) return { items: [] };
	return apiCall(event, `/${target}`, { parse: parseLookupCountPage });
}

function queryString(params: Record<string, string | number | undefined>, repeated: Record<string, string[]> = {}): string {
	const search = new URLSearchParams();
	for (const [key, value] of Object.entries(params)) {
		if (value !== undefined && value !== '') search.set(key, String(value));
	}
	/* Repeated parameters, per the accepted filter contract: topic=a&topic=b. */
	for (const [key, values] of Object.entries(repeated)) {
		for (const value of values) {
			if (value) search.append(key, value);
		}
	}
	const rendered = search.toString();
	return rendered ? `?${rendered}` : '';
}

export async function listQuestions(
	event: Event,
	params: { sort: Sort; page: number; filters: QuestionFilters }
): Promise<Page<QuestionSummary>> {
	if (USE_FIXTURES) return fixtures.listQuestions(params);
	return apiCall(
		event,
		`/questions${queryString(
			{ sort: params.sort, page: params.page },
			{
				company: params.filters.company,
				role: params.filters.role,
				topic: params.filters.topic,
				year: params.filters.year,
				difficulty: params.filters.difficulty
			}
		)}`,
		{ parse: parseQuestionPage }
	);
}

export async function getQuestion(event: Event, slug: string): Promise<Question | null> {
	if (USE_FIXTURES) return fixtures.getQuestionBySlug(slug);
	try {
		return await apiCall(event, `/questions/by-slug/${encodeURIComponent(slug)}`, {
			parse: parseQuestion
		});
	} catch (error) {
		/* NOT_FOUND covers absent and invisible alike, by contract. */
		if (error instanceof ApiError && error.status === 404) return null;
		throw error;
	}
}

export async function listExperiences(
	event: Event,
	params: { page: number; filters: ExperienceFilters }
): Promise<Page<ExperienceSummary>> {
	if (USE_FIXTURES) return fixtures.listExperiences(params);
	return apiCall(
		event,
		`/experiences${queryString(
			{ page: params.page },
			{
				company: params.filters.company,
				role: params.filters.role,
				year: params.filters.year,
				outcome: params.filters.outcome
			}
		)}`,
		{ parse: parseExperiencePage }
	);
}

export async function getExperience(event: Event, slug: string): Promise<Experience | null> {
	if (USE_FIXTURES) return fixtures.getExperienceBySlug(slug);
	try {
		return await apiCall(event, `/experiences/by-slug/${encodeURIComponent(slug)}`, {
			parse: parseExperience
		});
	} catch (error) {
		if (error instanceof ApiError && error.status === 404) return null;
		throw error;
	}
}

export async function search(
	event: Event,
	params: { q: string; page: number; filters: QuestionFilters }
): Promise<SearchOutcome> {
	if (USE_FIXTURES) return fixtures.search(params);
	try {
		const results = await apiCall(
			event,
			`/search${queryString(
				{ q: params.q, page: params.page },
				{
					company: params.filters.company,
					role: params.filters.role,
					topic: params.filters.topic,
					year: params.filters.year,
					difficulty: params.filters.difficulty
				}
			)}`,
			{ parse: parseSearchPage }
		);
		return { status: 'ok', results };
	} catch (error) {
		/**
		 * Search degrades to a browse fallback, which is the one accepted
		 * exception. It never serves stale or synthetic results, because doing
		 * so could show content that moderation has since hidden.
		 */
		const unavailable =
			error instanceof ApiUnavailable ||
			(error instanceof ApiError &&
				(error.code === 'SEARCH_UNAVAILABLE' || error.status >= 500));
		if (unavailable) return { status: 'unavailable' };
		throw error;
	}
}

export async function getFilterOptions(event: Event): Promise<FilterOptions> {
	if (USE_FIXTURES) return fixtures.getFilterOptions();
	return apiCall(event, '/meta/filter-options', { parse: parseFilterOptions });
}

/**
 * Builds a login URL that returns the user to where they were.
 *
 * Used when an authenticated action meets an expired session. Public pages
 * never call this: they simply continue anonymously.
 */
export function loginWithReturn(pathAndQuery: string): string {
	const safe =
		pathAndQuery.startsWith('/') && !pathAndQuery.startsWith('//') ? pathAndQuery : '/';
	return `/login?next=${encodeURIComponent(safe)}`;
}

/**
 * Current user for the header, or null when anonymous.
 *
 * The contract returns 200 with a null body for anonymous callers rather than
 * 401, because anonymous browsing is the common case and 401 would make every
 * such page load look like a failure in logs.
 *
 * A failure here must not take down a public page, so an unreachable API
 * yields an anonymous header rather than an error. Authorization still lives
 * in the API, so rendering an anonymous header can never grant access.
 */
export async function getMe(event: Event): Promise<Me | null> {
	if (USE_FIXTURES) return null;
	try {
		return await apiCall(event, '/me', { parse: parseMe });
	} catch (error) {
		if (error instanceof ApiError && (error.status === 401 || error.status === 404)) {
			return null;
		}
		if (error instanceof ApiUnavailable) return null;
		throw error;
	}
}

/**
 * Public profile.
 *
 * Codex has not published these routes yet, so the mocked branch stands in.
 * It is gated on USE_FIXTURES exactly like every other read, which means a
 * production deployment cannot serve a mocked profile: it fails instead.
 */
export async function getProfile(event: Event, username: string): Promise<PublicProfile | null> {
	if (USE_FIXTURES) return mockProfile(username);
	try {
		return await apiCall(event, `/users/${encodeURIComponent(username)}`, {
			parse: parseProfile
		});
	} catch (error) {
		if (error instanceof ApiError && error.status === 404) return null;
		throw error;
	}
}

export async function listComments(
	event: Event,
	target: 'questions' | 'experiences',
	slug: string,
	cursor?: string
): Promise<CommentPage> {
	if (USE_FIXTURES) return { items: [], next_cursor: null };
	const query = cursor ? `?after=${encodeURIComponent(cursor)}` : '';
	return apiCall(event, `/${target}/by-slug/${encodeURIComponent(slug)}/comments${query}`, {
		parse: parseCommentPage
	});
}

function mockProfile(username: string): PublicProfile {
	return {
		username,
		display_name: username,
		avatar_url: DEFAULT_AVATAR_URL,
		join_month: '2026-01',
		public_question_count: 0,
		public_experience_count: 0,
		bio: null
	};
}
