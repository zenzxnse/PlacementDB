<script lang="ts">
	import type { Failure } from '$lib/failure';

	/**
	 * Compact panel for one failed section or page region.
	 *
	 * role="status" for a service problem the user did not cause, role="alert"
	 * for something actionable. A service outage announced as an alert trains
	 * people to ignore alerts.
	 */
	let {
		failure,
		retryHref,
		escape
	}: {
		failure: Failure;
		retryHref?: string;
		escape?: { href: string; label: string };
	} = $props();
</script>

<div
	class="unavailable"
	role={failure.serviceProblem ? 'status' : 'alert'}
	aria-live={failure.serviceProblem ? 'polite' : 'assertive'}
>
	<p class="unavailable-message">{failure.message}</p>

	{#if failure.retryAfterSeconds}
		<p class="meta">Try again in about {failure.retryAfterSeconds} seconds.</p>
	{/if}

	<p class="unavailable-actions">
		{#if retryHref && failure.retryable}
			<!-- An ordinary link, so retry works with JavaScript disabled. -->
			<a href={retryHref}>Try again</a>
		{/if}
		{#if escape}
			{#if retryHref && failure.retryable}<span aria-hidden="true"> &middot; </span>{/if}
			<a href={escape.href}>{escape.label}</a>
		{/if}
	</p>

	{#if failure.requestId}
		<!-- Selectable so a user can quote it in a report. -->
		<p class="meta">Reference: <code>{failure.requestId}</code></p>
	{/if}
</div>
