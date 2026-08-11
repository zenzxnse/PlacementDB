<script lang="ts">
	import '../app.css';
	import type { Snippet } from 'svelte';
	import { page } from '$app/state';
	import type { LayoutServerData } from './$types';
	import TopBar from '$lib/components/TopBar.svelte';
	import SideNav from '$lib/components/SideNav.svelte';

	let { children, data }: { children: Snippet; data: LayoutServerData } = $props();

	const me = $derived(data.me);
	const pathname = $derived(page.url.pathname);
	/* Keeps the current query visible in the bar after a search. */
	const query = $derived(page.url.searchParams.get('q') ?? '');
</script>

<a class="skip-link" href="#main-content">Skip to main content</a>

<TopBar {me} {query} />

<div class="shell">
	<SideNav {me} {pathname} />
	<main id="main-content" class="shell-main" tabindex="-1">
		<div class="shell-main-inner">
			{@render children()}
		</div>
	</main>
</div>

<footer class="site-footer">
	<div class="shell-main-inner">
		<p>
			PlacementDB is a community question bank for SRM placements. Every submission is
			reviewed by moderators before it appears.
		</p>
		<nav aria-label="Site information">
			<ul class="footer-links">
				<li><a href="/about">About</a></li>
				<li><a href="/about/help">Help</a></li>
				<li><a href="/about/moderation">How moderation works</a></li>
			</ul>
		</nav>
	</div>
</footer>
