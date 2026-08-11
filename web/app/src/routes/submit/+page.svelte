<script lang="ts">
	import type { PageData } from './$types';

	let { data }: { data: PageData } = $props();
</script>

<svelte:head>
	<title>Submit: PlacementDB</title>
</svelte:head>

<h1>Submit</h1>

{#if !data.me.can_submit}
	<p class="notice" role="status">
		This account cannot submit content right now. If you think that is wrong, contact a
		moderator through <a href="/about/help">Help</a>.
	</p>
{:else}
	<p>
		Both kinds of submission go to a moderator before they appear. Nothing you send is public
		until someone approves it.
	</p>

	<ul class="list-plain">
		<li>
			<a href="/submit/question">Submit a question</a>
			<p class="meta">One interview question, the round it came from, and optional guidance.</p>
		</li>
		<li>
			<a href="/submit/experience">Share a placement experience</a>
			<p class="meta">
				The whole process for one company: the rounds you went through, and the outcome if you
				want it shown.
			</p>
		</li>
	</ul>
{/if}
