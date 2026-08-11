<script lang="ts">import type { PageData } from './$types'; let { data }: { data: PageData } = $props();</script>
<svelte:head><title>Moderation audit: PlacementDB</title></svelte:head>
<h1>Moderation audit</h1>
{#if data.audit.items.length === 0}<p class="empty-state">No moderation events.</p>{:else}
<table class="data"><thead><tr><th>Time</th><th>Target</th><th>Change</th><th>Reason</th></tr></thead><tbody>
{#each data.audit.items as item}<tr><td>{item.created_at}</td><td>{item.target_type}</td><td>{item.previous_state} → {item.new_state}</td><td>{item.reason ?? '—'}</td></tr>{/each}
</tbody></table>
{#if data.audit.next_cursor}<p><a href="?cursor={encodeURIComponent(data.audit.next_cursor)}">Older events</a></p>{/if}{/if}
