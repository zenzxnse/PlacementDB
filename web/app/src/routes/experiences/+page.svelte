<script lang="ts">
	import type { PageData } from './$types';
	import { buildExperienceQuery, hasActiveFilters } from '$lib/query';
	import { outcomeLabel, companyName, visibleOutcome } from '$lib/format';

	let { data }: { data: PageData } = $props();

	const result = $derived(data.result);
	const filters = $derived(data.filters);
	const activeFilters = $derived(hasActiveFilters(filters));

	function selected(values: string[], candidate: string): boolean {
		return values.includes(candidate);
	}
</script>

<svelte:head>
	<title>Experiences: PlacementDB</title>
</svelte:head>

<h1>Placement experiences</h1>

<form
	method="get"
	action="/experiences"
	class="panel panel-body filters"
	aria-label="Filter experiences"
>
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
				<option value={role.slug} selected={selected(filters.role, role.slug)}>{role.name}</option>
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
		<label for="filter-outcome">Outcome</label>
		<select id="filter-outcome" name="outcome">
			<option value="">Any outcome</option>
			{#each data.options.outcomes as outcome}
				<option value={outcome} selected={selected(filters.outcome, outcome)}
					>{outcomeLabel(outcome)}</option
				>
			{/each}
		</select>
	</div>
	<div class="button-row">
		<button type="submit" class="primary">Apply filters</button>
		{#if activeFilters}
			<a class="button" href="/experiences">Clear filters</a>
		{/if}
	</div>
</form>

{#if result.total === 0}
	<p class="empty-state">
		No published experiences match these filters.
		<a href="/experiences">Clear the filters</a> to see the full list.
	</p>
{:else}
	<p class="meta">
		{result.total} experience{result.total === 1 ? '' : 's'}, page {result.page} of
		{result.total_pages}
	</p>
	<table class="data">
		<caption class="visually-hidden">Published placement experiences, newest first</caption>
		<thead>
			<tr>
				<th scope="col">Title</th>
				<th scope="col">Company</th>
				<th scope="col">Role</th>
				<th scope="col">Year</th>
				<th scope="col">Outcome</th>
			</tr>
		</thead>
		<tbody>
			{#each result.items as experience (experience.public_id)}
				<tr>
					<td><a href="/experiences/{experience.slug}">{experience.title}</a></td>
					<td>{companyName(experience.company)}</td>
					<td>{experience.role?.name ?? 'Not recorded'}</td>
					<td>{experience.source_year}</td>
					<td>{visibleOutcome(experience) ?? ''}</td>
				</tr>
			{/each}
		</tbody>
	</table>

	<nav class="pagination" aria-label="Pagination">
		{#if result.page > 1}
			<a class="button" href="/experiences{buildExperienceQuery({ filters, page: result.page - 1 })}"
				>Previous page</a
			>
		{/if}
		<span class="current" aria-current="page">Page {result.page}</span>
		{#if result.page < result.total_pages}
			<a class="button" href="/experiences{buildExperienceQuery({ filters, page: result.page + 1 })}"
				>Next page</a
			>
		{/if}
	</nav>
{/if}
