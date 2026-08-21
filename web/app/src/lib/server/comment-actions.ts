import type { RequestEvent } from '@sveltejs/kit';
import { apiCall, ApiError, toFailure } from './api';
import { loginWithReturn } from './content';
import { requireModerator } from './moderation';
import {
	REPORT_REASON_MAX_LENGTH,
	REPORT_REASON_VALUES,
	type CommentActionOutcome
	, type ContentReportOutcome
} from '$lib/types';

/**
 * Shared actions for comment reports and moderator hiding.
 *
 * Both are plain mutations against the accepted comments/moderation contract:
 * never retried, CSRF-checked before any upstream work, and the comment's
 * public ID is taken from the form because it is not derivable from the page
 * slug. That is acceptable here, unlike the difficulty vote, because the ID
 * only names a target the API independently authorizes: the report route
 * re-checks visibility, ownership, and session, and the hide route requires a
 * moderator. A tampered form can only aim at a comment the API will refuse.
 */

type Event = Pick<RequestEvent, 'cookies' | 'fetch' | 'request' | 'url'>;

export async function reportContent(
	event: Event,
	fields: { csrf: string; targetType: 'question' | 'experience' | 'user'; publicId: string; reason: string; details: string }
): Promise<ContentReportOutcome> {
	if (!fields.csrf) return { kind: 'error', message: 'This form expired. Reload the page and try again.' };
	if (!(REPORT_REASON_VALUES as readonly string[]).includes(fields.reason))
		return { kind: 'error', message: 'Choose a reason for the report.' };
	if (fields.details.length > REPORT_REASON_MAX_LENGTH)
		return { kind: 'error', message: `Details are limited to ${REPORT_REASON_MAX_LENGTH} characters.` };
	try {
		await apiCall(event, '/reports', {
			method: 'POST', headers: { 'x-csrf-token': fields.csrf },
			body: { target_type: fields.targetType, public_id: fields.publicId, reason: fields.reason,
				...(fields.details ? { details: fields.details } : {}) }
		});
		return { kind: 'reported', message: 'Report received. A moderator will review it.' };
	} catch (error_) {
		if (error_ instanceof ApiError && error_.status === 401)
			return { kind: 'error', message: 'Your session expired. Log in again to report this item.', loginHref: loginWithReturn(event.url.pathname) };
		if (error_ instanceof ApiError && error_.status === 409)
			return { kind: 'error', message: 'You already have an open report for this item.' };
		if (error_ instanceof ApiError && (error_.status === 403 || error_.status === 404))
			return { kind: 'error', message: 'You cannot report this item.' };
		return { kind: 'error', message: toFailure(error_).message };
	}
}

export async function reportComment(
	event: Event,
	fields: { csrf: string; commentId: string; reason: string; details: string }
): Promise<CommentActionOutcome> {
	const { csrf, commentId, reason, details } = fields;
	if (csrf.length === 0) {
		return {
			commentId,
			kind: 'error',
			message: 'This form expired. Reload the page and try again.'
		};
	}
	if (!(REPORT_REASON_VALUES as readonly string[]).includes(reason)) {
		return {
			commentId,
			kind: 'error',
			message: 'Choose a reason for the report.'
		};
	}
	if (details.length > REPORT_REASON_MAX_LENGTH) {
		return {
			commentId,
			kind: 'error',
			message: `Details are limited to ${REPORT_REASON_MAX_LENGTH} characters.`
		};
	}
	try {
		await apiCall(event, `/comments/${encodeURIComponent(commentId)}/reports`, {
			method: 'POST',
			headers: { 'x-csrf-token': csrf },
			/*
			 * Details are optional on the wire; an empty string is omitted
			 * rather than sent as blank, so the API's own optionality stays
			 * the single source.
			 */
			body: details.length === 0 ? { reason } : { reason, details }
		});
	} catch (error_) {
		if (error_ instanceof ApiError) {
			if (error_.status === 401) {
				return {
					commentId,
					kind: 'error',
					message: 'Your session expired. Log in again to report this comment.',
					loginHref: loginWithReturn(`${event.url.pathname}#comments-heading`)
				};
			}
			if (error_.status === 403) {
				return {
					commentId,
					kind: 'error',
					message: 'You cannot report this comment.'
				};
			}
			if (error_.status === 409) {
				return {
					commentId,
					kind: 'error',
					message: 'You already have an open report for this comment.'
				};
			}
			if (error_.status === 429) {
				const wait = error_.retryAfterSeconds;
				return {
					commentId,
					kind: 'error',
					message: wait
						? `Too many reports. Try again in about ${wait} seconds.`
						: 'Too many reports. Try again later.'
				};
			}
		}
		return { commentId, kind: 'error', message: toFailure(error_).message };
	}
	return {
		commentId,
		kind: 'reported',
		message: 'Report received. A moderator will review it.'
	};
}

export async function hideComment(
	event: Event & { url: URL },
	fields: { csrf: string; commentId: string; reason: string }
): Promise<CommentActionOutcome> {
	const { csrf, commentId, reason } = fields;
	/*
	 * The form renders only for moderators, but the action is reachable to
	 * anyone who forges a POST, so the gate is checked again here exactly as
	 * the API checks it again after that.
	 */
	await requireModerator(event);
	if (csrf.length === 0) {
		return {
			commentId,
			kind: 'error',
			message: 'This form expired. Reload the page and try again.'
		};
	}
	const trimmed = reason.trim();
	if (trimmed.length === 0) {
		return {
			commentId,
			kind: 'error',
			message: 'A reason is required to hide a comment.'
		};
	}
	if (trimmed.length > REPORT_REASON_MAX_LENGTH) {
		return {
			commentId,
			kind: 'error',
			message: `The reason is limited to ${REPORT_REASON_MAX_LENGTH} characters.`
		};
	}
	try {
		await apiCall(event, `/moderation/comments/${encodeURIComponent(commentId)}/hide`, {
			method: 'POST',
			headers: { 'x-csrf-token': csrf },
			body: { reason: trimmed }
		});
	} catch (error_) {
		if (error_ instanceof ApiError) {
			if (error_.status === 401) {
				return {
					commentId,
					kind: 'error',
					message: 'Your session expired. Log in again and retry the action.',
					loginHref: loginWithReturn(`${event.url.pathname}#comments-heading`)
				};
			}
			if (error_.status === 409) {
				return {
					commentId,
					kind: 'error',
					message: 'The comment was already hidden by another action. Reload to see the current list.'
				};
			}
			if (error_.status === 429) {
				const wait = error_.retryAfterSeconds;
				return {
					commentId,
					kind: 'error',
					message: wait
						? `Too many moderation actions. Try again in about ${wait} seconds.`
						: 'Too many moderation actions. Try again later.'
				};
			}
		}
		return { commentId, kind: 'error', message: toFailure(error_).message };
	}
	return {
		commentId,
		kind: 'hidden',
		message: 'Comment hidden. It no longer appears publicly.'
	};
}
