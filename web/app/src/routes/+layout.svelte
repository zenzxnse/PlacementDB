<script lang="ts">
	import '../app.css';
	import type { Snippet } from 'svelte';
	import { page } from '$app/state';

	let { children }: { children: Snippet } = $props();

	const navItems = [
		{ href: '/', label: 'Home' },
		{ href: '/questions', label: 'Questions' },
		{ href: '/experiences', label: 'Experiences' },
		{ href: '/search', label: 'Search' },
		{ href: '/login', label: 'Log in' }
	];

	function isCurrent(href: string): boolean {
		const path = page.url.pathname;
		if (href === '/') return path === '/';
		return path === href || path.startsWith(`${href}/`);
	}
</script>

<a class="skip-link" href="#main-content">Skip to main content</a>

<header class="site-header">
	<div class="container">
		<a class="brand" href="/">PlacementDB</a>
		<nav class="site-nav" aria-label="Primary">
			<ul>
				{#each navItems as item}
					<li>
						<a href={item.href} aria-current={isCurrent(item.href) ? 'page' : undefined}
							>{item.label}</a
						>
					</li>
				{/each}
			</ul>
		</nav>
	</div>
</header>

<main id="main-content" class="container">
	{@render children()}
</main>

<footer class="site-footer">
	<div class="container">
		<p>
			PlacementDB is a community question bank for SRM placements. Every submission is
			reviewed by moderators before it appears.
		</p>
	</div>
</footer>
