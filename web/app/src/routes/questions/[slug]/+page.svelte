<script lang="ts">
	import Comments from '$lib/components/Comments.svelte';
	import DifficultyVote from '$lib/components/DifficultyVote.svelte';
	import ContentReport from '$lib/components/ContentReport.svelte';
	import Unavailable from '$lib/components/Unavailable.svelte';
	import type { ActionData, PageData } from './$types';
	import { formatDate, roundLabel, companyName } from '$lib/format';

	let { data, form }: { data: PageData; form: ActionData } = $props();

	const question = $derived(data.question);
	const retryCommentsHref = $derived(`/questions/${data.question.slug}#comments-heading`);

	/*
	 * A signed-in reader cannot rate their own submission. An anonymous author
	 * is null on the wire, so this check cannot see through anonymity; the API
	 * refuses that case and the form reports its refusal.
	 */
	const isOwnSubmission = $derived(
		!!data.me && !!question.author && question.author.username === data.me.username
	);
	/*
	 * The action supplies a return path only on the expired-session branch, so
	 * this narrows rather than assuming the field is on every action result.
	 */
	const voteLoginHref = $derived(
		form && 'voteLoginHref' in form && typeof form.voteLoginHref === 'string'
			? form.voteLoginHref
			: undefined
	);
	const loginHref = $derived(
		voteLoginHref ?? `/login?next=${encodeURIComponent(`/questions/${question.slug}`)}`
	);
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
		<dd>{question.source_year ?? 'Not recorded'}</dd>
		<dt>Published</dt>
		<dd>{formatDate(question.published_at)}</dd>
	</dl>
	<!--
		Difficulty is deliberately not repeated in the facts list. It changes
		when the reader votes, and a second copy sourced from the loaded page
		would still show the pre-vote score.
	-->

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

	<!--
		The rating shown here is the fresh one from a just-submitted vote when
		there is one, otherwise the loaded value. Nothing is computed on the
		client: the API returns the recomputed weighted score with the vote.
	-->
	<DifficultyVote
		difficulty={form?.difficulty ?? question.difficulty}
		me={data.me}
		csrfToken={data.csrfToken}
		myVote={form?.myVote ?? null}
		isOwnSubmission={isOwnSubmission}
		error={form?.voteError}
		loginHref={loginHref}
	/>

	<p class="meta">
		{#if question.author}
			Posted by {question.author.display_name} (@{question.author.username}) on
			{formatDate(question.published_at)}.
		{:else}
			Posted anonymously on {formatDate(question.published_at)}.
		{/if}
	</p>
</article>

{#if data.me}<ContentReport csrfToken={data.csrfToken} outcome={form?.contentReport} />{/if}

{#if data.commentsError}
	<!--
		Comments failed to load, but the question body above still rendered.
		Show an honest unavailable panel and offer to retry the same page;
		retrying the comment cursor would loop if the cursor itself is stale.
	-->
	<section class="comments" aria-labelledby="comments-unavailable">
		<h2 id="comments-unavailable">Comments</h2>
		<Unavailable
			failure={data.commentsError}
			retryHref={retryCommentsHref}
			escape={{ href: '/questions', label: 'Back to questions' }}
		/>
	</section>
{:else}
	<Comments
		comments={data.comments.items}
		nextCursor={data.comments.next_cursor}
		me={data.me}
		action="?/comment"
		csrfToken={data.csrfToken}
		error={form?.commentError}
		reportResult={form?.reportResult}
		hideResult={form?.hideResult}
	/>
{/if}
