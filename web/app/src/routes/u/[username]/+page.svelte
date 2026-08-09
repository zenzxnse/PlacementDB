<script lang="ts">
	import type { PageData } from './$types';
	import Avatar from '$lib/components/Avatar.svelte';
	import { formatDate } from '$lib/format';

	let { data }: { data: PageData } = $props();
	const profile = $derived(data.profile);
</script>

<svelte:head>
	<title>{profile.display_name}: PlacementDB</title>
</svelte:head>

<div class="profile-head">
	<Avatar src={profile.avatar_url} name={profile.display_name} size={96} />
	<div>
		<h1>{profile.display_name}</h1>
		<p class="meta">@{profile.username} &middot; joined {profile.join_month}</p>
		{#if data.isSelf}
			<p><a href="/account/profile">Edit your profile</a></p>
		{/if}
	</div>
</div>

{#if profile.bio}
	<p>{profile.bio}</p>
{/if}

<div class="panel">
	<div class="panel-head">Public contributions</div>
	<div class="panel-body">
		<dl class="detail-list">
			<dt>Questions</dt>
			<dd>{profile.public_question_count}</dd>
			<dt>Experiences</dt>
			<dd>{profile.public_experience_count}</dd>
		</dl>
		<p class="meta">
			Only published content appears here. Drafts, submissions under review, and anything a
			moderator has hidden stay private.
		</p>
	</div>
</div>
