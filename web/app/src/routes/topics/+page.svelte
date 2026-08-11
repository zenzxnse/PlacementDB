<script lang="ts">
	import type { PageData } from './$types';
	let { data }: { data: PageData } = $props();
</script>

<svelte:head><title>Topics: PlacementDB</title></svelte:head>
<h1>Topics</h1>
<p>Browse published placement questions by subject.</p>
{#if data.result.items.length === 0}
	<p class="empty-state">No topics have published questions yet.</p>
{:else}
	<table class="data">
		<caption class="visually-hidden">Topics and published content counts</caption>
		<thead><tr><th>Topic</th><th>Questions</th></tr></thead>
		<tbody>
			{#each data.result.items as topic (topic.slug)}
				<tr><td><a href="/questions?topic={encodeURIComponent(topic.slug)}">{topic.name}</a></td><td>{topic.question_count}</td></tr>
			{/each}
		</tbody>
	</table>
{/if}
