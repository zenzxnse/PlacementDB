<script lang="ts">
	import Comments from '$lib/components/Comments.svelte';
	import type { ActionData, PageData } from './$types';
	import { difficultySummary, formatDate, roundLabel , companyName } from '$lib/format';

	let { data, form }: { data: PageData; form: ActionData } = $props();

	const question = $derived(data.question);
</script>

<svelte:head>
	<title>{question.title}: PlacementDB</title>
</svelte:head>

<nav class="breadcrumbs" aria-label="Breadcrumb">
	<ol>
		<li><a href="/questions">Questions</a></li>
		<li aria-current="page">{question.title}</li>
	</ol>
</nav>

<article>
	<h1>{question.title}</h1>

	<dl class="facts">
		<dt>Company</dt>
		<dd>{companyName(question.company)}</dd>
		<dt>Role</dt>
		<dd>{question.role?.name ?? 'Not recorded'}</dd>
		<dt>Round</dt>
		<dd>{roundLabel(question.round)}</dd>
		<dt>Source year</dt>
		<dd>{question.source_year}</dd>
		<dt>Published</dt>
		<dd>{formatDate(question.published_at)}</dd>
		<dt>Difficulty</dt>
		<dd>{difficultySummary(question.difficulty)} (scale of 1 to 5)</dd>
	</dl>

	{#if question.topics.length > 0}
		<ul class="topics" aria-label="Topics">
			{#each question.topics as topic (topic.slug)}
				<li class="badge">{topic.name}</li>
			{/each}
		</ul>
	{/if}

	<div class="panel">
		<div class="panel-head">Prompt</div>
		<div class="panel-body prose-block">{question.prompt}</div>
	</div>

	{#if question.answer_guidance}
		<div class="panel">
			<div class="panel-head">Answer guidance</div>
			<div class="panel-body prose-block">{question.answer_guidance}</div>
		</div>
	{/if}

	<div class="panel">
		<div class="panel-head">Difficulty vote</div>
		<div class="panel-body">
			<p>
				Difficulty voting needs an account, and login is not connected yet. Once the API
				lands you will be able to rate this question from 1 to 5 here.
			</p>
		</div>
	</div>

	<p class="meta">
		{#if question.author}
			Posted by {question.author.display_name} (@{question.author.username}) on
			{formatDate(question.published_at)}.
		{:else}
			Posted anonymously on {formatDate(question.published_at)}.
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
