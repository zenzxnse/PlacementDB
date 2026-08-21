import { error, redirect } from '@sveltejs/kit';
import type { RequestEvent } from '@sveltejs/kit';
import { apiCall } from './api';
import { getMe, loginWithReturn } from './content';
import {
	REPORT_STATE_VALUES,
	type ModerationReportItem,
	type ModerationReportPage
} from '$lib/types';

type Event = Pick<RequestEvent, 'cookies' | 'fetch' | 'request' | 'url'>;
export interface ModerationQueueItem { target_type: 'question' | 'experience'; public_id: string; title: string; state: string; author: { username: string; display_name: string }; created_at?: string; }
export interface ModerationAuditItem { event_kind: string; target_type: string; public_id: string; previous_state: string; new_state: string; reason: string | null; actor_role: string; created_at: string; }
export interface ModerationPage<T> { items: T[]; next_cursor: string | null; }

function pageParser<T>(value: unknown): ModerationPage<T> {
	if (!value || typeof value !== 'object' || !Array.isArray((value as { items?: unknown }).items))
		throw new Error('Malformed moderation response');
	const page = value as { items: T[]; next_cursor?: unknown };
	if (page.next_cursor !== null && page.next_cursor !== undefined && typeof page.next_cursor !== 'string')
		throw new Error('Malformed moderation cursor');
	return { items: page.items, next_cursor: (page.next_cursor as string | null | undefined) ?? null };
}

export async function requireModerator(event: Event) {
	const me = await getMe(event);
	if (!me) redirect(303, loginWithReturn(event.url.pathname));
	if (!me.can_moderate) error(403, 'Moderator access is required.');
	return me;
}

export async function moderationQueue(event: Event, cursor?: string) {
	return apiCall<ModerationPage<ModerationQueueItem>>(event, `/moderation/queue${cursor ? `?cursor=${encodeURIComponent(cursor)}` : ''}`, { parse: pageParser });
}

export async function moderationAudit(event: Event, cursor?: string) {
	return apiCall<ModerationPage<ModerationAuditItem>>(event, `/moderation/audit${cursor ? `?cursor=${encodeURIComponent(cursor)}` : ''}`, { parse: pageParser });
}

function isRecord(value: unknown): value is Record<string, unknown> {
	return typeof value === 'object' && value !== null && !Array.isArray(value);
}

/** Narrows a state string to the accepted four-value set. */
export function isReportState(value: string): value is ModerationReportItem['state'] {
	return (REPORT_STATE_VALUES as readonly string[]).includes(value);
}

/**
 * Strict parser for GET /moderation/reports, in the wire.ts style: declared
 * fields checked, unknown fields allowed, failures carry a path. The state is
 * validated against the accepted four-value set so an unexpected value cannot
 * render as if it were a real state.
 */
function parseReportPage(value: unknown): ModerationReportPage {
	if (!isRecord(value)) throw new Error('Malformed moderation response');
	const rawItems = Array.isArray(value.items) ? value.items : [];
	const items: ModerationReportItem[] = rawItems.map((raw, index) => {
		if (!isRecord(raw)) throw new Error(`Malformed report at items[${index}]`);
		const state = typeof raw.state === 'string' ? raw.state : '';
		if (!isReportState(state)) {
			throw new Error(`Unexpected report state at items[${index}]`);
		}
		return {
			public_id: typeof raw.public_id === 'string' ? raw.public_id : '',
			target_type: typeof raw.target_type === 'string' ? raw.target_type : '',
			reason: typeof raw.reason === 'string' ? raw.reason : '',
			details: typeof raw.details === 'string' ? raw.details : null,
			state,
			reporter_label: typeof raw.reporter_label === 'string' ? raw.reporter_label : '',
			created_at: typeof raw.created_at === 'string' ? raw.created_at : ''
		};
	});
	for (const item of items) {
		if (!item.public_id || !item.created_at) throw new Error('Malformed report identity');
	}
	const next = value.next_cursor;
	return { items, next_cursor: typeof next === 'string' ? next : null };
}

export async function moderationReports(event: Event, state: string, cursor?: string) {
	const params = new URLSearchParams({ state });
	if (cursor) params.set('cursor', cursor);
	return apiCall<ModerationReportPage>(event, `/moderation/reports?${params.toString()}`, {
		parse: parseReportPage
	});
}
