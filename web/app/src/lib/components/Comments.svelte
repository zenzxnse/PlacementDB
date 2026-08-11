<script lang="ts">
	import { COMMENT_MAX_LENGTH, type Comment, type Me } from '$lib/types';
	import Avatar from './Avatar.svelte';
	import { formatDate } from '$lib/format';

	/**
	 * Comment list and form.
	 *
	 * Bodies are plain text and render through ordinary Svelte text bindings, so
	 * they are escaped by default. Nothing here uses {@html}.
	 *
	 * The form is a normal POST to a named action, so commenting works without
	 * JavaScript. Hidden and deleted comments never reach this component: the
	 * API omits them rather than sending a tombstone.
	 */
	let {
		comments,
		me,
		action,
		csrfToken,
		error,
		nextCursor
	}: {
		comments: Comment[];
		me: Me | null;
		action: string;
		csrfToken: string;
		error?: string | undefined;
		nextCursor?: string | null;
	} = $props();
</script>

<section class="comments" aria-labelledby="comments-heading">
	<h2 id="comments-heading">Comments</h2>

	{#if comments.length === 0}
		<p class="meta">No comments yet.</p>
	{:else}
		<ul class="comment-list">
			{#each comments as comment (comment.public_id)}
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
						{#if comment.can_report}
							<p class="meta">
								<a href="/report/comment/{comment.public_id}">Report</a>
							</p>
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
