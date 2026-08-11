<script lang="ts">
	import type { PageData } from './$types';
	let { data }: { data: PageData } = $props();
</script>

<svelte:head><title>Companies: PlacementDB</title></svelte:head>
<h1>Companies</h1>
<p>Browse published questions and interview experiences by company.</p>
{#if data.result.items.length === 0}
	<p class="empty-state">No companies have published content yet.</p>
{:else}
	<table class="data">
		<caption class="visually-hidden">Companies and published content counts</caption>
		<thead><tr><th>Company</th><th>Questions</th><th>Experiences</th></tr></thead>
		<tbody>
			{#each data.result.items as company (company.slug)}
				<tr>
					<td>{company.name}</td>
					<td><a href="/questions?company={encodeURIComponent(company.slug)}">{company.question_count}</a></td>
					<td><a href="/experiences?company={encodeURIComponent(company.slug)}">{company.experience_count}</a></td>
				</tr>
			{/each}
		</tbody>
	</table>
{/if}
