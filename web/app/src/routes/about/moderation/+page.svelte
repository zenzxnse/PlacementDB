<script lang="ts">
	/* See /about for why this page has no server loader. */
</script>

<svelte:head>
	<title>How moderation works: PlacementDB</title>
	<meta
		name="description"
		content="Every question and experience is reviewed by a moderator before publication. Moderation events are an immutable audit trail."
	/>
</svelte:head>

<h1>How moderation works</h1>

<nav class="breadcrumbs" aria-label="Breadcrumb">
	<ol>
		<li><a href="/about">About</a></li>
		<li aria-current="page">Moderation</li>
	</ol>
</nav>

<div class="panel">
	<div class="panel-head">The submission lifecycle</div>
	<div class="panel-body">
		<p>
			Every question and experience moves through the same lifecycle. A submission starts as
			<em>draft</em>, is submitted for review as <em>pending</em>, and is published by a
			moderator as <em>published</em>. A moderator may move pending content back to
			<em>changes requested</em> or refuse it as <em>rejected</em>. Published content may
			later be moved to <em>hidden</em>, which removes it from public lists, search, and
			rankings without deleting the record.
		</p>
		<p class="meta" role="status">
			Only published, non-hidden content appears in public lists, search results, ranking
			feeds, and sitemaps.
		</p>
	</div>
</div>

<div class="panel">
	<div class="panel-head">The audit trail</div>
	<div class="panel-body">
		<p>
			Every moderation transition records the moderator who acted, the timestamp, the reason
			entered, the previous state, and the new state. The audit log is immutable: it is
			append-only, never edited, and never deleted.
		</p>
		<p>
			Authors cannot approve their own submissions. Authors cannot act on reports against
			their own content. These rules are enforced by the API even when the web interface would
			hide the relevant controls from you.
		</p>
	</div>
</div>

<div class="panel">
	<div class="panel-head">Reporting content</div>
	<div class="panel-body">
		<p>
			Anyone signed in can report a question, experience, or comment for review by a
			moderator. A report does not change the item's state; it enters the moderation queue for
			a moderator to decide. Reports are audited alongside moderation events.
		</p>
	</div>
</div>

<nav class="button-row" aria-label="More about this archive">
	<a class="button" href="/about">About</a>
	<a class="button" href="/about/help">Help</a>
	<a class="button" href="/">Back to the archive</a>
</nav>