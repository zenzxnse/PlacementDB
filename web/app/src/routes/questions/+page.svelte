<script lang="ts">
	import type { PageData } from './$types';
	import { buildQuestionQuery, hasActiveFilters } from '$lib/query';
	import { difficultySummary, roundLabel } from '$lib/format';
	import type { Sort } from '$lib/types';

	let { data }: { data: PageData } = $props();

	const sorts: Sort[] = ['hot', 'new', 'top'];
	const result = $derived(data.result);
	const filters = $derived(data.filters);
	const activeFilters = $derived(hasActiveFilters(filters));

	const prevQuery = $derived(
		buildQuestionQuery({ sort: data.sort, filters, page: result.page - 1 })
	);
	const nextQuery = $derived(
		buildQuestionQuery({ sort: data.sort, filters, page: result.page + 1 })
	);

	function selected(values: string[], candidate: string): boolean {
		return values.includes(candidate);
	}
</script>

<svelte:head>
	<title>Questions: PlacementDB</title>
</svelte:head>

<h1>Questions</h1>

<form method="get" action="/questions" class="panel panel-body filters" aria-label="Filter questions">
	<div class="field">
		<label for="filter-company">Company</label>
		<select id="filter-company" name="company">
			<option value="">Any company</option>
			{#each data.options.companies as company}
				<option value={company.slug} selected={selected(filters.company, company.slug)}
					>{company.name}</option
				>
			{/each}
		</select>
	</div>
	<div class="field">
		<label for="filter-role">Role</label>
		<select id="filter-role" name="role">
			<option value="">Any role</option>
			{#each data.options.roles as role}
				<option value={role} selected={selected(filters.role, role)}>{role}</option>
			{/each}
		</select>
	</div>
	<div class="field">
		<label for="filter-topic">Topic</label>
		<select id="filter-topic" name="topic">
			<option value="">Any topic</option>
			{#each data.options.topics as topic}
				<option value={topic.slug} selected={selected(filters.topic, topic.slug)}
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
				<option value={String(year)} selected={selected(filters.year, String(year))}
					>{year}</option
				>
			{/each}
		</select>
	</div>
	<div class="field">
		<label for="filter-difficulty">Difficulty</label>
		<select id="filter-difficulty" name="difficulty">
			<option value="">Any difficulty</option>
			{#each [1, 2, 3, 4, 5] as level}
				<option value={String(level)} selected={selected(filters.difficulty, String(level))}
					>{level} of 5</option
				>
			{/each}
		</select>
	</div>
	<input type="hidden" name="sort" value={data.sort} />
	<div class="button-row">
		<button type="submit" class="primary">Apply filters</button>
		{#if activeFilters}
			<a class="button" href="/questions?sort={data.sort}">Clear filters</a>
		{/if}
	</div>
</form>

<nav class="sort-tabs" aria-label="Sort order">
	{#each sorts as sort}
		<a
			href="/questions{buildQuestionQuery({ sort, filters })}"
			aria-current={sort === data.sort ? 'true' : undefined}>{sort}</a
		>
	{/each}
</nav>

{#if result.total === 0}
	<p class="empty-state">
		No published questions match these filters.
		<a href="/questions">Clear the filters</a> to see the full list.
	</p>
{:else}
	<p class="meta">
		{result.total} question{result.total === 1 ? '' : 's'}, page {result.page} of
		{result.total_pages}
	</p>
	<table class="data">
		<caption class="visually-hidden">
			Published questions, sorted by {data.sort}
		</caption>
		<thead>
			<tr>
				<th scope="col">Title</th>
				<th scope="col">Company</th>
				<th scope="col">Round</th>
				<th scope="col">Year</th>
				<th scope="col">Difficulty</th>
			</tr>
		</thead>
		<tbody>
			{#each result.items as question (question.public_id)}
				<tr>
					<td><a href="/questions/{question.slug}">{question.title}</a></td>
					<td>{question.company.name}</td>
					<td>{roundLabel(question.round)}</td>
					<td>{question.source_year}</td>
					<td>{difficultySummary(question.difficulty)}</td>
				</tr>
			{/each}
		</tbody>
	</table>

	<nav class="pagination" aria-label="Pagination">
		{#if result.page > 1}
			<a class="button" href="/questions{prevQuery}">Previous page</a>
		{/if}
		<span class="current" aria-current="page">Page {result.page}</span>
		{#if result.page < result.total_pages}
			<a class="button" href="/questions{nextQuery}">Next page</a>
		{/if}
	</nav>
{/if}
