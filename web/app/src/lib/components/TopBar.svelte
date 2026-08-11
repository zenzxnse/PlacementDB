<script lang="ts">
	import Icon from './Icon.svelte';
	import type { Me } from '$lib/types';

	/**
	 * Global bar: identity, search, and the personal cluster.
	 *
	 * Search lives here rather than on a page because it is the fastest route
	 * into a question bank, and a GET form keeps it shareable and functional
	 * without JavaScript.
	 */
	let { me, query = '' }: { me: Me | null; query?: string } = $props();
</script>

<header class="topbar">
	<div class="topbar-inner">
		<a class="brand" href="/">
			<span class="brand-mark" aria-hidden="true">PDB</span>
			<span class="brand-word">PlacementDB</span>
		</a>

		<nav class="topbar-links" aria-label="Secondary">
			<a href="/about">About</a>
			<a href="/about/help">Help</a>
		</nav>

		<!-- GET so a search is a shareable URL and works with JavaScript off. -->
		<form class="topbar-search" method="get" action="/search" role="search">
			<label class="visually-hidden" for="global-search">Search questions and experiences</label>
			<span class="topbar-search-icon" aria-hidden="true"><Icon name="search" size={17} /></span>
			<input
				id="global-search"
				type="search"
				name="q"
				value={query}
				placeholder="Search questions, companies, topics"
				autocomplete="off"
			/>
			<!--
				A submit control is required so the form is operable without relying
				on implicit Enter-key submission. Visually hidden because the icon's
				affordance already signals the form's purpose; the button is in the
				tab order and discoverable to assistive tech.
			-->
			<button type="submit" class="visually-hidden">Search</button>
		</form>

		<div class="topbar-actions">
			{#if me}
				<a class="topbar-identity" href="/u/{me.username}">
					<Icon name="user" size={18} />
					<span>{me.display_name}</span>
				</a>
				<form method="post" action="/logout" class="topbar-logout">
					<button type="submit" class="link-button">Log out</button>
				</form>
			{:else}
				<a class="topbar-action" href="/about/help" aria-label="Help">
					<Icon name="help" size={18} />
				</a>
				<a class="topbar-login" href="/login">Log in</a>
				<a class="topbar-login" href="/register">Sign up</a>
			{/if}
		</div>
	</div>
</header>
