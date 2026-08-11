<script lang="ts">
	/*
	 * Static page. No server loader: the parent layout already computes `me`
	 * through getMe(event), which degrades to anonymous when /me is
	 * unavailable, so this page renders identically whether the API is up or
	 * down. Nothing here calls the backend or invents a payload.
	 */
</script>

<svelte:head>
	<title>About: PlacementDB</title>
	<meta
		name="description"
		content="PlacementDB is a community question bank for SRM placements. Every question and experience is reviewed by moderators before publication."
	/>
</svelte:head>

<h1>About PlacementDB</h1>

<div class="panel">
	<div class="panel-head">What this archive is</div>
	<div class="panel-body">
		<p>
			PlacementDB is a community question bank for SRM placement preparation. Students
			share interview questions and placement experiences, and moderators review every
			submission before it appears. No record reaches the public list, search, or rankings
			until a moderator has approved it.
		</p>
		<p>
			The archive is built to be useful without JavaScript. Every page is server-rendered,
			every control is an ordinary link or a real HTML form, and every result has a URL you
			can share or bookmark.
		</p>
		<p class="meta">No data is fetched from a third-party host while you browse.</p>
	</div>
</div>

<div class="panel">
	<div class="panel-head">What it contains</div>
	<div class="panel-body">
		<p>Published items in this archive are one of two kinds:</p>
		<ul class="list-plain">
			<li>
				<strong>Questions</strong> carry the prompt, optional answer guidance, the company
				and round if known, the source year, the topics, and the community difficulty vote
				on a 1-to-5 scale.
			</li>
			<li>
				<strong>Experiences</strong> are an anonymous narrative of one student's placement
				round, with rounds listed in order. The author chooses whether the outcome is
				visible. A withheld outcome is never shown, not even as "Unknown".
			</li>
		</ul>
		<p class="meta">
			Imported records stay anonymous until an author claims them. Practice items, recruiter
			counts, and private placement statistics never appear publicly.
		</p>
	</div>
</div>

<nav class="button-row" aria-label="More about this archive">
	<a class="button" href="/about/help">How to use this site</a>
	<a class="button" href="/about/moderation">How moderation works</a>
	<a class="button" href="/">Back to the archive</a>
</nav>