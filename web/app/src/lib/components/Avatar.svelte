<script lang="ts">
	import { DEFAULT_AVATAR_URL } from '$lib/types';

	/**
	 * Avatar image with a fallback.
	 *
	 * The API always supplies avatar_url, already resolved. The onerror handler
	 * is a second line of defence for the case the server cannot know about: a
	 * stored object that has gone missing, or storage being unreachable at the
	 * moment the browser fetches it. Swapping the src in that case keeps the
	 * page intact without touching the profile row.
	 *
	 * The guard stops a loop if the default itself ever fails to load.
	 */
	let {
		src,
		name,
		size = 96
	}: { src: string | null | undefined; name: string; size?: number } = $props();

	const resolved = $derived(src && src.trim() ? src : DEFAULT_AVATAR_URL);
</script>

<img
	class="avatar"
	src={resolved}
	width={size}
	height={size}
	loading="lazy"
	decoding="async"
	alt=""
	data-name={name}
	onerror={(event) => {
		const image = event.currentTarget as HTMLImageElement;
		if (!image.src.endsWith(DEFAULT_AVATAR_URL)) {
			image.src = DEFAULT_AVATAR_URL;
		}
	}}
/>
