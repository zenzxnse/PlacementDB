import {
	ROUND_VALUES,
	type Author,
	type Comment,
	type CommentPage,
	type Company,
	type Difficulty,
	type DifficultyVoteResult,
	type Experience,
	type ExperienceRound,
	type ExperienceSummary,
	type FilterOptions,
	type JobRole,
	type LookupCountPage,
	type Me,
	type NamedSlug,
	type Outcome,
	type Page,
	type PublicProfile,
	type Question,
	type QuestionSummary,
	type Round,
	type SearchHit,
	type Topic,
	type UserRole
} from './types';

/**
 * Runtime validation for the API boundary.
 *
 * TypeScript erases at runtime, so `JSON.parse(raw) as Question` is a promise,
 * not a check. A Drogon response that does not match the declared type used to
 * flow through as a lie and surface much later as an unrelated render error,
 * usually somewhere that had nothing to do with the mismatch. These parsers
 * turn that into one honest failure at the seam it came from.
 *
 * Rules that shaped this file:
 *
 * - **Unknown fields are allowed and preserved.** The wire contract is
 *   additively versioned: a new field the backend starts sending must not take
 *   the site down. Only declared fields are checked.
 * - **Numbers must be finite.** NaN and Infinity are not JSON, but a
 *   misbehaving proxy or a hand-rolled serializer can still produce them
 *   through a lenient parser, and they poison every arithmetic path downstream.
 * - **Errors carry a path, not the value.** A response body can contain other
 *   people's content; the path says `items[3].difficulty.mean` and stops there.
 *   `ApiMalformed` keeps even that internal.
 * - **No dependency.** Hand-written per wire type, per the dependency gate.
 */

export class WireError extends Error {
	constructor(readonly path: string, readonly detail: string) {
		super(`${path || 'response'}: ${detail}`);
		this.name = 'WireError';
	}
}

export type Parser<T> = (value: unknown, path?: string) => T;

function isRecord(value: unknown): value is Record<string, unknown> {
	return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function record(value: unknown, path: string): Record<string, unknown> {
	if (!isRecord(value)) throw new WireError(path, 'expected an object');
	return value;
}

function str(value: unknown, path: string): string {
	if (typeof value !== 'string') throw new WireError(path, 'expected a string');
	return value;
}

function num(value: unknown, path: string): number {
	if (typeof value !== 'number' || !Number.isFinite(value)) {
		throw new WireError(path, 'expected a finite number');
	}
	return value;
}

function int(value: unknown, path: string): number {
	const parsed = num(value, path);
	if (!Number.isInteger(parsed)) throw new WireError(path, 'expected an integer');
	return parsed;
}

function bool(value: unknown, path: string): boolean {
	if (typeof value !== 'boolean') throw new WireError(path, 'expected a boolean');
	return value;
}

/** Absent and null are the same thing to a reader; both become null. */
function nullable<T>(parse: (value: unknown, path: string) => T) {
	return (value: unknown, path: string): T | null =>
		value === null || value === undefined ? null : parse(value, path);
}

function arrayOf<T>(parse: (value: unknown, path: string) => T) {
	return (value: unknown, path: string): T[] => {
		if (!Array.isArray(value)) throw new WireError(path, 'expected an array');
		return value.map((item, index) => parse(item, `${path}[${index}]`));
	};
}

function oneOf<T extends string>(allowed: readonly T[]) {
	return (value: unknown, path: string): T => {
		const text = str(value, path);
		if (!(allowed as readonly string[]).includes(text)) {
			throw new WireError(path, 'value outside the accepted set');
		}
		return text as T;
	};
}

const ROLE_VALUES: readonly UserRole[] = ['user', 'moderator', 'administrator'];
const OUTCOME_VALUES: readonly Outcome[] = ['offered', 'rejected', 'withdrew', 'unknown'];

const parseRound = oneOf<Round>(ROUND_VALUES);
const parseOutcome = oneOf<Outcome>(OUTCOME_VALUES);

function parseNamedSlug(value: unknown, path: string): NamedSlug {
	const object = record(value, path);
	return { slug: str(object.slug, `${path}.slug`), name: str(object.name, `${path}.name`) };
}

const parseCompany: (value: unknown, path: string) => Company = parseNamedSlug;
const parseTopic: (value: unknown, path: string) => Topic = parseNamedSlug;
const parseJobRole: (value: unknown, path: string) => JobRole = parseNamedSlug;

export function parseLookupCountPage(value: unknown, path = 'lookup'): LookupCountPage {
	const object = record(value, path);
	return {
		items: arrayOf((item, itemPath) => {
			const row = record(item, itemPath);
			return {
				slug: str(row.slug, `${itemPath}.slug`),
				name: str(row.name, `${itemPath}.name`),
				question_count: int(row.question_count, `${itemPath}.question_count`),
				experience_count: int(row.experience_count, `${itemPath}.experience_count`)
			};
		})(object.items, `${path}.items`)
	};
}

function parseAuthor(value: unknown, path: string): Author {
	const object = record(value, path);
	const author: Author = {
		username: str(object.username, `${path}.username`),
		display_name: str(object.display_name, `${path}.display_name`)
	};
	if (object.avatar_url !== undefined && object.avatar_url !== null) {
		author.avatar_url = str(object.avatar_url, `${path}.avatar_url`);
	}
	return author;
}

function parseDifficulty(value: unknown, path: string): Difficulty {
	const object = record(value, path);
	/*
	 * `mean` is never null: an unrated question scores exactly 3.0 through the
	 * weighted prior. A null here is a backend that has not been updated, and
	 * treating it as 0 would render "Fundamentals" for an unrated question.
	 */
	return {
		mean: num(object.mean, `${path}.mean`),
		vote_count: int(object.vote_count, `${path}.vote_count`)
	};
}

function parseQuestionSummary(value: unknown, path: string): QuestionSummary {
	const object = record(value, path);
	return {
		public_id: str(object.public_id, `${path}.public_id`),
		slug: str(object.slug, `${path}.slug`),
		title: str(object.title, `${path}.title`),
		company: nullable(parseCompany)(object.company, `${path}.company`),
		role: nullable(parseJobRole)(object.role, `${path}.role`),
		round: nullable(parseRound)(object.round, `${path}.round`),
		source_year: nullable(int)(object.source_year, `${path}.source_year`),
		topics: arrayOf(parseTopic)(object.topics ?? [], `${path}.topics`),
		difficulty: parseDifficulty(object.difficulty, `${path}.difficulty`),
		published_at: str(object.published_at, `${path}.published_at`)
	};
}

export function parseQuestion(value: unknown, path = 'question'): Question {
	const object = record(value, path);
	return {
		...parseQuestionSummary(value, path),
		prompt: str(object.prompt, `${path}.prompt`),
		answer_guidance: nullable(str)(object.answer_guidance, `${path}.answer_guidance`),
		author: nullable(parseAuthor)(object.author, `${path}.author`)
	};
}

/**
 * Outcome visibility, as a discriminated pair.
 *
 * When `outcome_visible` is false the API omits `outcome` entirely, and this
 * parser drops it if it arrives anyway. That is deliberate: a withheld outcome
 * that leaks through as a value would render as real, and for imported records
 * the student never consented to it being here at all.
 */
function parseVisibility(
	object: Record<string, unknown>,
	path: string
): { outcome_visible: true; outcome: Outcome } | { outcome_visible: false } {
	const visible = bool(object.outcome_visible, `${path}.outcome_visible`);
	if (!visible) return { outcome_visible: false };
	return { outcome_visible: true, outcome: parseOutcome(object.outcome, `${path}.outcome`) };
}

function parseExperienceSummary(value: unknown, path: string): ExperienceSummary {
	const object = record(value, path);
	return {
		public_id: str(object.public_id, `${path}.public_id`),
		slug: str(object.slug, `${path}.slug`),
		title: str(object.title, `${path}.title`),
		company: nullable(parseCompany)(object.company, `${path}.company`),
		role: nullable(parseJobRole)(object.role, `${path}.role`),
		source_year: nullable(int)(object.source_year, `${path}.source_year`),
		author: nullable(parseAuthor)(object.author, `${path}.author`),
		published_at: str(object.published_at, `${path}.published_at`),
		...parseVisibility(object, path)
	};
}

function parseExperienceRound(value: unknown, path: string): ExperienceRound {
	const object = record(value, path);
	return {
		ordinal: int(object.ordinal, `${path}.ordinal`),
		round: parseRound(object.round, `${path}.round`),
		notes: nullable(str)(object.notes, `${path}.notes`)
	};
}

export function parseExperience(value: unknown, path = 'experience'): Experience {
	const object = record(value, path);
	return {
		...parseExperienceSummary(value, path),
		narrative: str(object.narrative, `${path}.narrative`),
		rounds: arrayOf(parseExperienceRound)(object.rounds ?? [], `${path}.rounds`)
	};
}

export function pageOf<T>(parseItem: (value: unknown, path: string) => T): Parser<Page<T>> {
	return (value: unknown, path = 'page'): Page<T> => {
		const object = record(value, path);
		return {
			items: arrayOf(parseItem)(object.items, `${path}.items`),
			page: int(object.page, `${path}.page`),
			per_page: int(object.per_page, `${path}.per_page`),
			total: int(object.total, `${path}.total`),
			total_is_estimate: bool(object.total_is_estimate, `${path}.total_is_estimate`),
			total_pages: int(object.total_pages, `${path}.total_pages`),
			next_cursor: nullable(str)(object.next_cursor, `${path}.next_cursor`)
		};
	};
}

export const parseQuestionPage = pageOf(parseQuestionSummary);
export const parseExperiencePage = pageOf(parseExperienceSummary);

function parseSearchHit(value: unknown, path: string): SearchHit {
	const object = record(value, path);
	const kind = str(object.kind, `${path}.kind`);
	if (kind !== 'question' && kind !== 'experience') {
		throw new WireError(`${path}.kind`, 'value outside the accepted set');
	}
	return {
		kind,
		public_id: str(object.public_id, `${path}.public_id`),
		slug: str(object.slug, `${path}.slug`),
		title: str(object.title, `${path}.title`),
		snippet: str(object.snippet, `${path}.snippet`),
		company: nullable(parseCompany)(object.company, `${path}.company`),
		source_year: nullable(int)(object.source_year, `${path}.source_year`),
		difficulty: nullable(parseDifficulty)(object.difficulty, `${path}.difficulty`)
	};
}

export const parseSearchPage = pageOf(parseSearchHit);

export function parseFilterOptions(value: unknown, path = 'filter-options'): FilterOptions {
	const object = record(value, path);
	return {
		companies: arrayOf(parseCompany)(object.companies ?? [], `${path}.companies`),
		roles: arrayOf(parseJobRole)(object.roles ?? [], `${path}.roles`),
		topics: arrayOf(parseTopic)(object.topics ?? [], `${path}.topics`),
		years: arrayOf(int)(object.years ?? [], `${path}.years`),
		rounds: arrayOf(parseRound)(object.rounds ?? [], `${path}.rounds`),
		outcomes: arrayOf(parseOutcome)(object.outcomes ?? [], `${path}.outcomes`)
	};
}

/**
 * `/me`, which answers null for an anonymous caller.
 *
 * The capability booleans are required, not optional. If the API stops sending
 * them, failing here is right: defaulting them to false would silently hide
 * controls from people who have the capability, and defaulting them to true
 * would offer controls the API then refuses.
 */
export function parseMe(value: unknown, path = 'me'): Me | null {
	if (value === null || value === undefined) return null;
	const object = record(value, path);
	const status = str(object.status, `${path}.status`);
	if (status !== 'active' && status !== 'suspended') {
		throw new WireError(`${path}.status`, 'value outside the accepted set');
	}
	return {
		public_id: str(object.public_id, `${path}.public_id`),
		username: str(object.username, `${path}.username`),
		display_name: str(object.display_name, `${path}.display_name`),
		role: oneOf<UserRole>(ROLE_VALUES)(object.role, `${path}.role`),
		status,
		can_submit: bool(object.can_submit, `${path}.can_submit`),
		can_moderate: bool(object.can_moderate, `${path}.can_moderate`),
		unread_moderation_count: int(
			object.unread_moderation_count ?? 0,
			`${path}.unread_moderation_count`
		)
	};
}

export function parseProfile(value: unknown, path = 'profile'): PublicProfile {
	const object = record(value, path);
	return {
		username: str(object.username, `${path}.username`),
		display_name: str(object.display_name, `${path}.display_name`),
		avatar_url: str(object.avatar_url, `${path}.avatar_url`),
		join_month: str(object.join_month, `${path}.join_month`),
		public_question_count: int(object.public_question_count, `${path}.public_question_count`),
		public_experience_count: int(
			object.public_experience_count,
			`${path}.public_experience_count`
		),
		bio: nullable(str)(object.bio, `${path}.bio`)
	};
}

function parseComment(value: unknown, path: string): Comment {
	const object = record(value, path);
	return {
		public_id: str(object.public_id, `${path}.public_id`),
		body: str(object.body, `${path}.body`),
		author: nullable(parseAuthor)(object.author, `${path}.author`),
		created_at: str(object.created_at, `${path}.created_at`),
		can_report: bool(object.can_report ?? false, `${path}.can_report`)
	};
}

export function parseCommentPage(value: unknown, path = 'comments'): CommentPage {
	const object = record(value, path);
	return {
		items: arrayOf(parseComment)(object.items ?? [], `${path}.items`),
		next_cursor: nullable(str)(object.next_cursor, `${path}.next_cursor`)
	};
}

export function parseCsrf(value: unknown, path = 'csrf'): { csrf_token: string } {
	const object = record(value, path);
	const token = str(object.csrf_token, `${path}.csrf_token`);
	/*
	 * An empty token is treated as no token everywhere in the app, so accepting
	 * one here would render a form that is guaranteed to be refused.
	 */
	if (token.length === 0) throw new WireError(`${path}.csrf_token`, 'empty token');
	return { csrf_token: token };
}

export function parseDifficultyVote(value: unknown, path = 'vote'): DifficultyVoteResult {
	const object = record(value, path);
	const vote = int(object.my_vote, `${path}.my_vote`);
	if (vote < 1 || vote > 5) throw new WireError(`${path}.my_vote`, 'outside the 1 to 5 scale');
	return { difficulty: parseDifficulty(object.difficulty, `${path}.difficulty`), my_vote: vote };
}

export function parseAvatar(value: unknown, path = 'avatar'): { avatar_url: string } {
	const object = record(value, path);
	return { avatar_url: str(object.avatar_url, `${path}.avatar_url`) };
}

export interface SubmitReceipt {
	public_id: string;
	slug: string;
	state: string;
	updated_at: string;
}

export function parseSubmitReceipt(value: unknown, path = 'receipt'): SubmitReceipt {
	const object = record(value, path);
	return {
		public_id: str(object.public_id, `${path}.public_id`),
		slug: str(object.slug, `${path}.slug`),
		state: str(object.state, `${path}.state`),
		updated_at: str(object.updated_at, `${path}.updated_at`)
	};
}

/** For calls whose body carries nothing the caller uses, such as logout. */
export function parseIgnored(): null {
	return null;
}
