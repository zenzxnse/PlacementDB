<script lang="ts">
	import { page } from '$app/state';

	const status = page.status;
	const message =
		page.error?.message ??
		(status === 404
			? 'The page you are looking for does not exist.'
			: 'Something went wrong on our side.');
</script>

<svelte:head>
	<title>{status}: PlacementDB</title>
</svelte:head>

<h1>{status}</h1>
<p>{message}</p>
{#if page.error?.request_id}
	<p class="meta">Request ID: {page.error.request_id}</p>
{/if}
<p><a href="/">Return to the home page</a> or <a href="/questions">browse questions</a>.</p>
