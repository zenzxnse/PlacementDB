<script lang="ts">
	import type { ActionData, PageData } from './$types';
	import { formatDate } from '$lib/format';
	import {
		REPORT_REASON_LABELS,
		REPORT_REASON_MAX_LENGTH,
		REPORT_STATE_VALUES,
		type ReportReason,
		type ReportState
	} from '$lib/types';

	/**
	 * Moderator report management.
	 *
	 * The state filter is a plain GET form and pagination is a real link, so
	 * both work without JavaScript. Every decision form carries the report's
	 * current state as expected_state: the API compares and sets, so two
	 * moderators acting on the same report cannot silently overwrite each
	 * other; the loser gets a reload-and-review message, not a mystery.
	 */
	let { data, form }: { data: PageData; form: ActionData } = $props();

	const stateLabels: Record<ReportState, string> = {
		open: 'Open',
		under_review: 'Under review',
		resolved: 'Resolved',
		dismissed: 'Dismissed'
	};

	function reasonLabel(reason: string): string {
		return reason in REPORT_REASON_LABELS
			? REPORT_REASON_LABELS[reason as ReportReason]
			: reason;
	}
</script>

<svelte:head><title>Reports: PlacementDB</title></svelte:head>
<h1>Reports</h1>

{#if form?.message}
	<p class={form.failed ? 'inline-error' : 'notice notice-success'} role={form.failed ? 'alert' : 'status'}>
		{form.message}
	</p>
{/if}

<form method="get" action="/moderation/reports" class="panel panel-body filters" aria-label="Filter reports">
	<div class="field">
		<label for="report-state">State</label>
		<select id="report-state" name="state">
			{#each REPORT_STATE_VALUES as state (state)}
				<option value={state} selected={state === data.state}>{stateLabels[state]}</option>
			{/each}
		</select>
	</div>
	<div class="field">
		<button type="submit">Apply</button>
	</div>
</form>

{#if data.reports.items.length === 0}
	<p class="empty-state">No {stateLabels[data.state].toLowerCase()} reports.</p>
{:else}
	{#each data.reports.items as report (report.public_id)}
		<article class="panel panel-body stack">
			<h2>{reasonLabel(report.reason)} on a {report.target_type}</h2>
			<p class="meta">
				Reported by {report.reporter_label} on {formatDate(report.created_at)}.
				State: {stateLabels[report.state]}.
			</p>
			{#if report.details}
				<blockquote class="report-details">{report.details}</blockquote>
			{/if}
			{#if report.state === 'open' || report.state === 'under_review'}
				<form method="post" class="stack">
					<input type="hidden" name="_csrf" value={data.csrfToken} />
					<input type="hidden" name="public_id" value={report.public_id} />
					<input type="hidden" name="expected_state" value={report.state} />
					<div class="field">
						<label for="reason-{report.public_id}">Reason</label>
						<textarea
							id="reason-{report.public_id}"
							name="reason"
							rows="2"
							maxlength={REPORT_REASON_MAX_LENGTH}
							required
						></textarea>
					</div>
					<div class="button-row">
						<button name="decision" value="resolved" class="primary" disabled={!data.csrfToken}>
							Mark resolved
						</button>
						<button name="decision" value="dismissed" disabled={!data.csrfToken}>Dismiss</button>
					</div>
				</form>
			{/if}
		</article>
	{/each}
	{#if data.reports.next_cursor}
		<p>
			<a href="?state={encodeURIComponent(data.state)}&cursor={encodeURIComponent(data.reports.next_cursor)}">
				Older reports
			</a>
		</p>
	{/if}
{/if}

<style>
	.report-details {
		margin: 0;
		padding: 0.5rem 0.75rem;
		border-left: 3px solid var(--border);
		white-space: pre-wrap;
	}
</style>
