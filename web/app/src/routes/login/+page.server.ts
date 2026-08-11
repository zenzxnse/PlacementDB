import { fail, redirect } from '@sveltejs/kit';
import type { Actions, PageServerLoad } from './$types';
import { apiCall, apiLoginCall, ApiError, ApiUnavailable } from '$lib/server/api';
import { parseCsrf } from '$lib/wire';

/**
 * Login through the accepted contract.
 *
 * GET /auth/csrf issues the short-lived login CSRF token and sets its cookie.
 * The token goes into a hidden form field; the cookie is forwarded by the API
 * client. POST /auth/login verifies both.
 *
 * The password is never preserved, never logged, and never returned.
 */

interface CsrfResponse {
	csrf_token: string;
}

export const load: PageServerLoad = async (event) => {
	try {
		const issued = await apiCall(event, '/auth/csrf', { parse: parseCsrf });
		return { csrfToken: issued.csrf_token, apiReachable: true };
	} catch {
		/**
		 * Render the form without a token rather than an error page. The post
		 * will be refused by the API, which is the correct outcome, and the
		 * user sees a normal page instead of a stack of failures.
		 */
		return { csrfToken: '', apiReachable: false };
	}
};

export const actions: Actions = {
	default: async (event) => {
		const form = await event.request.formData();
		const identity = String(form.get('identity') ?? '').trim();
		const password = String(form.get('password') ?? '');
		const csrfToken = String(form.get('_csrf') ?? '');

		const errors: { field: string; message: string }[] = [];
		/*
		 * Refuse rather than attempt a login the API is certain to reject.
		 * Without a token the request is indistinguishable from a forged one,
		 * and attempting it would burn an Argon2id slot for nothing.
		 */
		if (csrfToken.length === 0) {
			return fail(400, {
				identity,
				errors: [
					{
						field: 'form',
						message: 'This form expired. Reload the page and try again.'
					}
				]
			});
		}
		if (identity.length === 0) {
			errors.push({ field: 'identity', message: 'Enter your username or email.' });
		}
		if (password.length === 0) {
			errors.push({ field: 'password', message: 'Enter your password.' });
		}
		if (errors.length > 0) {
			return fail(400, { identity, errors });
		}

		try {
			await apiLoginCall(event, '/auth/login', {
				method: 'POST',
				headers: { 'x-csrf-token': csrfToken },
				/* The token travels in the body too, per the accepted contract. */
				body: { identity, password, csrf_token: csrfToken }
			});
		} catch (error) {
			if (error instanceof ApiUnavailable) {
				return fail(503, {
					identity,
					errors: [
						{
							field: 'form',
							message: 'Sign-in is unavailable right now. Please try again shortly.'
						}
					]
				});
			}
			if (error instanceof ApiError) {
				/**
				 * Field-level messages come from the API where it supplied
				 * them. Everything else collapses to one form-level message,
				 * so a failure cannot become an account-existence oracle.
				 */
				const fields = error.fields.length
					? error.fields.map((f) => ({ field: f.field, message: f.message }))
					: [{ field: 'form', message: error.message }];
				return fail(error.status, { identity, errors: fields });
			}
			throw error;
		}

		/*
		 * Redirect after success so a reload cannot repost credentials.
		 *
		 * `next` returns the user where they were sent from. Only a same-origin
		 * path is honoured, so a crafted link cannot turn login into an open
		 * redirect, and a loop back to /login is refused.
		 */
		const requested = event.url.searchParams.get('next') ?? '';
		const safeNext =
			requested.startsWith('/') &&
			!requested.startsWith('//') &&
			!requested.startsWith('/login')
				? requested
				: '/';
		redirect(303, safeNext);
	}
};
