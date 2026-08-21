<script lang="ts">
	import type { PageData } from './$types';
	import { difficultyName, formatDate } from '$lib/format';
	let { data }: { data: PageData } = $props();
</script>

<svelte:head><title>Your activity: PlacementDB</title></svelte:head>
<h1>Your activity</h1>
<nav class="tabs" aria-label="Activity sections">
	<a href="?section=submissions" aria-current={data.section === 'submissions' ? 'page' : undefined}>Submissions</a>
	<a href="?section=votes" aria-current={data.section === 'votes' ? 'page' : undefined}>Votes</a>
	<a href="?section=reports" aria-current={data.section === 'reports' ? 'page' : undefined}>Reports</a>
</nav>

{#if data.page.items.length === 0}<p class="empty">No activity in this section yet.</p>{:else}
	<div class="panel panel-flush"><table class="data"><thead><tr>
		{#if data.section === 'submissions'}<th>Item</th><th>State</th><th>Updated</th>
		{:else if data.section === 'votes'}<th>Question</th><th>Rating</th><th>Updated</th>
		{:else}<th>Target</th><th>Reason</th><th>State</th><th>Reported</th>{/if}
	</tr></thead><tbody>
	{#each data.page.items as item}
		<tr>
		{#if data.section === 'submissions' && 'kind' in item}
			<td>{#if item.state === 'published'}<a href="/{item.kind === 'question' ? 'questions' : 'experiences'}/{item.slug}">{item.title}</a>{:else}{item.title}{/if}</td><td>{item.state.replaceAll('_', ' ')}</td><td>{formatDate(item.updated_at)}</td>
		{:else if data.section === 'votes' && 'value' in item}
			<td>{#if item.target}<a href="/questions/{item.target.slug}">{item.target.title}</a>{:else}Unavailable question{/if}</td><td>{difficultyName(item.value)}</td><td>{formatDate(item.updated_at)}</td>
		{:else if 'target_type' in item}
			<td>{item.target_type}</td><td>{item.reason.replaceAll('_', ' ')}</td><td>{item.state.replaceAll('_', ' ')}</td><td>{formatDate(item.created_at)}</td>
		{/if}
		</tr>
	{/each}
	</tbody></table></div>
{/if}

{#if data.page.next_cursor}<p><a href="?section={data.section}&cursor={encodeURIComponent(data.page.next_cursor)}">Older activity</a></p>{/if}
