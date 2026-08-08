import { error } from '@sveltejs/kit';
import type { PageServerLoad } from './$types';
import { getQuestionBySlug } from '$lib/server/data';

export const load: PageServerLoad = ({ params }) => {
	const question = getQuestionBySlug(params.slug);
	if (question === null) {
		error(404, { message: 'This question does not exist or is not published.' });
	}
	return { question };
};
