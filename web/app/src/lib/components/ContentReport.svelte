<script lang="ts">
	import { REPORT_REASON_VALUES } from '$lib/types';
	let { csrfToken, outcome }: { csrfToken: string; outcome: { kind: string; message: string; loginHref?: string } | undefined } = $props();
</script>

<section class="panel" aria-labelledby="report-content-heading">
	<div class="panel-head" id="report-content-heading">Report this item</div>
	<div class="panel-body">
		{#if outcome}
			<p class={outcome.kind === 'reported' ? 'notice' : 'error'} role={outcome.kind === 'reported' ? 'status' : 'alert'}>{outcome.message}</p>
			{#if outcome.loginHref}<p><a href={outcome.loginHref}>Log in</a></p>{/if}
		{/if}
		<details>
			<summary>Send a report to moderators</summary>
			<form method="post" action="?/reportContent" class="stack">
				<input type="hidden" name="_csrf" value={csrfToken} />
				<label for="content-report-reason">Reason</label>
				<select id="content-report-reason" name="reason" required>
					<option value="">Choose a reason</option>
					{#each REPORT_REASON_VALUES as reason}<option value={reason}>{reason.replace('_', ' ')}</option>{/each}
				</select>
				<label for="content-report-details">Details (optional)</label>
				<textarea id="content-report-details" name="details" maxlength="1000" rows="3"></textarea>
				<button type="submit" disabled={!csrfToken}>Submit report</button>
			</form>
		</details>
	</div>
</section>
