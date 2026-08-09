import type { LayoutServerLoad } from './$types';
import { getMe } from '$lib/server/content';

/**
 * Header state for every page.
 *
 * Runs on the server so the session cookie never reaches client code. Control
 * flow in templates uses can_submit and can_moderate only, never a comparison
 * against `role`.
 */
export const load: LayoutServerLoad = async (event) => {
	return { me: await getMe(event) };
};
