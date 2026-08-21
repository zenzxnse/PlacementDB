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

export interface LookupCount extends NamedSlug {
	question_count: number;
	experience_count: number;
}

export interface LookupCountPage {
	items: LookupCount[];
}

export interface Difficulty {
	/*
	 * Never null. Zero votes yields 3.0 (the prior), per the 2026-08-11
	 * weighted-scoring decision. vote_count is the honest disclosure that
	 * tells the reader whether the mean reflects community placement or the
	 * default.
	 */
	mean: number;
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

/**
 * Outcome visibility, encoded so the wrong shape cannot compile.
 *
 * When visibility is false the API omits `outcome` entirely rather than
 * sending null, and this union makes that structural: there is no `outcome`
 * property to read on the hidden branch, so a template cannot accidentally
 * render one. The old shape was `outcome_visible: boolean` plus
 * `outcome?: Outcome | null`, which could not tell a hidden outcome from an
 * unknown one, and a hidden outcome rendered as "Unknown". That discloses
 * something the author chose to withhold, and it matters most for imported
 * records where the student never consented here at all.
 */
export type OutcomeVisibility =
	| { outcome_visible: true; outcome: Outcome }
	| { outcome_visible: false };

export type ExperienceSummary = {
	public_id: string;
	slug: string;
	title: string;
	company: Company | null;
	role: JobRole | null;
	source_year: number | null;
	author: Author | null;
	published_at: string;
} & OutcomeVisibility;

export type Experience = ExperienceSummary & {
	/**
	 * One plain-text string, not a pre-split array. Paragraph splitting is
	 * presentation and happens at render time, so the author's exact text
	 * survives.
	 */
	narrative: string;
	rounds: ExperienceRound[];
};

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

/**
 * Response to PUT /questions/{public_id}/difficulty.
 *
 * `my_vote` appears only here. No read route exposes the caller's own vote, so
 * a freshly loaded page cannot pre-select the radio the user last chose; it
 * only knows after a vote in the same session. That is a backend gap, recorded
 * rather than papered over with a guess.
 */
export interface DifficultyVoteResult {
	difficulty: Difficulty;
	my_vote: number;
}

/** The accepted 1-5 scale. Shared by the vote control and its validation. */
export const DIFFICULTY_MIN = 1;
export const DIFFICULTY_MAX = 5;

export const COMMENT_MIN_LENGTH = 1;
export const COMMENT_MAX_LENGTH = 4000;

/**
 * Accepted reasons for POST /comments/{public_id}/reports, from the accepted
 * comments/moderation contract. The moderation report read echoes the same
 * vocabulary, so the labels serve both surfaces.
 */
export type ReportReason =
	| 'spam'
	| 'offensive'
	| 'duplicate'
	| 'incorrect'
	| 'personal_info'
	| 'other';

export const REPORT_REASON_VALUES: readonly ReportReason[] = [
	'spam',
	'offensive',
	'duplicate',
	'incorrect',
	'personal_info',
	'other'
] as const;

export const REPORT_REASON_LABELS: Record<ReportReason, string> = {
	spam: 'Spam',
	offensive: 'Offensive',
	duplicate: 'Duplicate',
	incorrect: 'Incorrect',
	personal_info: 'Personal information',
	other: 'Something else'
};

/** Report details and every moderation reason share this ceiling. */
export const REPORT_REASON_MAX_LENGTH = 1000;

/** States a content report moves through, per the accepted contract. */
export type ReportState = 'open' | 'under_review' | 'resolved' | 'dismissed';

export const REPORT_STATE_VALUES: readonly ReportState[] = [
	'open',
	'under_review',
	'resolved',
	'dismissed'
] as const;

/**
 * Outcome of a comment report or moderator hide, carried from the form action
 * back to the comment it names. Lives here rather than in server code because
 * the Comments component renders it, and $lib/server must stay unreachable
 * from components.
 */
export interface CommentActionOutcome {
	/** Which comment the outcome belongs to, so the UI anchors it there. */
	commentId: string;
	kind: 'reported' | 'hidden' | 'error';
	message: string;
	/** Present only on the expired-session branch. */
	loginHref?: string;
}

/** One row of GET /moderation/reports, per the accepted contract. */
export interface ModerationReportItem {
	public_id: string;
	target_type: string;
	reason: string;
	details: string | null;
	state: ReportState;
	reporter_label: string;
	created_at: string;
}

export interface ModerationReportPage {
	items: ModerationReportItem[];
	next_cursor: string | null;
}

export interface AccountSubmissionItem {
	kind: 'question' | 'experience';
	public_id: string;
	slug: string;
	title: string;
	state: string;
	updated_at: string;
}

export interface AccountVoteItem {
	value: number;
	updated_at: string;
	target: { public_id: string; slug: string; title: string } | null;
}

export interface AccountReportItem {
	public_id: string;
	target_type: 'question' | 'experience' | 'user' | 'comment';
	reason: string;
	details: string | null;
	state: ReportState;
	created_at: string;
}

export interface CursorPage<T> {
	items: T[];
	next_cursor: string | null;
}

export interface ContentReportOutcome {
	kind: 'reported' | 'error';
	message: string;
	loginHref?: string;
}
