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

{#if !data.csrfToken}
	<p class="notice" role="status">
		Sign-in is temporarily unavailable. Reload the page in a moment and try again.
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
	<!--
		The CSRF token is rendered from server data. When issuance failed it is
		empty, the submit button is disabled, and the action refuses the post,
		so a submission cannot be attempted that the API is certain to reject.
	-->
	<input type="hidden" name="_csrf" value={data.csrfToken} />

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
		<!--
			The submit guard is progressive enhancement only. Correctness lives
			in the API's CSRF check and the single-use login token, so the form
			is still safe with JavaScript disabled.
		-->
		<button
			type="submit"
			class="primary"
			disabled={!data.csrfToken}
			onclick={(event) => {
				const button = event.currentTarget as HTMLButtonElement;
				if (button.dataset.submitted === 'true') {
					event.preventDefault();
					return;
				}
				button.dataset.submitted = 'true';
			}}>Log in</button
		>
	</div>
</form>

<p class="meta">
	No account yet? <a href="/register">Create one</a>. Password reset and account recovery are
	still deferred until email delivery has an owner, so choose a password you can remember.
</p>
