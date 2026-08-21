<script lang="ts">
	import {
		COMMENT_MAX_LENGTH,
		REPORT_REASON_MAX_LENGTH,
		REPORT_REASON_LABELS,
		REPORT_REASON_VALUES,
		type Comment,
		type CommentActionOutcome,
		type Me
	} from '$lib/types';
	import Avatar from './Avatar.svelte';
	import { formatDate } from '$lib/format';

	/**
	 * Comment list, comment form, per-comment reporting, and moderator hiding.
	 *
	 * Bodies are plain text and render through ordinary Svelte text bindings, so
	 * they are escaped by default. Nothing here uses {@html}.
	 *
	 * Every control is a real form posting to a named action, so all of it
	 * works without JavaScript. The report and hide forms sit inside native
	 * details elements, which disclose without script. Hidden and deleted
	 * comments never reach this component: the API omits them rather than
	 * sending a tombstone.
	 *
	 * The comment's public ID travels in a hidden field. Unlike the difficulty
	 * vote it cannot be derived from the page slug, and the API re-checks
	 * visibility and authorization on every report, so a forged ID only names
	 * a target the API refuses.
	 */
	let {
		comments,
		me,
		action,
		csrfToken,
		error,
		nextCursor,
		reportAction = '?/report',
		hideAction = '?/hide',
		reportResult = undefined,
		hideResult = undefined
	}: {
		comments: Comment[];
		me: Me | null;
		action: string;
		csrfToken: string;
		error?: string | undefined;
		nextCursor?: string | null;
		reportAction?: string;
		hideAction?: string;
		reportResult?: CommentActionOutcome | undefined;
		hideResult?: CommentActionOutcome | undefined;
	} = $props();
</script>

<section class="comments" aria-labelledby="comments-heading">
	<h2 id="comments-heading">Comments</h2>

	{#if comments.length === 0}
		<p class="meta">No comments yet.</p>
	{:else}
		<ul class="comment-list">
			{#each comments as comment (comment.public_id)}
				{@const outcome =
					reportResult?.commentId === comment.public_id
						? reportResult
						: hideResult?.commentId === comment.public_id
							? hideResult
							: undefined}
				<li class="comment">
					<Avatar src={comment.author?.avatar_url} name={comment.author?.display_name ?? 'Anonymous'} size={32} />
					<div class="comment-body">
						<p class="meta">
							{#if comment.author}
								<a href="/u/{comment.author.username}">{comment.author.display_name}</a>
							{:else}
								Anonymous
							{/if}
							&middot; {formatDate(comment.created_at)}
						</p>
						<!-- Plain text, escaped by Svelte. Line breaks preserved by CSS. -->
						<p class="comment-text">{comment.body}</p>
						{#if outcome}
							<p
								class={outcome.kind === 'error' ? 'inline-error' : 'notice notice-success'}
								role={outcome.kind === 'error' ? 'alert' : 'status'}
							>
								{outcome.message}
								{#if outcome.kind === 'error' && outcome.loginHref}
									<a href={outcome.loginHref}>Log in again</a>.
								{/if}
							</p>
						{/if}
						{#if comment.can_report}
							<details class="comment-tool">
								<summary>Report</summary>
								<form method="post" action={reportAction} class="stack">
									<input type="hidden" name="_csrf" value={csrfToken} />
									<input type="hidden" name="comment_id" value={comment.public_id} />
									<fieldset class="report-reasons">
										<legend>Why are you reporting this comment?</legend>
										{#each REPORT_REASON_VALUES as reason, index (reason)}
											<label for="report-{comment.public_id}-{reason}">
												<input
													type="radio"
													id="report-{comment.public_id}-{reason}"
													name="reason"
													value={reason}
													required={index === 0}
												/>
												<span>{REPORT_REASON_LABELS[reason]}</span>
											</label>
										{/each}
									</fieldset>
									<div class="field">
										<label for="report-details-{comment.public_id}">Details (optional)</label>
										<textarea
											id="report-details-{comment.public_id}"
											name="details"
											rows="2"
											maxlength={REPORT_REASON_MAX_LENGTH}
											aria-describedby="report-details-help-{comment.public_id}"
										></textarea>
										<p class="meta" id="report-details-help-{comment.public_id}">
											Plain text, at most {REPORT_REASON_MAX_LENGTH} characters. A moderator reads every report.
										</p>
									</div>
									<div class="button-row">
										<button type="submit" class="primary" disabled={!csrfToken}>Report comment</button>
									</div>
								</form>
							</details>
						{/if}
						{#if me?.can_moderate}
							<!--
								Moderator hide. The API requires a reason and records it in the
								audit trail; so does this form. Authorization is re-checked by the
								action and again by the API, so the link being absent for others
								is courtesy, not the enforcement.
							-->
							<details class="comment-tool">
								<summary>Hide (moderation)</summary>
								<form method="post" action={hideAction} class="stack">
									<input type="hidden" name="_csrf" value={csrfToken} />
									<input type="hidden" name="comment_id" value={comment.public_id} />
									<div class="field">
										<label for="hide-reason-{comment.public_id}">Reason</label>
										<textarea
											id="hide-reason-{comment.public_id}"
											name="reason"
											rows="2"
											maxlength={REPORT_REASON_MAX_LENGTH}
											required
										></textarea>
									</div>
									<div class="button-row">
										<button type="submit" disabled={!csrfToken}>Hide comment</button>
									</div>
								</form>
							</details>
						{/if}
					</div>
				</li>
			{/each}
		</ul>

		{#if nextCursor}
			<p><a href="?after={encodeURIComponent(nextCursor)}#comments-heading">Older comments</a></p>
		{/if}
	{/if}

	{#if me}
		{#if error}
			<!--
				Error summary: an explicit heading inside lets a screen reader
				user locate the failure as the next heading inside the section.
				tabindex="-1" makes it programmatically focusable.
			-->
			<div class="error-summary" role="alert" tabindex="-1">
				<h2>Comment was not posted</h2>
				<p>{error}</p>
			</div>
		{/if}
		<form method="post" {action} class="stack">
			<input type="hidden" name="_csrf" value={csrfToken} />
			<div class="field">
				<label for="comment-body">Add a comment</label>
				<textarea
					id="comment-body"
					name="body"
					rows="4"
					maxlength={COMMENT_MAX_LENGTH}
					required
					aria-describedby="comment-help"
				></textarea>
				<p class="meta" id="comment-help">
					Plain text, up to {COMMENT_MAX_LENGTH} characters. Comments appear immediately and
					are reviewed afterwards.
				</p>
			</div>
			<div class="button-row">
				<button type="submit" class="primary" disabled={!csrfToken}>Post comment</button>
			</div>
		</form>
	{:else}
		<p class="meta"><a href="/login">Log in</a> to comment.</p>
	{/if}
</section>

<style>
	.comment-tool {
		margin-top: 0.35rem;
		border: 1px solid var(--border);
	}

	.comment-tool summary {
		cursor: pointer;
		padding: 0.2rem 0.5rem;
		font-size: 0.85rem;
		color: var(--ink-muted, inherit);
		width: fit-content;
	}

	.comment-tool[open] summary {
		border-bottom: 1px solid var(--border);
	}

	.comment-tool form {
		padding: 0.5rem 0.75rem;
	}

	.report-reasons {
		border: 1px solid var(--border);
		padding: 0.35rem 0.6rem;
		display: flex;
		flex-direction: column;
		gap: 0.2rem;
	}

	.report-reasons legend {
		font-size: 0.9rem;
		font-weight: 700;
		padding: 0 0.25rem;
	}

	.report-reasons label {
		display: flex;
		align-items: center;
		gap: 0.5rem;
	}

	.report-reasons input {
		width: auto;
	}
</style>
