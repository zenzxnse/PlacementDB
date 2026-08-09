import type { Cookies, RequestEvent } from '@sveltejs/kit';
import { API_BASE, LOGIN_TIMEOUT_MS, PUBLIC_ORIGIN, READ_TIMEOUT_MS } from './config';
import type { ApiErrorBody, ApiFieldError } from '$lib/types';
import { kindForStatus, makeFailure, parseRetryAfter, type Failure } from '$lib/failure';

/**
 * Typed server-side client for Drogon.
 *
 * Only SvelteKit server code calls this. The browser never reaches Drogon
 * directly, so moderation and authorization are always enforced on the API
 * side and no credential reaches client JavaScript.
 */

/** Guards against a runaway upstream filling memory on one request. */
const MAX_RESPONSE_BYTES = 4 * 1024 * 1024;

/** Uploads are slower than reads: a 2 MiB image plus decoding upstream. */
const UPLOAD_TIMEOUT_MS = 30_000;

/** Statuses worth one retry: the upstream is likely mid-restart. */
function isTransient(status: number): boolean {
	return status === 502 || status === 503 || status === 504;
}

function sleep(ms: number): Promise<void> {
	return new Promise((resolve) => setTimeout(resolve, ms));
}

/** Cookie names the API owns. SvelteKit forwards them and interprets neither. */
const FORWARDED_COOKIES = [
	'__Host-placedb_session',
	'placedb_session',
	'__Host-placedb_login_csrf',
	'placedb_login_csrf'
] as const;

export class ApiError extends Error {
	constructor(
		readonly status: number,
		readonly code: string,
		message: string,
		readonly requestId: string,
		readonly fields: ApiFieldError[] = [],
		readonly retryAfterSeconds?: number
	) {
		super(message);
		this.name = 'ApiError';
	}

	/** Classification for pages. Preserves only the safe message and request ID. */
	toFailure(): Failure {
		/* SEARCH_UNAVAILABLE is a dependency problem whatever status carries it. */
		const kind =
			this.code === 'SEARCH_UNAVAILABLE' ? 'dependency' : kindForStatus(this.status);
		return makeFailure(kind, {
			message: this.message,
			requestId: this.requestId,
			retryAfterSeconds: this.retryAfterSeconds,
			fields: this.fields.map((f) => ({ field: f.field, message: f.message }))
		});
	}

	/** True when the API was reachable but refused, as opposed to being down. */
	get isRefusal(): boolean {
		return this.status > 0 && this.status < 500;
	}

	fieldMessage(field: string): string | undefined {
		return this.fields.find((f) => f.field === field)?.message;
	}
}

export class ApiUnavailable extends Error {
	constructor(
		readonly cause_: unknown,
		readonly timedOut = false
	) {
		super('The API is unavailable.');
		this.name = 'ApiUnavailable';
	}

	toFailure(): Failure {
		return makeFailure(this.timedOut ? 'timeout' : 'outage');
	}
}

/** Thrown when a 200 response is not usable JSON. Never a success. */
export class ApiMalformed extends Error {
	constructor(readonly detail: string) {
		super('The service returned an unreadable response.');
		this.name = 'ApiMalformed';
	}

	toFailure(): Failure {
		/* detail stays internal: it can quote a raw body fragment. */
		return makeFailure('malformed');
	}
}

/** Any failure this client can raise. */
export type ApiFailure = ApiError | ApiUnavailable | ApiMalformed;

export function toFailure(error: unknown): Failure {
	if (
		error instanceof ApiError ||
		error instanceof ApiUnavailable ||
		error instanceof ApiMalformed
	) {
		return error.toFailure();
	}
	return makeFailure('dependency');
}

interface CallOptions {
	method?: 'GET' | 'POST' | 'PUT' | 'DELETE' | 'PATCH';
	body?: unknown;
	timeoutMs?: number;
	/** Extra headers, used for the CSRF token on mutations. */
	headers?: Record<string, string>;
}

function buildCookieHeader(cookies: Cookies): string {
	const parts: string[] = [];
	for (const name of FORWARDED_COOKIES) {
		const value = cookies.get(name);
		if (value !== undefined) {
			parts.push(`${name}=${value}`);
		}
	}
	return parts.join('; ');
}

/**
 * Copies Drogon's Set-Cookie headers to the browser response unchanged.
 *
 * SvelteKit never mints, renames, re-signs, or re-scopes the session. Drogon
 * is the sole session authority, so rewriting anything here would silently
 * move that authority and break instant revocation.
 *
 * setHeaders cannot emit Set-Cookie, so this parses each header and replays it
 * through cookies.set with the attributes the API chose.
 */
function forwardSetCookies(response: Response, cookies: Cookies): void {
	const raw =
		typeof response.headers.getSetCookie === 'function'
			? response.headers.getSetCookie()
			: [];
	const allowed = new Set<string>(FORWARDED_COOKIES);
	for (const header of raw) {
		const [pair, ...attributes] = header.split(';');
		const equals = pair.indexOf('=');
		if (equals <= 0) continue;
		const name = pair.slice(0, equals).trim();
		const value = pair.slice(equals + 1).trim();
		/*
		 * Replay only the four cookies Drogon owns. An unexpected Set-Cookie
		 * from the API is dropped rather than passed to the browser, so a
		 * compromised or misconfigured upstream cannot set arbitrary cookies
		 * on this origin.
		 */
		if (!allowed.has(name)) continue;

		let path = '/';
		let maxAge: number | undefined;
		let httpOnly = false;
		let secure = false;
		let sameSite: 'lax' | 'strict' | 'none' = 'lax';

		for (const attribute of attributes) {
			const [key, attrValue = ''] = attribute.split('=');
			switch (key.trim().toLowerCase()) {
				case 'path':
					path = attrValue.trim() || '/';
					break;
				case 'max-age':
					maxAge = Number(attrValue.trim());
					break;
				case 'httponly':
					httpOnly = true;
					break;
				case 'secure':
					secure = true;
					break;
				case 'samesite': {
					const normalized = attrValue.trim().toLowerCase();
					if (normalized === 'strict' || normalized === 'none' || normalized === 'lax') {
						sameSite = normalized;
					}
					break;
				}
			}
		}

		cookies.set(name, value, {
			path,
			httpOnly,
			secure,
			sameSite,
			...(maxAge === undefined || Number.isNaN(maxAge) ? {} : { maxAge })
		});
	}
}

async function parseError(response: Response, fallbackStatus: number): Promise<ApiError> {
	let code = 'INTERNAL';
	let message = 'Something went wrong.';
	let requestId = response.headers.get('x-request-id') ?? '';
	let fields: ApiFieldError[] = [];
	try {
		const body = (await response.json()) as ApiErrorBody;
		if (body?.error) {
			code = body.error.code ?? code;
			message = body.error.message ?? message;
			requestId = body.error.request_id ?? requestId;
			fields = body.error.details?.fields ?? [];
		}
	} catch {
		/* A non-JSON error body is not worth surfacing verbatim to a user. */
	}
	return new ApiError(
		fallbackStatus,
		code,
		message,
		requestId,
		fields,
		parseRetryAfter(response.headers.get('retry-after'))
	);
}

/**
 * Performs one API call.
 *
 * Throws ApiError when the API answered with a failure, and ApiUnavailable
 * when it could not be reached at all. Callers distinguish the two because the
 * contract requires a browse fallback for search outages but not for refusals.
 */
export async function apiCall<T>(
	event: Pick<RequestEvent, 'cookies' | 'fetch' | 'request'>,
	path: string,
	options: CallOptions = {}
): Promise<T> {
	const method = options.method ?? 'GET';
	const timeout = options.timeoutMs ?? READ_TIMEOUT_MS;

	const headers: Record<string, string> = {
		accept: 'application/json',
		...options.headers
	};

	/*
	 * Drogon validates Origin on every state-changing request. Sending it only
	 * on mutations keeps reads free of a header that would otherwise imply a
	 * cross-origin context they do not have.
	 */
	if (method !== 'GET' && PUBLIC_ORIGIN) {
		headers.origin = PUBLIC_ORIGIN;
	}

	const cookieHeader = buildCookieHeader(event.cookies);
	if (cookieHeader) {
		headers.cookie = cookieHeader;
	}
	if (options.body !== undefined) {
		headers['content-type'] = 'application/json';
	}

	/* Correlates SvelteKit and Drogon logs for one browser request. */
	const incomingRequestId = event.request.headers.get('x-request-id');
	if (incomingRequestId) {
		headers['x-request-id'] = incomingRequestId;
	}

	/**
	 * One bounded retry, for idempotent GET only.
	 *
	 * Mutations are never retried: a login, vote, submission, report, or
	 * moderation action that timed out may well have succeeded, and repeating
	 * it could double-apply. Only transient connection failures qualify, and
	 * the single short jittered wait avoids synchronised retry storms when many
	 * requests fail at once.
	 */
	const canRetry = method === 'GET';
	let response: Response | undefined;
	let lastCause: unknown;
	let timedOut = false;

	for (let attempt = 0; attempt <= (canRetry ? 1 : 0); attempt += 1) {
		const controller = new AbortController();
		const timer = setTimeout(() => {
			timedOut = true;
			controller.abort();
		}, timeout);
		try {
			response = await event.fetch(`${API_BASE}${path}`, {
				method,
				headers,
				signal: controller.signal,
				...(options.body === undefined ? {} : { body: JSON.stringify(options.body) })
			});
		} catch (cause) {
			lastCause = cause;
			response = undefined;
		} finally {
			clearTimeout(timer);
		}

		if (response && !isTransient(response.status)) break;
		if (attempt === 0 && canRetry && !timedOut) {
			await sleep(120 + Math.floor(Math.random() * 120));
			continue;
		}
		break;
	}

	if (!response) {
		throw new ApiUnavailable(lastCause, timedOut);
	}

	forwardSetCookies(response, event.cookies);

	if (!response.ok) {
		throw await parseError(response, response.status);
	}
	if (response.status === 204) {
		return undefined as T;
	}

	/*
	 * A 200 is not automatically usable. Wrong content type usually means a
	 * proxy error page reached us, and parsing it as success would render
	 * whatever it contained.
	 */
	const contentType = response.headers.get('content-type') ?? '';
	if (!contentType.toLowerCase().includes('application/json')) {
		throw new ApiMalformed(`unexpected content-type: ${contentType || 'none'}`);
	}

	const declaredLength = Number(response.headers.get('content-length') ?? '0');
	if (Number.isFinite(declaredLength) && declaredLength > MAX_RESPONSE_BYTES) {
		throw new ApiMalformed(`response too large: ${declaredLength}`);
	}

	const raw = await response.text();
	if (raw.length > MAX_RESPONSE_BYTES) {
		throw new ApiMalformed(`response too large: ${raw.length}`);
	}
	try {
		return JSON.parse(raw) as T;
	} catch (cause) {
		throw new ApiMalformed(`invalid JSON: ${String(cause)}`);
	}
}

/**
 * Multipart upload.
 *
 * Sends a FormData body untouched. The Content-Type header is deliberately
 * never set here: fetch must generate it so it carries the multipart boundary
 * it actually used. Setting it by hand produces a boundary-less header and the
 * upstream parser sees a malformed body.
 *
 * Otherwise identical to apiCall: cookies forwarded, Origin sent because this
 * is a mutation, Set-Cookie replayed, failures classified the same way.
 */
export async function apiUpload<T>(
	event: Pick<RequestEvent, 'cookies' | 'fetch' | 'request'>,
	path: string,
	form: FormData,
	options: { csrfToken?: string; timeoutMs?: number } = {}
): Promise<T> {
	const controller = new AbortController();
	const timer = setTimeout(() => controller.abort(), options.timeoutMs ?? UPLOAD_TIMEOUT_MS);

	const headers: Record<string, string> = { accept: 'application/json' };
	if (PUBLIC_ORIGIN) headers.origin = PUBLIC_ORIGIN;
	if (options.csrfToken) headers['x-csrf-token'] = options.csrfToken;
	const cookieHeader = buildCookieHeader(event.cookies);
	if (cookieHeader) headers.cookie = cookieHeader;
	const incomingRequestId = event.request.headers.get('x-request-id');
	if (incomingRequestId) headers['x-request-id'] = incomingRequestId;

	let response: Response;
	try {
		response = await event.fetch(`${API_BASE}${path}`, {
			method: 'POST',
			headers,
			body: form,
			signal: controller.signal
		});
	} catch (cause) {
		throw new ApiUnavailable(cause, controller.signal.aborted);
	} finally {
		clearTimeout(timer);
	}

	forwardSetCookies(response, event.cookies);
	if (!response.ok) throw await parseError(response, response.status);

	const contentType = response.headers.get('content-type') ?? '';
	if (!contentType.toLowerCase().includes('application/json')) {
		throw new ApiMalformed(`unexpected content-type: ${contentType || 'none'}`);
	}
	return (await response.json()) as T;
}

/** Convenience wrapper for the slower credential path. */
export function apiLoginCall<T>(
	event: Pick<RequestEvent, 'cookies' | 'fetch' | 'request'>,
	path: string,
	options: CallOptions = {}
): Promise<T> {
	return apiCall<T>(event, path, { ...options, timeoutMs: LOGIN_TIMEOUT_MS });
}
