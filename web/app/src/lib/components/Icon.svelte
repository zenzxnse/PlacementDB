<script lang="ts">
	import { ICONS, type IconName } from '$lib/icons';

	/**
	 * Inline SVG icon.
	 *
	 * Renders structured geometry through svelte:element rather than {@html},
	 * which is banned. The shapes come from a fixed compile-time set, so an
	 * icon name can never be attacker chosen and no request leaves the origin.
	 *
	 * Decorative by default: an icon beside a text label must not be announced
	 * twice, so it is aria-hidden unless a label is supplied for an icon-only
	 * control.
	 */
	let {
		name,
		size = 18,
		label
	}: { name: IconName; size?: number; label?: string } = $props();

	const shapes = $derived(ICONS[name] ?? []);
</script>

<svg
	class="icon"
	width={size}
	height={size}
	viewBox="0 0 24 24"
	fill="none"
	stroke="currentColor"
	stroke-width="1.75"
	stroke-linecap="round"
	stroke-linejoin="round"
	role={label ? 'img' : undefined}
	aria-label={label}
	aria-hidden={label ? undefined : 'true'}
	focusable="false"
>
	{#each shapes as shape}
		<svelte:element this={shape.tag} {...shape.attrs} />
	{/each}
</svg>
