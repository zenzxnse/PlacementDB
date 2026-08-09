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
			<a href="/companies">Companies</a>
		</nav>

		<!-- GET so a search is a shareable URL and works with JavaScript off. -->
		<form class="topbar-search" method="get" action="/search" role="search">
			<label class="visually-hidden" for="global-search">Search questions and experiences</label>
			<span class="topbar-search-icon"><Icon name="search" size={17} /></span>
			<input
				id="global-search"
				type="search"
				name="q"
				value={query}
				placeholder="Search questions, companies, topics"
				autocomplete="off"
			/>
		</form>

		<div class="topbar-actions">
			{#if me}
				<a class="topbar-action" href="/account/inbox">
					<Icon name="bell" size={18} label="Inbox" />
				</a>
				<a class="topbar-action" href="/account/activity">
					<Icon name="trophy" size={18} label="Your activity" />
				</a>
				<a class="topbar-identity" href="/u/{me.username}">
					<Icon name="user" size={18} />
					<span>{me.display_name}</span>
				</a>
				<form method="post" action="/logout" class="topbar-logout">
					<button type="submit" class="link-button">Log out</button>
				</form>
			{:else}
				<a class="topbar-action" href="/about/help">
					<Icon name="help" size={18} label="Help" />
				</a>
				<a class="topbar-login" href="/login">Log in</a>
			{/if}
		</div>
	</div>
</header>
