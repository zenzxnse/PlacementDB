import { error, redirect } from '@sveltejs/kit';
import type { RequestEvent } from '@sveltejs/kit';
import { apiCall } from './api';
import { getMe, loginWithReturn } from './content';

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
