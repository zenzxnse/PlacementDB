export type Round =
	| 'online_assessment'
	| 'technical'
	| 'system_design'
	| 'hr'
	| 'managerial'
	| 'other';

export type Outcome = 'offered' | 'rejected' | 'withdrew' | 'unknown';

export type Sort = 'hot' | 'new' | 'top';

export interface Author {
	username: string;
	display_name: string;
}

export interface Company {
	slug: string;
	name: string;
}

export interface Topic {
	slug: string;
	name: string;
}

export interface Difficulty {
	mean: number | null;
	vote_count: number;
}

export interface Question {
	public_id: string;
	slug: string;
	title: string;
	prompt: string;
	answer_guidance: string | null;
	company: Company;
	role: string;
	round: Round;
	source_year: number;
	topics: Topic[];
	author: Author | null;
	published_at: string;
	difficulty: Difficulty;
}

export interface ExperienceRound {
	name: string;
	round: Round;
	summary: string;
}

export interface Experience {
	public_id: string;
	slug: string;
	title: string;
	company: Company;
	role: string;
	source_year: number;
	outcome: Outcome | null;
	author: Author | null;
	published_at: string;
	summary: string;
	narrative: string[];
	rounds: ExperienceRound[];
}

export interface QuestionFilters {
	company: string[];
	role: string[];
	topic: string[];
	year: string[];
	difficulty: string[];
}

export interface ExperienceFilters {
	company: string[];
	role: string[];
	year: string[];
	outcome: string[];
}

export interface Page<T> {
	items: T[];
	page: number;
	per_page: number;
	total: number;
	total_pages: number;
}

export interface SearchHit {
	kind: 'question' | 'experience';
	title: string;
	url: string;
	snippet: string;
	company: Company;
	year: number;
	difficulty: Difficulty | null;
}

export type SearchOutcome =
	| { status: 'ok'; results: Page<SearchHit> }
	| { status: 'unavailable' };

export interface FilterOptions {
	companies: Company[];
	roles: string[];
	topics: Topic[];
	years: number[];
	outcomes: Outcome[];
}
