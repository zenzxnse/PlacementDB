<script lang="ts">
	import type { ActionData, PageData } from './$types';
	import { roundLabel } from '$lib/format';
	import { ROUND_VALUES } from '$lib/types';
	import { GUIDANCE_MAX, PROMPT_MAX, PROMPT_MIN, TITLE_MAX, TOPICS_MAX } from '$lib/submission';
	import Unavailable from '$lib/components/Unavailable.svelte';

	let { data, form }: { data: PageData; form: ActionData } = $props();

	const errors = $derived(form?.errors ?? []);
	const draft = $derived(form?.draft);

	function errorFor(field: string): string | undefined {
		return errors.find((e) => e.field === field)?.message;
	}

	const titleError = $derived(errorFor('title'));
	const promptError = $derived(errorFor('prompt'));
	const guidanceError = $derived(errorFor('answer_guidance'));
	const yearError = $derived(errorFor('source_year'));
	const roundError = $derived(errorFor('round'));
	const topicsError = $derived(errorFor('topic_slugs'));

	const topics = $derived(data.options?.topics ?? []);
	const companies = $derived(data.options?.companies ?? []);
	const roles = $derived(data.options?.roles ?? []);
	const chosenTopics = $derived(new Set(draft?.topic_slugs ?? []));
</script>

<svelte:head>
	<title>Submit a question: PlacementDB</title>
</svelte:head>

<h1>Submit a question</h1>

{#if data.submitted}
	<p class="notice notice-success" role="status">
		Thanks. Your question is with the moderators. It appears publicly once one of them approves
		it, and it is not visible to anyone else until then.
	</p>
{/if}

{#if !data.me.can_submit}
	<!--
		Gated on the capability the API computed, never on a role string. A
		suspended account lands here rather than on a form whose post is refused.
	-->
	<p class="notice" role="status">
		This account cannot submit content right now. If you think that is wrong, contact a
		moderator through <a href="/about/help">Help</a>.
	</p>
{:else}
	<p>
		Write the question the way it was actually asked. Every submission goes to a moderator before
		it appears, and imported or edited records keep their original wording.
	</p>

	{#if data.optionsError}
		<!--
			The vocabulary lists failed to load. The form still renders, because
			company, role, and topics are all optional; only the pickers are gone.
		-->
		<Unavailable
			failure={data.optionsError}
			retryHref="/submit/question"
			escape={{ href: '/questions', label: 'Back to questions' }}
		/>
	{/if}

	{#if !data.csrfToken}
		<p class="notice" role="status">
			Submission is temporarily unavailable. Reload the page in a moment and try again.
		</p>
	{/if}

	{#if errors.length > 0}
		<div class="error-summary" role="alert" tabindex="-1" id="error-summary">
			<h2>There is a problem</h2>
			<ul>
				{#each errors as err}
					<li>
						{#if err.field === 'form'}
							{err.message}
						{:else}
							<a href="#{err.field}">{err.message}</a>
						{/if}
					</li>
				{/each}
			</ul>
		</div>
	{/if}

	<form method="post" action="/submit/question" class="panel panel-body" novalidate>
		<input type="hidden" name="_csrf" value={data.csrfToken} />

		<div class="field">
			<label for="title">Title</label>
			<input
				type="text"
				id="title"
				name="title"
				value={draft?.title ?? ''}
				maxlength={TITLE_MAX}
				aria-invalid={titleError ? true : undefined}
				aria-describedby={titleError ? 'title-error' : undefined}
				required
			/>
			{#if titleError}<p class="inline-error" id="title-error">{titleError}</p>{/if}
		</div>

		<div class="field">
			<label for="prompt">The question</label>
			<textarea
				id="prompt"
				name="prompt"
				rows="6"
				aria-invalid={promptError ? true : undefined}
				aria-describedby={promptError ? 'prompt-error prompt-help' : 'prompt-help'}
				required>{draft?.prompt ?? ''}</textarea
			>
			<p class="meta" id="prompt-help">
				{PROMPT_MIN} to {PROMPT_MAX} characters. Plain text; formatting marks are shown as typed.
			</p>
			{#if promptError}<p class="inline-error" id="prompt-error">{promptError}</p>{/if}
		</div>

		<div class="field">
			<label for="answer_guidance">Answer guidance (optional)</label>
			<textarea
				id="answer_guidance"
				name="answer_guidance"
				rows="4"
				aria-invalid={guidanceError ? true : undefined}
				aria-describedby={guidanceError ? 'answer_guidance-error answer_guidance-help' : 'answer_guidance-help'}
				>{draft?.answer_guidance ?? ''}</textarea
			>
			<p class="meta" id="answer_guidance-help">
				What a good answer covered, up to {GUIDANCE_MAX} characters. Leave blank if you would rather not say.
			</p>
			{#if guidanceError}
				<p class="inline-error" id="answer_guidance-error">{guidanceError}</p>
			{/if}
		</div>

		<div class="filters">
			<div class="field">
				<label for="company_slug">Company</label>
				<select id="company_slug" name="company_slug">
					<option value="">Not recorded</option>
					{#each companies as company (company.slug)}
						<option value={company.slug} selected={draft?.company_slug === company.slug}
							>{company.name}</option
						>
					{/each}
				</select>
			</div>

			<div class="field">
				<label for="job_role_slug">Role</label>
				<select id="job_role_slug" name="job_role_slug">
					<option value="">Not recorded</option>
					{#each roles as role (role.slug)}
						<option value={role.slug} selected={draft?.job_role_slug === role.slug}
							>{role.name}</option
						>
					{/each}
				</select>
			</div>

			<div class="field">
				<label for="round">Round</label>
				<select
					id="round"
					name="round"
					aria-invalid={roundError ? true : undefined}
					aria-describedby={roundError ? 'round-error' : undefined}
				>
					<option value="">Not recorded</option>
					{#each ROUND_VALUES as round (round)}
						<option value={round} selected={draft?.round === round}>{roundLabel(round)}</option>
					{/each}
				</select>
				{#if roundError}<p class="inline-error" id="round-error">{roundError}</p>{/if}
			</div>

			<div class="field">
				<label for="source_year">Year asked</label>
				<input
					type="text"
					inputmode="numeric"
					id="source_year"
					name="source_year"
					value={draft?.source_year ?? ''}
					aria-invalid={yearError ? true : undefined}
					aria-describedby={yearError ? 'source_year-error' : undefined}
				/>
				{#if yearError}<p class="inline-error" id="source_year-error">{yearError}</p>{/if}
			</div>
		</div>

		{#if topics.length > 0}
			<fieldset class="topic-picker" id="topic_slugs">
				<legend>Topics (up to {TOPICS_MAX})</legend>
				{#if topicsError}<p class="inline-error" id="topic_slugs-error">{topicsError}</p>{/if}
				<div class="topic-grid">
					{#each topics as topic (topic.slug)}
						<label for="topic-{topic.slug}">
							<input
								type="checkbox"
								id="topic-{topic.slug}"
								name="topic_slugs"
								value={topic.slug}
								checked={chosenTopics.has(topic.slug)}
							/>
							<span>{topic.name}</span>
						</label>
					{/each}
				</div>
			</fieldset>
		{/if}

		<div class="button-row">
			<button
				type="submit"
				class="primary"
				disabled={!data.csrfToken}
				onclick={(event) => {
					const button = event.currentTarget as HTMLButtonElement;
					if (button.dataset.submitted === 'true') {
						event.preventDefault();
						return;
					}
					button.dataset.submitted = 'true';
				}}>Send for review</button
			>
			<a class="button" href="/questions">Cancel</a>
		</div>
	</form>
{/if}

<style>
	.topic-picker {
		border: 1px solid var(--border);
		border-radius: var(--radius);
		padding: 0.5rem 0.75rem;
		margin: 0.75rem 0;
	}

	.topic-picker legend {
		font-weight: 700;
		font-size: 0.9rem;
		padding: 0 0.25rem;
	}

	.topic-grid {
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(12rem, 1fr));
		gap: 0.25rem 1rem;
	}

	.topic-grid label {
		display: flex;
		align-items: center;
		gap: 0.5rem;
	}

	.topic-grid input {
		width: auto;
	}
</style>
