<script lang="ts">
	/* See /about for why this page has no server loader. */
</script>

<svelte:head>
	<title>Help: PlacementDB</title>
	<meta
		name="description"
		content="How to browse, search, filter, and submit on PlacementDB without JavaScript."
	/>
</svelte:head>

<h1>Help</h1>

<nav class="breadcrumbs" aria-label="Breadcrumb">
	<ol>
		<li><a href="/about">About</a></li>
		<li aria-current="page">Help</li>
	</ol>
</nav>

<div class="panel">
	<div class="panel-head">Browse</div>
	<div class="panel-body">
		<p>
			Start at <a href="/">the home page</a> for hot and new questions and recent
			experiences, or open <a href="/questions">Browse all questions</a> for the full list.
			Questions can be sorted by hot, new, or top, and filtered by company, role, topic,
			year, and difficulty. Pagination uses ordinary page links, so every view is shareable.
		</p>
		<p>
			Each question shows the prompt, answer guidance if any, topics as small tags, and the
			community difficulty on a scale of 1 to 5. Each experience shows the rounds the author
			went through and, when the author chose to share it, the outcome.
		</p>
	</div>
</div>

<div class="panel">
	<div class="panel-head">Search and filter</div>
	<div class="panel-body">
		<p>
			The top search box is a plain GET form, so a search URL is shareable and works with
			JavaScript off. The search page lets you combine a free-text query with company, topic,
			and year filters and step through result pages.
		</p>
		<p>
			Search is a derived index, not the source of truth. While indexing is happening or the
			index is temporarily unavailable, search degrades to a notice on the page rather than a
			broken screen. Browsing, ranking, and detail pages keep working; only search is
			affected.
		</p>
		<p class="meta">Search results only return published items.</p>
	</div>
</div>

<div class="panel">
	<div class="panel-head">Submitting and voting</div>
	<div class="panel-body">
		<p>
			Signed-in accounts can submit questions and experiences and vote on a question's
			difficulty. Every submission enters the moderation queue, and a moderator must approve
			it before it becomes public. Authors cannot approve their own submissions.
		</p>
		<p class="meta">
			<a href="/submit">Submission</a> and difficulty rating are built. Reports and the
			moderator queue land as the matching backend routes are published. You can read about the
			workflow in <a href="/about/moderation">How moderation works</a>.
		</p>
	</div>
</div>

<nav class="button-row" aria-label="More about this archive">
	<a class="button" href="/about">About</a>
	<a class="button" href="/about/moderation">How moderation works</a>
	<a class="button" href="/">Back to the archive</a>
</nav>