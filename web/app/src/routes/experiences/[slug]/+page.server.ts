import { error } from '@sveltejs/kit';
import type { PageServerLoad } from './$types';
import { getExperienceBySlug } from '$lib/server/data';

export const load: PageServerLoad = ({ params }) => {
	const experience = getExperienceBySlug(params.slug);
	if (experience === null) {
		error(404, { message: 'This experience does not exist or is not published.' });
	}
	return { experience };
};
