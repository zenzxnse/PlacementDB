<script lang="ts">
	import { DIFFICULTY_LEVELS, difficultySummary } from '$lib/format';
	import type { Difficulty, Me } from '$lib/types';

	/**
	 * Community difficulty rating and the vote control.
	 *
	 * A plain radio group in a real form, so it works with JavaScript disabled.
	 * The scale is 1 to 5 and the names come from the one exported mapping in
	 * $lib/format; nothing here hardcodes a level name.
	 *
	 * The control is disabled, with the reason stated in text, when the reader
	 * is anonymous, when the form token could not be issued, or when the reader
	 * is the question's author. The API enforces all three; this only explains
	 * them, because a disabled control with no reason is worse than no control.
	 */
	let {
		difficulty,
		me,
		csrfToken,
		myVote = null,
		isOwnSubmission = false,
		error,
		loginHref
	}: {
		difficulty: Difficulty;
		me: Me | null;
		csrfToken: string;
		myVote?: number | null;
		isOwnSubmission?: boolean;
		error?: string | undefined;
		loginHref: string;
	} = $props();

	const canVote = $derived(!!me && !isOwnSubmission && csrfToken.length > 0);
</script>

<section class="panel" aria-labelledby="difficulty-heading">
	<div class="panel-head" id="difficulty-heading">Difficulty</div>
	<div class="panel-body">
		<p>
			<strong>{difficultySummary(difficulty)}</strong>
		</p>
		{#if difficulty.vote_count === 0}
			<!--
				Every question sits at Standard until the community moves it. Saying
				so keeps the default from reading as a community judgement.
			-->
			<p class="meta">
				Unrated questions sit at Standard by default. Yours would be the first rating.
			</p>
		{/if}

		{#if error}
			<p class="inline-error" role="alert">{error}</p>
		{/if}

		{#if myVote !== null}
			<p class="notice notice-success" role="status">
				Your rating is saved. The score above already includes it.
			</p>
		{/if}

		{#if !me}
			<p class="meta">
				<a href={loginHref}>Log in</a> to rate this question.
			</p>
		{:else if isOwnSubmission}
			<p class="meta">You cannot rate a question you submitted.</p>
		{:else if csrfToken.length === 0}
			<p class="meta" role="status">
				Rating is temporarily unavailable. Reload the page and try again.
			</p>
		{:else}
			<form method="post" action="?/vote">
				<input type="hidden" name="_csrf" value={csrfToken} />
				<fieldset class="difficulty-scale">
					<legend>How hard was this question?</legend>
					{#each DIFFICULTY_LEVELS as level (level.value)}
						<label for="difficulty-{level.value}">
							<input
								type="radio"
								id="difficulty-{level.value}"
								name="value"
								value={level.value}
								checked={myVote === level.value}
							/>
							<span>{level.value}. {level.name}</span>
						</label>
					{/each}
				</fieldset>
				<div class="button-row">
					<button type="submit" disabled={!canVote}>
						{myVote === null ? 'Save rating' : 'Change rating'}
					</button>
				</div>
			</form>
		{/if}
	</div>
</section>

<style>
	.difficulty-scale {
		border: 1px solid var(--border);
		border-radius: var(--radius);
		padding: 0.5rem 0.75rem;
		display: flex;
		flex-direction: column;
		gap: 0.25rem;
	}

	.difficulty-scale legend {
		font-weight: 700;
		font-size: 0.9rem;
		padding: 0 0.25rem;
	}

	.difficulty-scale label {
		display: flex;
		align-items: center;
		gap: 0.5rem;
	}

	.difficulty-scale input {
		width: auto;
	}
</style>
