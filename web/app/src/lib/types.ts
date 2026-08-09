/**
 * Wire types for the accepted /api/v1 contract.
 *
 * These mirror adr/claude/json-api-contract-decision.md exactly. Where the
 * earlier fixture-only shapes disagreed with the contract, the contract wins
 * and the reason is noted, because several of those disagreements were not
 * cosmetic.
 */

/** The accepted ten-value set. The first six are the old fixture set. */
export type Round =
	| 'online_assessment'
	| 'aptitude'
	| 'coding'
	| 'technical'
	| 'system_design'
	| 'behavioral'
	| 'managerial'
	| 'group_discussion'
	| 'hr'
	| 'other';

export const ROUND_VALUES: readonly Round[] = [
	'online_assessment',
	'aptitude',
	'coding',
	'technical',
	'system_design',
	'behavioral',
	'managerial',
	'group_discussion',
	'hr',
	'other'
] as const;

export type Outcome = 'offered' | 'rejected' | 'withdrew' | 'unknown';

export type Sort = 'hot' | 'new' | 'top';

export type UserRole = 'user' | 'moderator' | 'administrator';

export type ContentState =
	| 'draft'
	| 'pending_review'
	| 'changes_requested'
	| 'rejected'
	| 'published'
	| 'hidden';

export interface Author {
	username: string;
	display_name: string;
	avatar_url?: string;
}

export interface NamedSlug {
	slug: string;
	name: string;
}

export type Company = NamedSlug;
export type Topic = NamedSlug;
export type JobRole = NamedSlug;

export interface Difficulty {
	/** Null when vote_count is 0. Null is not a difficulty of zero. */
	mean: number | null;
	vote_count: number;
}

export interface QuestionSummary {
	public_id: string;
	slug: string;
	title: string;
	/** Null for company-agnostic questions, which the schema allows. */
	company: Company | null;
	role: JobRole | null;
	round: Round | null;
	source_year: number | null;
	topics: Topic[];
	difficulty: Difficulty;
	published_at: string;
}

export interface Question extends QuestionSummary {
	prompt: string;
	answer_guidance: string | null;
	/** Null means the author chose anonymity, never missing data. */
	author: Author | null;
}

export interface ExperienceRound {
	ordinal: number;
	round: Round;
	notes: string | null;
}

export interface ExperienceSummary {
	public_id: string;
	slug: string;
	title: string;
	company: Company | null;
	role: JobRole | null;
	source_year: number | null;
	/**
	 * When false the API omits `outcome` entirely rather than sending null.
	 *
	 * The old shape had only `outcome: Outcome | null`, which could not tell a
	 * hidden outcome from an unknown one, so a hidden outcome rendered as
	 * "Unknown". That discloses something about the placement the author chose
	 * to withhold, and it matters most for imported records where the student
	 * never consented here at all.
	 */
	outcome_visible: boolean;
	outcome?: Outcome | null;
	author: Author | null;
	published_at: string;
}

export interface Experience extends ExperienceSummary {
	/**
	 * One plain-text string, not a pre-split array. Paragraph splitting is
	 * presentation and happens at render time, so the author's exact text
	 * survives.
	 */
	narrative: string;
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
	total_is_estimate: boolean;
	total_pages: number;
	/** Opaque. Carries the fixed as_of for hot and top. Never parsed here. */
	next_cursor: string | null;
}

export interface SearchHit {
	kind: 'question' | 'experience';
	public_id: string;
	slug: string;
	title: string;
	snippet: string;
	company: Company | null;
	source_year: number | null;
	/** Null for experiences, which have no difficulty. */
	difficulty: Difficulty | null;
}

export type SearchOutcome =
	| { status: 'ok'; results: Page<SearchHit> }
	| { status: 'unavailable' };

export interface FilterOptions {
	companies: Company[];
	roles: JobRole[];
	topics: Topic[];
	years: number[];
	rounds: Round[];
	outcomes: Outcome[];
}

/** Header state. Control flow uses the booleans, never the role string. */
export interface Me {
	public_id: string;
	username: string;
	display_name: string;
	role: UserRole;
	status: 'active' | 'suspended';
	can_submit: boolean;
	can_moderate: boolean;
	unread_moderation_count: number;
}

export interface ApiFieldError {
	field: string;
	code: string;
	message: string;
}

export interface ApiErrorBody {
	error: {
		code: string;
		message: string;
		request_id: string;
		details?: { fields?: ApiFieldError[] };
	};
}

/** Fallback whenever a profile has no avatar or one fails to load. */
export const DEFAULT_AVATAR_URL = '/avatars/default-user.svg';

export interface PublicProfile {
	username: string;
	display_name: string;
	/** Always supplied by the API, already resolved to a servable URL. */
	avatar_url: string;
	join_month: string;
	public_question_count: number;
	public_experience_count: number;
	bio: string | null;
}

export interface AvatarResponse {
	avatar_url: string;
}

export interface Comment {
	/** Stable opaque ID. Internal numeric IDs are never exposed. */
	public_id: string;
	body: string;
	author: Author | null;
	created_at: string;
	/** True when the signed-in user may report it. */
	can_report: boolean;
}

export interface CommentPage {
	items: Comment[];
	/** Opaque cursor. Comments use cursor pagination, never offsets. */
	next_cursor: string | null;
}

export const COMMENT_MIN_LENGTH = 1;
export const COMMENT_MAX_LENGTH = 4000;
