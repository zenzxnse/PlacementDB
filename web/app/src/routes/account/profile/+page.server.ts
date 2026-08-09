import { fail } from '@sveltejs/kit';
import type { Actions, PageServerLoad } from './$types';
import { apiCall, apiUpload, toFailure } from '$lib/server/api';
import { getMe, getProfile } from '$lib/server/content';
import { DEFAULT_AVATAR_URL, type AvatarResponse, type Me } from '$lib/types';
import { redirect } from '@sveltejs/kit';

/** Mirrors the upstream limit so an oversized file fails before it is sent. */
const MAX_AVATAR_BYTES = 2 * 1024 * 1024;

/**
 * Accepted formats. Checked here as an early filter only.
 *
 * The browser-reported type is advisory and trivially forged, so this is not a
 * security control. Drogon decodes magic bytes and is the real gate. Checking
 * here just spares the user a round trip for an obvious mistake.
 */
const ACCEPTED_TYPES = ['image/jpeg', 'image/png', 'image/webp'];

/**
 * One shape for every avatar outcome.
 *
 * Uniform keys mean the template reads a single type instead of a union of
 * near-identical variants, which is what made requestId unreachable before.
 */
interface AvatarState {
	scope: 'avatar';
	message?: string;
	requestId?: string;
	avatarUrl?: string;
	status?: 'saved' | 'removed';
}

function avatarFailure(message: string, requestId?: string): AvatarState {
	return { scope: 'avatar', message, ...(requestId ? { requestId } : {}) };
}

function avatarResult(avatarUrl: string, status: 'saved' | 'removed'): AvatarState {
	return { scope: 'avatar', avatarUrl, status };
}

interface CsrfResponse {
	csrf_token: string;
}

/* Narrow to what the API client needs, so loads and actions can both call it. */
type ApiEvent = Parameters<typeof apiCall>[0];

async function csrfToken(event: ApiEvent): Promise<string> {
	const issued = await apiCall<CsrfResponse>(event, '/auth/csrf');
	return issued.csrf_token;
}

export const load: PageServerLoad = async (event) => {
	const me: Me | null = await getMe(event);
	if (!me) {
		/* Send an unauthenticated visitor to login with a safe return path. */
		redirect(303, '/login?next=%2Faccount%2Fprofile');
	}
	let token = '';
	try {
		token = await csrfToken(event);
	} catch {
		/* Render the form without a token; the actions refuse to submit. */
	}
	const profile = await getProfile(event, me.username);
	return { me, csrfToken: token, defaultAvatar: profile?.avatar_url ?? DEFAULT_AVATAR_URL };
};

export const actions: Actions = {
	/**
	 * Avatar upload.
	 *
	 * Parses the browser's multipart body, validates that `avatar` really is a
	 * File, and builds a fresh FormData for the upstream request.
	 *
	 * Content-Type is never set by hand. fetch generates it so the multipart
	 * boundary matches the body it actually encoded; a hand-written header
	 * carries no boundary and the upstream parser rejects the request.
	 */
	avatar: async (event) => {
		const form = await event.request.formData();
		const file = form.get('avatar');

		if (!(file instanceof File) || file.size === 0) {
			return fail(400, avatarFailure('Choose an image file to upload.'));
		}
		if (form.getAll('avatar').length !== 1) {
			return fail(400, avatarFailure('Upload exactly one image.'));
		}
		if (file.size > MAX_AVATAR_BYTES) {
			return fail(413, avatarFailure('That image is larger than 2 MiB. Choose a smaller one.'));
		}
		if (file.type && !ACCEPTED_TYPES.includes(file.type)) {
			return fail(415, avatarFailure('Use a JPEG, PNG, or WebP image.'));
		}

		let token: string;
		try {
			token = await csrfToken(event);
		} catch {
			return fail(403, avatarFailure('This form expired. Reload the page and try again.'));
		}

		/*
		 * A new FormData rather than forwarding the incoming one: only the
		 * single field the contract names travels upstream, and the filename is
		 * dropped because the server ignores it and generates its own key.
		 */
		const upstream = new FormData();
		upstream.append('avatar', file, 'avatar');

		try {
			const result = await apiUpload<AvatarResponse>(event, '/me/avatar', upstream, {
				csrfToken: token
			});
			return avatarResult(result.avatar_url, 'saved');
		} catch (error) {
			const failure = toFailure(error);
			return fail(
				failure.kind === 'validation' ? 400 : 502,
				avatarFailure(failure.message, failure.requestId)
			);
		}
	},

	/**
	 * Avatar removal.
	 *
	 * The database key is cleared transactionally upstream; object deletion is
	 * best effort afterwards, so a success here means the profile no longer
	 * points at an image even if the bytes linger.
	 */
	removeAvatar: async (event) => {
		let token: string;
		try {
			token = await csrfToken(event);
		} catch {
			return fail(403, avatarFailure('This form expired. Reload the page and try again.'));
		}
		try {
			const result = await apiCall<AvatarResponse>(event, '/me/avatar', {
				method: 'DELETE',
				headers: { 'x-csrf-token': token }
			});
			return avatarResult(result.avatar_url, 'removed');
		} catch (error) {
			const failure = toFailure(error);
			return fail(502, avatarFailure(failure.message, failure.requestId));
		}
	}
};
