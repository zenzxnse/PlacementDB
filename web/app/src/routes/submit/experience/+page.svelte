<script lang="ts">
	import type { ActionData, PageData } from './$types';
	import { outcomeLabel, roundLabel } from '$lib/format';
	import { ROUND_VALUES, type Outcome } from '$lib/types';
	import { NARRATIVE_MAX, NARRATIVE_MIN, TITLE_MAX } from '$lib/submission';
	import Unavailable from '$lib/components/Unavailable.svelte';

	let { data, form }: { data: PageData; form: ActionData } = $props();

	const errors = $derived(form?.errors ?? []);
	const draft = $derived(form?.draft);

	function errorFor(field: string): string | undefined {
		return errors.find((e) => e.field === field)?.message;
	}

	const titleError = $derived(errorFor('title'));
	const narrativeError = $derived(errorFor('narrative'));
	const yearError = $derived(errorFor('source_year'));
	const outcomeError = $derived(errorFor('outcome'));

	const companies = $derived(data.options?.companies ?? []);
	const roles = $derived(data.options?.roles ?? []);

	/*
	 * A fixed number of round slots. Adding a row on demand needs JavaScript,
	 * and this form has to work without it, so the slots are here from the
	 * start and blank ones are dropped server-side. Five covers the usual
	 * process; the contract allows twenty, and that ceiling is the API's.
	 */
	const SLOTS = 5;
	const slots = $derived(
		Array.from({ length: SLOTS }, (_, i) => draft?.rounds?.[i] ?? { round: '', notes: '' })
	);

	const OUTCOMES: readonly Outcome[] = ['offered', 'rejected', 'withdrew', 'unknown'];
</script>

<svelte:head>
	<title>Share an experience: PlacementDB</title>
</svelte:head>

<h1>Share a placement experience</h1>

{#if data.submitted}
	<p class="notice notice-success" role="status">
		Thanks. Your experience is with the moderators and is not visible to anyone else until one of
		them approves it.
	</p>
{/if}

{#if !data.me.can_submit}
	<p class="notice" role="status">
		This account cannot submit content right now. If you think that is wrong, contact a
		moderator through <a href="/about/help">Help</a>.
	</p>
{:else}
	<p>
		Describe the process as you went through it. Do not name interviewers or other students, and
		do not paste anything you signed an agreement not to share.
	</p>

	{#if data.optionsError}
		<Unavailable
			failure={data.optionsError}
			retryHref="/submit/experience"
			escape={{ href: '/experiences', label: 'Back to experiences' }}
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

	<form method="post" action="/submit/experience" class="panel panel-body" novalidate>
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
			<label for="narrative">What happened</label>
			<textarea
				id="narrative"
				name="narrative"
				rows="12"
				aria-invalid={narrativeError ? true : undefined}
				aria-describedby={narrativeError ? 'narrative-error narrative-help' : 'narrative-help'}
				required>{draft?.narrative ?? ''}</textarea
			>
			<p class="meta" id="narrative-help">
				{NARRATIVE_MIN} to {NARRATIVE_MAX} characters. Leave a blank line between paragraphs;
				that is the only formatting the page reads.
			</p>
			{#if narrativeError}<p class="inline-error" id="narrative-error">{narrativeError}</p>{/if}
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
				<label for="source_year">Year</label>
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

		<fieldset class="rounds">
			<legend>Rounds, in order</legend>
			<p class="meta">
				Fill in as many as you went through and leave the rest blank. The order here is the
				order shown on the page.
			</p>
			{#each slots as slot, index (index)}
				{@const slotError = errorFor(`round_${index}`)}
				<div class="round-slot" id="round_{index}">
					<div class="field">
						<label for="round-{index}">Round {index + 1}</label>
						<select
							id="round-{index}"
							name="round"
							aria-invalid={slotError ? true : undefined}
							aria-describedby={slotError ? `round_${index}-error` : undefined}
						>
							<option value="">Not used</option>
							{#each ROUND_VALUES as round (round)}
								<option value={round} selected={slot.round === round}>{roundLabel(round)}</option>
							{/each}
						</select>
					</div>
					<div class="field">
						<label for="round-notes-{index}">Notes (optional)</label>
						<textarea id="round-notes-{index}" name="round_notes" rows="2"
							>{slot.notes ?? ''}</textarea
						>
					</div>
					{#if slotError}<p class="inline-error" id="round_{index}-error">{slotError}</p>{/if}
				</div>
			{/each}
		</fieldset>

		<fieldset class="rounds">
			<legend>Outcome and attribution</legend>
			<div class="field checkbox-field">
				<label for="outcome_visible">
					<input
						type="checkbox"
						id="outcome_visible"
						name="outcome_visible"
						checked={draft ? draft.outcome_visible : true}
					/>
					<span>Show the outcome publicly</span>
				</label>
				<p class="meta">
					Unchecked hides it entirely. A hidden outcome is omitted, not shown as "Unknown", so
					nobody can infer it from the page.
				</p>
			</div>

			<div class="field">
				<label for="outcome">Outcome</label>
				<select
					id="outcome"
					name="outcome"
					aria-invalid={outcomeError ? true : undefined}
					aria-describedby={outcomeError ? 'outcome-error' : undefined}
				>
					<option value="">Choose one</option>
					{#each OUTCOMES as outcome (outcome)}
						<option value={outcome} selected={draft?.outcome === outcome}
							>{outcomeLabel(outcome)}</option
						>
					{/each}
				</select>
				{#if outcomeError}<p class="inline-error" id="outcome-error">{outcomeError}</p>{/if}
			</div>

			<div class="field checkbox-field">
				<label for="anonymous">
					<input
						type="checkbox"
						id="anonymous"
						name="anonymous"
						checked={draft?.anonymous ?? false}
					/>
					<span>Post anonymously</span>
				</label>
				<p class="meta">Your username is not shown on the page. Moderators still see it.</p>
			</div>
		</fieldset>

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
			<a class="button" href="/experiences">Cancel</a>
		</div>
	</form>
{/if}

<style>
	.rounds {
		border: 1px solid var(--border);
		border-radius: var(--radius);
		padding: 0.5rem 0.75rem;
		margin: 0.75rem 0;
	}

	.rounds legend {
		font-weight: 700;
		font-size: 0.9rem;
		padding: 0 0.25rem;
	}

	.round-slot {
		display: grid;
		grid-template-columns: minmax(10rem, 1fr) minmax(12rem, 2fr);
		gap: 0.5rem 1rem;
		padding: 0.5rem 0;
		border-top: 1px solid var(--border);
	}

	.round-slot:first-of-type {
		border-top: 0;
	}

	.checkbox-field label {
		display: flex;
		align-items: center;
		gap: 0.5rem;
	}

	.checkbox-field input {
		width: auto;
	}
</style>
