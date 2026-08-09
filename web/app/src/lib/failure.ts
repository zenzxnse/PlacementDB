/**
 * Typed frontend failure classification.
 *
 * Every backend failure lands in exactly one of these kinds, so pages decide
 * what to show from a closed set rather than from an HTTP status scattered
 * through templates.
 *
 * Two rules hold everywhere below. A failure is never rendered as success, and
 * nothing from a response body reaches the user except the API's own safe
 * message and request ID. Raw bodies, stack traces, internal paths, and SQL
 * never appear.
 *
 * This module is shared rather than server-only: templates need the kind to
 * choose wording, and it carries no secret.
 */

export type FailureKind =
	| 'outage'
	| 'timeout'
	| 'malformed'
	| 'validation'
	| 'session_expired'
	| 'forbidden'
	| 'not_found'
	| 'conflict'
	| 'too_large'
	| 'unsupported_media'
	| 'rate_limited'
	| 'dependency';

export interface FailureFieldError {
	field: string;
	message: string;
}

export interface Failure {
	kind: FailureKind;
	/** The API's safe human message, or a written fallback. Never a raw body. */
	message: string;
	/** Present only when the API supplied one. Shown so a user can quote it. */
	requestId?: string;
	/** Seconds, from Retry-After. Guidance only, never a client busy loop. */
	retryAfterSeconds?: number;
	fields?: FailureFieldError[];
	/** True when a human retrying the same page might reasonably succeed. */
	retryable: boolean;
	/** True when this is a service problem rather than the user's request. */
	serviceProblem: boolean;
}

const DEFAULT_MESSAGES: Record<FailureKind, string> = {
	outage: 'We could not reach the service. It may be restarting.',
	timeout: 'The service took too long to answer.',
	malformed: 'The service returned a response we could not read.',
	validation: 'Some details need fixing.',
	session_expired: 'Your session expired. Sign in again to continue.',
	forbidden: 'You do not have access to that.',
	not_found: 'We could not find that.',
	conflict: 'Someone else changed this first.',
	too_large: 'That submission is too large.',
	unsupported_media: 'That format is not supported.',
	rate_limited: 'Too many requests. Please wait a moment.',
	dependency: 'The service is temporarily unavailable.'
};

const RETRYABLE: ReadonlySet<FailureKind> = new Set<FailureKind>([
	'outage',
	'timeout',
	'malformed',
	'dependency',
	'rate_limited',
	'conflict'
]);

const SERVICE_PROBLEM: ReadonlySet<FailureKind> = new Set<FailureKind>([
	'outage',
	'timeout',
	'malformed',
	'dependency'
]);

export function makeFailure(
	kind: FailureKind,
	overrides: Partial<Omit<Failure, 'kind' | 'retryable' | 'serviceProblem'>> = {}
): Failure {
	return {
		kind,
		message: overrides.message?.trim() || DEFAULT_MESSAGES[kind],
		...(overrides.requestId ? { requestId: overrides.requestId } : {}),
		...(overrides.retryAfterSeconds !== undefined
			? { retryAfterSeconds: overrides.retryAfterSeconds }
			: {}),
		...(overrides.fields && overrides.fields.length ? { fields: overrides.fields } : {}),
		retryable: RETRYABLE.has(kind),
		serviceProblem: SERVICE_PROBLEM.has(kind)
	};
}

/** Maps an HTTP status onto a kind. Unknown statuses fail safe as dependency. */
export function kindForStatus(status: number): FailureKind {
	switch (status) {
		case 400:
			return 'validation';
		case 401:
			return 'session_expired';
		case 403:
			return 'forbidden';
		case 404:
			return 'not_found';
		case 409:
			return 'conflict';
		case 413:
			return 'too_large';
		case 415:
			return 'unsupported_media';
		case 429:
			return 'rate_limited';
		default:
			/*
			 * Anything unrecognised is treated as a service problem rather than
			 * as the user's fault. Guessing "validation" here would tell people
			 * to edit input that was fine.
			 */
			return status >= 500 ? 'dependency' : 'dependency';
	}
}

/**
 * Suggested wait in seconds, clamped.
 *
 * Parses the numeric form of Retry-After. The HTTP-date form is not honoured
 * because a clock-skewed client computes a nonsense wait, and the value is only
 * guidance shown to a person.
 */
export function parseRetryAfter(raw: string | null): number | undefined {
	if (!raw) return undefined;
	const seconds = Number(raw.trim());
	if (!Number.isFinite(seconds) || seconds < 0) return undefined;
	return Math.min(Math.ceil(seconds), 3600);
}

/**
 * Builds a retry target that preserves the user's filters and query.
 *
 * Only the path and search of a same-origin URL are kept, so a crafted value
 * cannot turn a retry link into an offsite redirect.
 */
export function safeRetryTarget(url: URL): string {
	const path = url.pathname.startsWith('/') ? url.pathname : '/';
	if (path.startsWith('//')) return '/';
	return `${path}${url.search}`;
}

/** A relevant escape hatch for a failed page. */
export function escapeLinkFor(pathname: string): { href: string; label: string } {
	if (pathname.startsWith('/questions')) return { href: '/questions', label: 'Back to questions' };
	if (pathname.startsWith('/experiences'))
		return { href: '/experiences', label: 'Back to experiences' };
	if (pathname.startsWith('/search')) return { href: '/questions', label: 'Browse questions' };
	if (pathname.startsWith('/moderation'))
		return { href: '/moderation/queue', label: 'Back to the queue' };
	return { href: '/', label: 'Back to home' };
}
