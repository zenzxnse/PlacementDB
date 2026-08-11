import { fail, redirect } from '@sveltejs/kit';
import type { Actions, PageServerLoad } from './$types';
import { apiCall, apiLoginCall, ApiError, ApiUnavailable } from '$lib/server/api';
import { getMe } from '$lib/server/content';
import { parseCsrf } from '$lib/wire';

/**
 * Registration, against the shapes in
 * adr/codex/account-submission-difficulty-contract-2026-08-11.md section 1.
 *
 * Registration uses the login-CSRF cookie plus the same token in the JSON
 * body, exactly like login, because the caller has no session yet. The token
 * also travels in X-CSRF-Token so the API can check either.
 *
 * The password is never preserved after a failure, never echoed, never logged.
 * Neither is the CSRF token, which is re-issued by the reload.
 *
 * No message on this page may reveal whether a username or email is already
 * taken. The API deliberately answers every identifier conflict with the same
 * 409 DUPLICATE and no field details; this page must not undo that by adding
 * a friendlier field-level message of its own.
 */

interface CsrfResponse {
	csrf_token: string;
}

/** Contract limits, mirrored so an obviously bad input never costs an Argon2id slot. */
const USERNAME_PATTERN = /^[A-Za-z][A-Za-z0-9_]{2,31}$/;
const DISPLAY_NAME_MAX = 80;
const EMAIL_MAX_BYTES = 254;
const PASSWORD_MIN_BYTES = 12;
/*
 * 256, not the 1024 in the contract ADR. `auth::kMaxPasswordLength` in
 * api/include/auth/secret.h is 256 and the API's own field message says so,
 * so 1024 here would accept a passphrase the API then refuses. The ADR is the
 * one that is wrong; see adr/claude/codex-batch-review-2026-08-11.md.
 */
const PASSWORD_MAX_BYTES = 256;

function byteLength(value: string): number {
	return new TextEncoder().encode(value).length;
}

/**
 * Deliberately loose email check.
 *
 * The API owns the authoritative rule. Rejecting more here than the contract
 * does would turn a valid address into a frontend-only failure, and address
 * syntax is one of the classic places a "smart" regex is wrong.
 */
function looksLikeEmail(value: string): boolean {
	if (value.includes(' ')) return false;
	const at = value.indexOf('@');
	return at > 0 && at === value.lastIndexOf('@') && at < value.length - 1;
}

export const load: PageServerLoad = async (event) => {
	/* Someone already signed in has no business on this page. */
	const me = await getMe(event);
	if (me) redirect(303, '/');
	try {
		const issued = await apiCall(event, '/auth/csrf', { parse: parseCsrf });
		return { csrfToken: issued.csrf_token };
	} catch {
		/*
		 * Render the form without a token rather than an error page: the user
		 * sees a normal page and a plain explanation, and the action refuses
		 * to post something the API is certain to reject.
		 */
		return { csrfToken: '' };
	}
};

export const actions: Actions = {
	default: async (event) => {
		const form = await event.request.formData();
		const username = String(form.get('username') ?? '').trim();
		const email = String(form.get('email') ?? '').trim();
		const displayName = String(form.get('display_name') ?? '').trim();
		const password = String(form.get('password') ?? '');
		const csrfToken = String(form.get('_csrf') ?? '');

		/* Everything safe to show again after a failure. Never the password. */
		const preserved = { username, email, displayName };

		if (csrfToken.length === 0) {
			return fail(403, {
				...preserved,
				errors: [
					{ field: 'form', message: 'This form expired. Reload the page and try again.' }
				]
			});
		}

		const errors: { field: string; message: string }[] = [];
		if (!USERNAME_PATTERN.test(username)) {
			errors.push({
				field: 'username',
				message:
					'Usernames are 3 to 32 characters, start with a letter, and use letters, digits, or underscores.'
			});
		}
		if (!looksLikeEmail(email) || byteLength(email) > EMAIL_MAX_BYTES) {
			errors.push({ field: 'email', message: 'Enter an email address you can receive mail at.' });
		}
		if (displayName.length === 0 || displayName.length > DISPLAY_NAME_MAX) {
			errors.push({
				field: 'display_name',
				message: `Enter a display name of 1 to ${DISPLAY_NAME_MAX} characters.`
			});
		}
		const passwordBytes = byteLength(password);
		if (passwordBytes < PASSWORD_MIN_BYTES || passwordBytes > PASSWORD_MAX_BYTES) {
			/*
			 * A length floor and nothing else, per the contract: no composition
			 * theatre. A long passphrase is the point.
			 */
			errors.push({
				field: 'password',
				message: `Use a password of at least ${PASSWORD_MIN_BYTES} characters. A passphrase works well.`
			});
		}
		if (errors.length > 0) {
			return fail(400, { ...preserved, errors });
		}

		try {
			await apiLoginCall(event, '/auth/register', {
				method: 'POST',
				headers: { 'x-csrf-token': csrfToken },
				body: {
					username,
					email,
					display_name: displayName,
					password,
					csrf_token: csrfToken
				}
			});
		} catch (error) {
			if (error instanceof ApiUnavailable) {
				return fail(503, {
					...preserved,
					errors: [
						{
							field: 'form',
							message: 'Account creation is unavailable right now. Please try again shortly.'
						}
					]
				});
			}
			if (error instanceof ApiError) {
				/*
				 * DUPLICATE stays a single form-level message with no field
				 * attached. Attaching it to `username` or `email` would tell a
				 * stranger which identifier exists, which is the enumeration
				 * oracle the contract is written to avoid.
				 */
				if (error.status === 409) {
					return fail(409, {
						...preserved,
						errors: [
							{
								field: 'form',
								message: 'An account could not be created with those details.'
							}
						]
					});
				}
				if (error.status === 429) {
					const wait = error.retryAfterSeconds;
					return fail(429, {
						...preserved,
						errors: [
							{
								field: 'form',
								message: wait
									? `Too many attempts. Try again in about ${wait} seconds.`
									: 'Too many attempts. Try again later.'
							}
						]
					});
				}
				/* Field errors the API supplied, otherwise one form-level message. */
				const fields = error.fields.length
					? error.fields.map((f) => ({ field: f.field, message: f.message }))
					: [{ field: 'form', message: error.message }];
				return fail(error.status, { ...preserved, errors: fields });
			}
			throw error;
		}

		/*
		 * Registration establishes the session itself, and its Set-Cookie was
		 * already forwarded by the API client. Redirect after success so a
		 * reload cannot repost the credentials.
		 */
		redirect(303, '/');
	}
};
