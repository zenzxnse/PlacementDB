import { describe, expect, it } from 'vitest';
import { render } from 'svelte/server';
import SideNav from './SideNav.svelte';
import type { Me } from '$lib/types';

/**
 * Capability gating for the primary navigation.
 *
 * The plan's resilience review named this gap: capability gating was verified
 * anonymous only, and authenticated + moderator paths needed mocked /me
 * render tests proving `can_submit` and `can_moderate` gate the markup rather
 * than a role string. These tests render the SideNav component with controlled
 * /me values and assert which links reach the markup.
 *
 * Render is server-side only, which matches the application's `csr = false`
 * mode. The tests assert the produced HTML directly; nothing on the client
 * changes the markup.
 */

function makeMe(overrides: Partial<Me> = {}): Me {
	return {
		public_id: 'u-test',
		username: 'student_001',
		display_name: 'Student One',
		role: 'user',
		status: 'active',
		can_submit: false,
		can_moderate: false,
		unread_moderation_count: 0,
		...overrides
	};
}

function renderNav(me: Me | null, pathname = '/'): string {
	const { body } = render(SideNav, { props: { me, pathname } });
	return body;
}

/*
 * What these tests can and cannot prove, stated plainly so the next reader is
 * not misled.
 *
 * `/submit` now exists, so the personal group is a live capability gate and is
 * tested as one below: flip `can_submit` and the link appears or vanishes.
 * Mutation-checked by changing the gate to `me?.can_moderate`, which fails the
 * can_submit-only and can_moderate-only cases and nothing else.
 *
 * The moderation group is still an empty constant, because none of its routes
 * exist. There is no live gate there to test, and a test claiming to verify
 * one would pass no matter what `can_moderate` said. An earlier version of
 * this file did exactly that: the component read `me?.can_moderate ? [] : []`,
 * and inverting both conditions left every assertion green. What is asserted
 * instead is the invariant that holds: no link to an unbuilt route reaches the
 * markup for ANY capability combination, and no role string is ever emitted.
 */
describe('SideNav route exposure', () => {
	const capabilities = [
		{ name: 'anonymous', me: null },
		{ name: 'can_submit only', me: makeMe({ can_submit: true, can_moderate: false }) },
		{ name: 'can_moderate only', me: makeMe({ can_submit: false, can_moderate: true }) },
		{ name: 'both capabilities', me: makeMe({ can_submit: true, can_moderate: true }) },
		{ name: 'neither capability', me: makeMe() }
	];

	/* Every route the side panel must not link to until it is built. */
	const unbuilt = [
		'/account/saved',
		'/account/inbox'
	];

	it.each(capabilities)('links to no unbuilt route for $name', ({ me }) => {
		const html = renderNav(me);
		/* Browse links are unconditional and must survive. */
		expect(html).toContain('href="/"');
		expect(html).toContain('href="/questions"');
		expect(html).toContain('href="/experiences"');
		expect(html).toContain('href="/topics"');
		expect(html).toContain('href="/companies"');
		for (const href of unbuilt) {
			expect(html).not.toContain(`href="${href}"`);
		}
		if (me) expect(html).toContain('href="/account/activity"');
		else expect(html).not.toContain('href="/account/activity"');
		if (me?.can_moderate) {
			expect(html).toContain('href="/moderation/queue"');
			expect(html).toContain('href="/moderation/reports"');
			expect(html).toContain('href="/moderation/audit"');
			expect(html).toContain('id="sidenav-moderation"');
		} else {
			expect(html).not.toContain('href="/moderation/queue"');
			expect(html).not.toContain('href="/moderation/reports"');
			expect(html).not.toContain('id="sidenav-moderation"');
		}
	});

	it('shows the submit entry only when can_submit is true', () => {
		const allowed = renderNav(makeMe({ can_submit: true }));
		expect(allowed).toContain('href="/submit"');
		expect(allowed).toContain('id="sidenav-you"');

		const refused = renderNav(makeMe({ can_submit: false }));
		expect(refused).not.toContain('href="/submit"');
		expect(refused).toContain('id="sidenav-you"');
	});

	it('hides the submit entry from anonymous visitors', () => {
		expect(renderNav(null)).not.toContain('href="/submit"');
	});

	it('does not let can_moderate stand in for can_submit', () => {
		/*
		 * A moderator who cannot submit gets no submit link. This fails if the
		 * gate is ever widened to "any capability" or swapped for a role check.
		 */
		const html = renderNav(makeMe({ can_submit: false, can_moderate: true }));
		expect(html).not.toContain('href="/submit"');
	});

	it.each(capabilities)('never emits a role string for $name', ({ me }) => {
		/*
		 * Control flow uses the capability booleans. A role string in the markup
		 * means someone branched on `me.role`, which is the pattern the plan
		 * bans because it drifts out of step with what the API actually allows.
		 */
		const html = renderNav(me);
		expect(html).not.toContain('role="user"');
		expect(html).not.toContain('role="moderator"');
		expect(html).not.toContain('role="administrator"');
	});

	it('keeps the moderation explainer link visible to everyone', () => {
		/*
		 * The "How moderation works" link under the static explainer is the
		 * only about-route entry inside the side panel, and it must be
		 * anonymous-readable. It points at the about route, not at the
		 * moderation queue.
		 */
		const html = renderNav(null);
		expect(html).toContain('href="/about/moderation"');
		expect(html).toContain('How moderation works');
	});
});
