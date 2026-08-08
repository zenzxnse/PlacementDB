import type { PageServerLoad } from './$types';
import { getHomeData } from '$lib/server/data';

export const load: PageServerLoad = () => {
	return getHomeData();
};
