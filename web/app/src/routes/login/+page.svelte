<script lang="ts">
	import type { ActionData, PageData } from './$types';

	let { data, form }: { data: PageData; form: ActionData } = $props();

	const errors = $derived(form?.errors ?? []);
	const identityValue = $derived(form?.identity ?? '');

	function errorFor(field: string): string | undefined {
		return errors.find((e) => e.field === field)?.message;
	}

	const identityError = $derived(errorFor('identity'));
	const passwordError = $derived(errorFor('password'));
</script>

<svelte:head>
	<title>Log in: PlacementDB</title>
</svelte:head>

<h1>Log in</h1>

{#if data.apiConnected === false}
	<p class="notice">
		Login is not connected to the API yet. This page shows the flow that will exist once
		the authentication milestone lands.
	</p>
{/if}

{#if errors.length > 0}
	<div class="error-summary" role="alert" tabindex="-1" id="error-summary">
		<h2>There is a problem</h2>
		<ul>
			{#each errors as err}
				<li>
					{#if err.field === 'form'}
						{err.message}
					{:else}
						<a href="#{err.field}">{err.message}</a>
					{/if}
				</li>
			{/each}
		</ul>
	</div>
{/if}

<form method="post" action="/login" class="panel panel-body" novalidate>
	<div class="field">
		<label for="identity">Username or email</label>
		<input
			type="text"
			id="identity"
			name="identity"
			value={identityValue}
			autocomplete="username"
			aria-invalid={identityError ? true : undefined}
			aria-describedby={identityError ? 'identity-error' : undefined}
			required
		/>
		{#if identityError}
			<p class="inline-error" id="identity-error">{identityError}</p>
		{/if}
	</div>

	<div class="field">
		<label for="password">Password</label>
		<input
			type="password"
			id="password"
			name="password"
			autocomplete="current-password"
			aria-invalid={passwordError ? true : undefined}
			aria-describedby={passwordError ? 'password-error' : undefined}
			required
		/>
		{#if passwordError}
			<p class="inline-error" id="password-error">{passwordError}</p>
		{/if}
	</div>

	<div class="button-row">
		<button type="submit" class="primary">Log in</button>
	</div>
</form>

<p class="meta">
	Registration, password reset, and account recovery are deferred until email delivery has an
	owner. Accounts are provisioned for the prototype.
</p>
