<script lang="ts">
	import Comments from '$lib/components/Comments.svelte';
	import type { ActionData, PageData } from './$types';
	import { formatDate, roundLabel, companyName, visibleOutcome, splitParagraphs } from '$lib/format';

	let { data, form }: { data: PageData; form: ActionData } = $props();

	const experience = $derived(data.experience);
</script>

<svelte:head>
	<title>{experience.title}: PlacementDB</title>
</svelte:head>

<nav class="breadcrumbs" aria-label="Breadcrumb">
	<ol>
		<li><a href="/experiences">Experiences</a></li>
		<li aria-current="page">{experience.title}</li>
	</ol>
</nav>

<article>
	<h1>{experience.title}</h1>

	<dl class="facts">
		<dt>Company</dt>
		<dd>{companyName(experience.company)}</dd>
		<dt>Role</dt>
		<dd>{experience.role?.name ?? 'Not recorded'}</dd>
		<dt>Year</dt>
		<dd>{experience.source_year}</dd>
		{#if visibleOutcome(experience)}
			<dt>Outcome</dt>
			<dd>{visibleOutcome(experience)}</dd>
		{/if}
		<dt>Published</dt>
		<dd>{formatDate(experience.published_at)}</dd>
	</dl>

	{#if experience.rounds.length > 0}
		<div class="panel panel-flush">
			<div class="panel-head">Rounds</div>
			<table class="data">
				<caption class="visually-hidden">Interview rounds for this experience</caption>
				<thead>
					<tr>
						<th scope="col">Round</th>
						<th scope="col">Type</th>
						<th scope="col">Summary</th>
					</tr>
				</thead>
				<tbody>
					{#each experience.rounds as round}
						<tr>
							<td>{round.ordinal}</td>
							<td>{roundLabel(round.round)}</td>
							<td>{round.notes ?? ''}</td>
						</tr>
					{/each}
				</tbody>
			</table>
		</div>
	{/if}

	<!--
		Split on blank lines for presentation only. The stored text is one
		string and is never modified, so the author's exact wording survives.
	-->
	{#each splitParagraphs(experience.narrative) as paragraph}
		<p>{paragraph}</p>
	{/each}

	<p class="meta">
		{#if experience.author}
			Posted by {experience.author.display_name} (@{experience.author.username}) on
			{formatDate(experience.published_at)}.
		{:else}
			Posted anonymously on {formatDate(experience.published_at)}.
		{/if}
	</p>
</article>

<Comments
	comments={data.comments.items}
	nextCursor={data.comments.next_cursor}
	me={data.me}
	action="?/comment"
	csrfToken={data.csrfToken}
	error={form?.commentError}
/>
