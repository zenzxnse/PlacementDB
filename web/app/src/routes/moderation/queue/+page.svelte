<script lang="ts">
	import type { ActionData, PageData } from './$types';
	let { data, form }: { data: PageData; form: ActionData } = $props();
</script>
<svelte:head><title>Moderation queue: PlacementDB</title></svelte:head>
<h1>Moderation queue</h1>
{#if form?.message}<p class="notice" role="status">{form.message}</p>{/if}
{#if data.queue.items.length === 0}
	<p class="empty-state">The review queue is empty.</p>
{:else}
	{#each data.queue.items as item (`${item.target_type}:${item.public_id}`)}
		<article class="panel panel-body stack">
			<h2>{item.title}</h2>
			<p class="meta">{item.target_type} by {item.author.display_name} (@{item.author.username})</p>
			<form method="post" class="stack">
				<input type="hidden" name="_csrf" value={data.csrfToken} />
				<input type="hidden" name="public_id" value={item.public_id} />
				<input type="hidden" name="target_type" value={item.target_type} />
				<label for="reason-{item.public_id}">Reason</label>
				<textarea id="reason-{item.public_id}" name="reason" maxlength="1000" required></textarea>
				<div class="button-row">
					<button name="decision" value="approve" class="primary">Approve</button>
					<button name="decision" value="request_changes">Request changes</button>
					<button name="decision" value="reject">Reject</button>
				</div>
			</form>
		</article>
	{/each}
	{#if data.queue.next_cursor}<p><a href="?cursor={encodeURIComponent(data.queue.next_cursor)}">Next page</a></p>{/if}
{/if}
