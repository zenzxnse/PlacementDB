<script lang="ts">
	import Icon from './Icon.svelte';
	import type { IconName } from '$lib/icons';
	import type { Me } from '$lib/types';

	/**
	 * Primary navigation.
	 *
	 * Entries are gated on the capability booleans the API computed, never on a
	 * role string. Rendered as a real list of links so the whole panel works
	 * without JavaScript, and marked with aria-current for the active route.
	 */
	let { me, pathname }: { me: Me | null; pathname: string } = $props();

	interface Item {
		href: string;
		label: string;
		icon: IconName;
	}

	const browse: Item[] = [
		{ href: '/', label: 'Home', icon: 'home' },
		{ href: '/questions', label: 'Questions', icon: 'questions' },
		{ href: '/experiences', label: 'Experiences', icon: 'experiences' },
		{ href: '/topics', label: 'Topics', icon: 'tag' },
		{ href: '/companies', label: 'Companies', icon: 'companies' }
	];

	const personal = $derived<Item[]>(
		me
			? [
					{ href: '/submit/question', label: 'Submit', icon: 'submit' },
					{ href: '/account/saved', label: 'Saved', icon: 'saved' },
					{ href: '/account/activity', label: 'Your activity', icon: 'activity' }
				]
			: []
	);

	const moderation = $derived<Item[]>(
		me?.can_moderate
			? [
					{ href: '/moderation/queue', label: 'Review queue', icon: 'moderation' },
					{ href: '/moderation/reports', label: 'Reports', icon: 'reports' },
					{ href: '/moderation/audit', label: 'Audit log', icon: 'audit' }
				]
			: []
	);

	function isCurrent(href: string): boolean {
		if (href === '/') return pathname === '/';
		return pathname === href || pathname.startsWith(`${href}/`);
	}
</script>

<nav class="sidenav" aria-label="Primary">
	<ul class="sidenav-group">
		{#each browse as item}
			<li>
				<a href={item.href} aria-current={isCurrent(item.href) ? 'page' : undefined}>
					<Icon name={item.icon} />
					<span>{item.label}</span>
				</a>
			</li>
		{/each}
	</ul>

	{#if personal.length > 0}
		<p class="sidenav-heading" id="sidenav-you">You</p>
		<ul class="sidenav-group" aria-labelledby="sidenav-you">
			{#each personal as item}
				<li>
					<a href={item.href} aria-current={isCurrent(item.href) ? 'page' : undefined}>
						<Icon name={item.icon} />
						<span>{item.label}</span>
					</a>
				</li>
			{/each}
		</ul>
	{/if}

	{#if moderation.length > 0}
		<p class="sidenav-heading" id="sidenav-moderation">Moderation</p>
		<ul class="sidenav-group" aria-labelledby="sidenav-moderation">
			{#each moderation as item}
				<li>
					<a href={item.href} aria-current={isCurrent(item.href) ? 'page' : undefined}>
						<Icon name={item.icon} />
						<span>{item.label}</span>
						{#if item.href === '/moderation/queue' && me && me.unread_moderation_count > 0}
							<span class="badge-count">{me.unread_moderation_count}</span>
						{/if}
					</a>
				</li>
			{/each}
		</ul>
	{/if}

	<!--
		Static explainer rather than a dismissible promo. A dismissal that does
		not persist is worse than no dismissal, and persisting one needs a
		backend preference that does not exist.
	-->
	<aside class="sidenav-note">
		<p class="sidenav-heading">About this archive</p>
		<p>
			Every question and experience here is reviewed by a moderator before it appears.
			Imported records stay anonymous until an author claims them.
		</p>
		<a href="/about/moderation">How moderation works</a>
	</aside>
</nav>
