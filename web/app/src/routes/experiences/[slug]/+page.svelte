<script lang="ts">
	import type { PageData } from './$types';
	import { formatDate, outcomeLabel, roundLabel } from '$lib/format';

	let { data }: { data: PageData } = $props();

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
		<dd>{experience.company.name}</dd>
		<dt>Role</dt>
		<dd>{experience.role}</dd>
		<dt>Year</dt>
		<dd>{experience.source_year}</dd>
		<dt>Outcome</dt>
		<dd>
			{experience.outcome === null ? 'Hidden by the author' : outcomeLabel(experience.outcome)}
		</dd>
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
							<td>{round.name}</td>
							<td>{roundLabel(round.round)}</td>
							<td>{round.summary}</td>
						</tr>
					{/each}
				</tbody>
			</table>
		</div>
	{/if}

	{#each experience.narrative as paragraph}
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
