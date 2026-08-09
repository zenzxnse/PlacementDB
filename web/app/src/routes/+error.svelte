<script lang="ts">
	import { page } from '$app/state';
	import { escapeLinkFor, safeRetryTarget } from '$lib/failure';

	/**
	 * Whole-page failure.
	 *
	 * Shows only the safe message the server chose plus a request ID when one
	 * exists. Never a stack, a path, or a response body.
	 */
	const status = $derived(page.status);
	const message = $derived(page.error?.message ?? 'Something went wrong.');
	const requestId = $derived(
		(page.error as { requestId?: string } | null)?.requestId ?? undefined
	);
	const serviceProblem = $derived(status >= 500);
	const retryTarget = $derived(safeRetryTarget(page.url));
	const escape = $derived(escapeLinkFor(page.url.pathname));

	const heading = $derived(
		status === 404
			? 'Not found'
			: status === 403
				? 'No access'
				: status === 429
					? 'Too many requests'
					: serviceProblem
						? 'Service unavailable'
						: 'Something went wrong'
	);
</script>

<svelte:head>
	<title>{heading}: PlacementDB</title>
	<!-- A failure page must never be indexed as content. -->
	<meta name="robots" content="noindex" />
</svelte:head>

<h1>{heading}</h1>

<div
	class="panel panel-body"
	role={serviceProblem ? 'status' : 'alert'}
	aria-live={serviceProblem ? 'polite' : 'assertive'}
	tabindex="-1"
>
	<p>{message}</p>

	{#if status === 404}
		<p class="meta">
			The link may be old, or the item may have been withdrawn by its author or a moderator.
		</p>
	{:else if serviceProblem}
		<p class="meta">This is a problem on our side, not with anything you did.</p>
	{/if}

	<p>
		{#if serviceProblem || status === 429}
			<a href={retryTarget}>Try again</a>
			<span aria-hidden="true"> &middot; </span>
		{/if}
		<a href={escape.href}>{escape.label}</a>
	</p>

	{#if requestId}
		<p class="meta">Reference: <code>{requestId}</code></p>
	{/if}
</div>
