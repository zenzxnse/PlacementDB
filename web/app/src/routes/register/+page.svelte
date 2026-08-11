<script lang="ts">
	import type { ActionData, PageData } from './$types';

	let { data, form }: { data: PageData; form: ActionData } = $props();

	const errors = $derived(form?.errors ?? []);

	function errorFor(field: string): string | undefined {
		return errors.find((e) => e.field === field)?.message;
	}

	const usernameError = $derived(errorFor('username'));
	const emailError = $derived(errorFor('email'));
	const displayNameError = $derived(errorFor('display_name'));
	const passwordError = $derived(errorFor('password'));
</script>

<svelte:head>
	<title>Create an account: PlacementDB</title>
</svelte:head>

<h1>Create an account</h1>

<p>
	An account lets you submit questions and experiences, rate difficulty, and comment. Everything you
	submit goes to a moderator before it appears.
</p>

{#if !data.csrfToken}
	<p class="notice" role="status">
		Account creation is temporarily unavailable. Reload the page in a moment and try again.
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

<form method="post" action="/register" class="panel panel-body" novalidate>
	<!--
		Rendered from server data. When issuance failed it is empty, the submit
		button is disabled, and the action refuses the post outright.
	-->
	<input type="hidden" name="_csrf" value={data.csrfToken} />

	<div class="field">
		<label for="username">Username</label>
		<input
			type="text"
			id="username"
			name="username"
			value={form?.username ?? ''}
			autocomplete="username"
			aria-invalid={usernameError ? true : undefined}
			aria-describedby={usernameError ? 'username-error username-help' : 'username-help'}
			required
		/>
		<p class="meta" id="username-help">
			3 to 32 characters. Starts with a letter, then letters, digits, or underscores.
		</p>
		{#if usernameError}
			<p class="inline-error" id="username-error">{usernameError}</p>
		{/if}
	</div>

	<div class="field">
		<label for="display_name">Display name</label>
		<input
			type="text"
			id="display_name"
			name="display_name"
			value={form?.displayName ?? ''}
			autocomplete="name"
			aria-invalid={displayNameError ? true : undefined}
			aria-describedby={displayNameError ? 'display_name-error display_name-help' : 'display_name-help'}
			required
		/>
		<p class="meta" id="display_name-help">The name shown beside anything you post.</p>
		{#if displayNameError}
			<p class="inline-error" id="display_name-error">{displayNameError}</p>
		{/if}
	</div>

	<div class="field">
		<label for="email">Email</label>
		<input
			type="email"
			id="email"
			name="email"
			value={form?.email ?? ''}
			autocomplete="email"
			aria-invalid={emailError ? true : undefined}
			aria-describedby={emailError ? 'email-error email-help' : 'email-help'}
			required
		/>
		<p class="meta" id="email-help">
			Used to identify your account. Email is not verified yet, so password recovery and email
			changes are unavailable until that lands.
		</p>
		{#if emailError}
			<p class="inline-error" id="email-error">{emailError}</p>
		{/if}
	</div>

	<div class="field">
		<label for="password">Password</label>
		<input
			type="password"
			id="password"
			name="password"
			autocomplete="new-password"
			aria-invalid={passwordError ? true : undefined}
			aria-describedby={passwordError ? 'password-error password-help' : 'password-help'}
			required
		/>
		<p class="meta" id="password-help">
			At least 12 characters. There are no character-mix rules; a memorable passphrase is
			stronger than a short scramble.
		</p>
		{#if passwordError}
			<p class="inline-error" id="password-error">{passwordError}</p>
		{/if}
	</div>

	<div class="button-row">
		<!--
			The disabled state and the double-submit guard are progressive
			enhancement only. Correctness lives in the API's CSRF check and rate
			limits, so the form is still safe with JavaScript disabled.
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
			}}>Create account</button
		>
		<a class="button" href="/login">I already have an account</a>
	</div>
</form>
