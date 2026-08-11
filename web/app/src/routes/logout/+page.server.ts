import { redirect } from '@sveltejs/kit';
import type { Actions } from './$types';
import { apiCall } from '$lib/server/api';
import { parseCsrf } from '$lib/wire';

/**
 * Logout.
 *
 * A POST form action, never a link. A GET that ends a session is a one-click
 * nuisance attack from any page that can embed a URL.
 *
 * The session CSRF token is fetched immediately before use rather than
 * rendered into every page, so a long-lived tab cannot hold a stale token and
 * the token appears in exactly one response.
 */
export const actions: Actions = {
	default: async (event) => {
		try {
			const issued = await apiCall(event, '/auth/csrf', { parse: parseCsrf });
			await apiCall(event, '/auth/logout', {
				method: 'POST',
				headers: { 'x-csrf-token': issued.csrf_token },
				body: { csrf_token: issued.csrf_token }
			});
		} catch {
			/*
			 * Redirect regardless. Drogon clears the cookie on success; if the
			 * call failed the session is still valid server-side, which is the
			 * safe direction to fail. An error page here would strand the user
			 * on something that still looks logged in.
			 */
		}
		redirect(303, '/');
	}
};
