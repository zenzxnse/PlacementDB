import { fail } from '@sveltejs/kit';
import type { Actions, PageServerLoad } from './$types';

export const load: PageServerLoad = () => {
	return { apiConnected: false };
};

export const actions: Actions = {
	default: async ({ request }) => {
		const form = await request.formData();
		const identity = String(form.get('identity') ?? '').trim();
		const password = String(form.get('password') ?? '');

		const errors: { field: string; message: string }[] = [];
		if (identity.length === 0) {
			errors.push({ field: 'identity', message: 'Enter your username or email.' });
		}
		if (password.length === 0) {
			errors.push({ field: 'password', message: 'Enter your password.' });
		}
		if (errors.length > 0) {
			return fail(400, { identity, errors });
		}

		return fail(503, {
			identity,
			errors: [
				{
					field: 'form',
					message:
						'Login is not connected to the API yet. Accounts arrive with the authentication milestone.'
				}
			]
		});
	}
};
