<script lang="ts">
	import type { PageData } from './$types';
	import { buildSearchQuery, hasActiveFilters } from '$lib/query';
	import { difficultySummary } from '$lib/format';

	let { data }: { data: PageData } = $props();

	const activeFilters = $derived(hasActiveFilters(data.filters));

	function selected(values: string[], candidate: string): boolean {
		return values.includes(candidate);
	}
</script>

<svelte:head>
	<title>Search: PlacementDB</title>
</svelte:head>

<h1>Search</h1>

<form method="get" action="/search" class="panel panel-body" aria-label="Search published content">
	<div class="search-box">
		<div class="field">
			<label for="search-q">Search questions and experiences</label>
			<input
				type="search"
				id="search-q"
				name="q"
				value={data.q}
				placeholder="arrays, system design, Acme Corp..."
			/>
		</div>
		<button type="submit" class="primary">Search</button>
	</div>

	<div class="filters">
		<div class="field">
			<label for="filter-company">Company</label>
			<select id="filter-company" name="company">
				<option value="">Any company</option>
				{#each data.options.companies as company}
					<option value={company.slug} selected={selected(data.filters.company, company.slug)}
						>{company.name}</option
					>
				{/each}
			</select>
		</div>
		<div class="field">
			<label for="filter-topic">Topic</label>
			<select id="filter-topic" name="topic">
				<option value="">Any topic</option>
				{#each data.options.topics as topic}
					<option value={topic.slug} selected={selected(data.filters.topic, topic.slug)}
						>{topic.name}</option
					>
				{/each}
			</select>
		</div>
		<div class="field">
			<label for="filter-year">Year</label>
			<select id="filter-year" name="year">
				<option value="">Any year</option>
				{#each data.options.years as year}
					<option value={String(year)} selected={selected(data.filters.year, String(year))}
						>{year}</option
					>
				{/each}
			</select>
		</div>
	</div>
</form>

{#if !data.submitted}
	<p class="notice">Enter a query to search published questions and experiences.</p>
{:else if data.outcome === null}
	<p class="empty-state">Nothing to show.</p>
{:else if data.outcome.status === 'unavailable'}
	<div class="notice notice-warn" role="status">
		<p>
			Search is temporarily unavailable. You can still
			<a href="/questions">browse questions</a> and
			<a href="/experiences">read experiences</a>.
		</p>
	</div>
{:else}
	{@const results = data.outcome.results}
	{#if results.total === 0}
		<p class="empty-state">
			No results for “{data.q}”. Check the spelling, try fewer words, or
			<a href="/questions">browse the question list</a>.
		</p>
	{:else}
		<p class="meta" role="status">
			{results.total} result{results.total === 1 ? '' : 's'} for “{data.q}”, page
			{results.page} of {results.total_pages}
		</p>
		{#if activeFilters}
			<p class="meta">
				Filters are active.
				<a href="/search{buildSearchQuery({ q: data.q })}">Clear filters</a>
			</p>
		{/if}
		<ul class="list-plain">
			{#each results.items as hit (hit.url)}
				<li>
					<a href={hit.url}>{hit.title}</a>
					<p class="meta">
						{hit.kind === 'question' ? 'Question' : 'Experience'} &middot;
						{hit.company.name} &middot; {hit.year}
						{#if hit.difficulty}
							&middot; Difficulty {difficultySummary(hit.difficulty)}
						{/if}
					</p>
					<p>{hit.snippet}</p>
				</li>
			{/each}
		</ul>

		<nav class="pagination" aria-label="Pagination">
			{#if results.page > 1}
				<a
					class="button"
					href="/search{buildSearchQuery({ q: data.q, filters: data.filters, page: results.page - 1 })}"
					>Previous page</a
				>
			{/if}
			<span class="current" aria-current="page">Page {results.page}</span>
			{#if results.page < results.total_pages}
				<a
					class="button"
					href="/search{buildSearchQuery({ q: data.q, filters: data.filters, page: results.page + 1 })}"
					>Next page</a
				>
			{/if}
		</nav>
	{/if}
{/if}
