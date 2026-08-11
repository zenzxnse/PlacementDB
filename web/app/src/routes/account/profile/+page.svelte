<script lang="ts">
	import type { ActionData, PageData } from './$types';
	import Avatar from '$lib/components/Avatar.svelte';

	let { data, form }: { data: PageData; form: ActionData } = $props();

	/* Prefer the URL the action just returned, so the change shows immediately. */
	const currentAvatar = $derived(form?.avatarUrl ?? data.defaultAvatar);
	const avatarMessage = $derived(form?.scope === 'avatar' ? form.message : undefined);
	const avatarSaved = $derived(form?.scope === 'avatar' && Boolean(form.status));
</script>

<svelte:head>
	<title>Your profile: PlacementDB</title>
</svelte:head>

<h1>Your profile</h1>

<div class="panel">
	<div class="panel-head">Avatar</div>
	<div class="panel-body">
		{#if avatarMessage}
			<!--
				Error summary: an explicit heading inside makes the region
				navigable as the next heading after the page H1, matching the
				login pattern. role="alert" announces the contents once.
			-->
			<div class="error-summary" role="alert" tabindex="-1" id="avatar-error">
				<h2>Avatar could not be updated</h2>
				<p>{avatarMessage}</p>
				{#if form?.requestId}
					<p class="meta">Reference: <code>{form.requestId}</code></p>
				{/if}
			</div>
		{:else if avatarSaved}
			<p class="notice" role="status">
				{form?.status === 'removed' ? 'Your avatar was removed.' : 'Your avatar was updated.'}
			</p>
		{/if}

		<div class="profile-head">
			<Avatar src={currentAvatar} name={data.me?.display_name ?? ''} size={96} />
			<div>
				<!--
					enctype is required: without it the browser sends
					application/x-www-form-urlencoded and the file arrives as a
					filename string rather than bytes.
				-->
				<form
					method="post"
					action="?/avatar"
					enctype="multipart/form-data"
					class="stack"
				>
					<div class="field">
						<label for="avatar">Choose an image</label>
						<input
							type="file"
							id="avatar"
							name="avatar"
							accept="image/jpeg,image/png,image/webp"
							required
							aria-describedby="avatar-help"
						/>
						<p class="meta" id="avatar-help">
							JPEG, PNG, or WebP, up to 2 MiB. The file name is discarded and the image is
							checked by its contents, not its extension.
						</p>
					</div>
					<div class="button-row">
						<button type="submit" class="primary" disabled={!data.csrfToken}>Upload</button>
					</div>
				</form>

				<form method="post" action="?/removeAvatar">
					<button type="submit" class="link-button" disabled={!data.csrfToken}>
						Remove current avatar
					</button>
				</form>
			</div>
		</div>
	</div>
</div>

<p class="meta">
	<a href="/u/{data.me?.username}">View your public profile</a>
</p>
