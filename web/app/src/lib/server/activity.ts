import type { RequestEvent } from '@sveltejs/kit';
import { apiCall } from './api';
import type {
	AccountReportItem,
	AccountSubmissionItem,
	AccountVoteItem,
	CursorPage
} from '$lib/types';

type Event = Pick<RequestEvent, 'cookies' | 'fetch' | 'request'>;

function record(value: unknown): Record<string, unknown> {
	if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Malformed activity response');
	return value as Record<string, unknown>;
}

function text(value: unknown): string {
	if (typeof value !== 'string' || value.length === 0) throw new Error('Malformed activity text');
	return value;
}

function page<T>(value: unknown, parseItem: (item: unknown) => T): CursorPage<T> {
	const raw = record(value);
	if (!Array.isArray(raw.items) || (raw.next_cursor !== null && typeof raw.next_cursor !== 'string'))
		throw new Error('Malformed activity page');
	return { items: raw.items.map(parseItem), next_cursor: raw.next_cursor as string | null };
}

export function parseSubmissionPage(value: unknown): CursorPage<AccountSubmissionItem> {
	return page(value, (item) => {
		const raw = record(item);
		const kind = text(raw.kind);
		if (kind !== 'question' && kind !== 'experience') throw new Error('Malformed submission kind');
		return { kind, public_id: text(raw.public_id), slug: text(raw.slug), title: text(raw.title), state: text(raw.state), updated_at: text(raw.updated_at) };
	});
}

export function parseVotePage(value: unknown): CursorPage<AccountVoteItem> {
	return page(value, (item) => {
		const raw = record(item);
		if (!Number.isInteger(raw.value) || (raw.value as number) < 1 || (raw.value as number) > 5)
			throw new Error('Malformed vote value');
		let target: AccountVoteItem['target'] = null;
		if (raw.target !== null) {
			const targetRaw = record(raw.target);
			target = { public_id: text(targetRaw.public_id), slug: text(targetRaw.slug), title: text(targetRaw.title) };
		}
		return { value: raw.value as number, updated_at: text(raw.updated_at), target };
	});
}

export function parseReportPage(value: unknown): CursorPage<AccountReportItem> {
	return page(value, (item) => {
		const raw = record(item);
		const targetType = text(raw.target_type);
		const state = text(raw.state);
		if (!['question', 'experience', 'user', 'comment'].includes(targetType) ||
			!['open', 'under_review', 'resolved', 'dismissed'].includes(state))
			throw new Error('Malformed report state');
		if (raw.details !== null && typeof raw.details !== 'string') throw new Error('Malformed report details');
		return { public_id: text(raw.public_id), target_type: targetType as AccountReportItem['target_type'], reason: text(raw.reason), details: raw.details as string | null, state: state as AccountReportItem['state'], created_at: text(raw.created_at) };
	});
}

function query(cursor?: string): string {
	return cursor ? `?cursor=${encodeURIComponent(cursor)}` : '';
}

export const mySubmissions = (event: Event, cursor?: string) => apiCall(event, `/me/submissions${query(cursor)}`, { parse: parseSubmissionPage });
export const myVotes = (event: Event, cursor?: string) => apiCall(event, `/me/votes${query(cursor)}`, { parse: parseVotePage });
export const myReports = (event: Event, cursor?: string) => apiCall(event, `/me/reports${query(cursor)}`, { parse: parseReportPage });
