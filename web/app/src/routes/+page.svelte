<script lang="ts">
	let { data }: { data: import('./$types').PageData } = $props();
</script>

<svelte:head>
	<title>PlacementDB: interview questions from SRM students</title>
	<meta
		name="description"
		content="Placement questions and interview experiences from SRM students, reviewed by moderators before publication."
	/>
</svelte:head>

<section aria-labelledby="intro-heading">
	<h1 id="intro-heading">Placement questions from SRM students</h1>
	<p>
		Browse, search, and share interview questions and placement experiences. Moderators
		review every submission before it appears.
	</p>
	<p class="button-row">
		<a class="button primary" href="/questions">Browse all questions</a>
		<a class="button" href="/experiences">Read experiences</a>
		<a class="button" href="/search">Search the bank</a>
	</p>
</section>

<div class="panel">
	<div class="panel-head">Hot questions</div>
	<div class="panel-body">
		{#if data.hot.length > 0}
			<ul class="list-plain">
				{#each data.hot as question (question.public_id)}
					<li>
						<a href="/questions/{question.slug}">{question.title}</a>
						<span class="meta">
							{question.company.name} &middot; {question.source_year}
						</span>
					</li>
				{/each}
			</ul>
			<p><a href="/questions?sort=hot">See more hot questions</a></p>
		{:else}
			<p class="empty-state">No questions have reached the hot feed yet.</p>
		{/if}
	</div>
</div>

<div class="panel">
	<div class="panel-head">New questions</div>
	<div class="panel-body">
		{#if data.fresh.length > 0}
			<ul class="list-plain">
				{#each data.fresh as question (question.public_id)}
					<li>
						<a href="/questions/{question.slug}">{question.title}</a>
						<span class="meta">
							{question.company.name} &middot; {question.source_year}
						</span>
					</li>
				{/each}
			</ul>
			<p><a href="/questions?sort=new">See more new questions</a></p>
		{:else}
			<p class="empty-state">Nothing published recently.</p>
		{/if}
	</div>
</div>

<div class="panel">
	<div class="panel-head">Recent experiences</div>
	<div class="panel-body">
		{#if data.recent.length > 0}
			<ul class="list-plain">
				{#each data.recent as experience (experience.public_id)}
					<li>
						<a href="/experiences/{experience.slug}">{experience.title}</a>
						<span class="meta">
							{experience.company.name} &middot; {experience.source_year}
						</span>
					</li>
				{/each}
			</ul>
			<p><a href="/experiences">Read all experiences</a></p>
		{:else}
			<p class="empty-state">No experiences published yet.</p>
		{/if}
	</div>
</div>
