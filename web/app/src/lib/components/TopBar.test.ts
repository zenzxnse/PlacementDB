import { describe, expect, it } from 'vitest';
import { render } from 'svelte/server';
import TopBar from './TopBar.svelte';
import type { Me } from '$lib/types';

/**
 * TopBar renders identity and personal controls based on auth state.
 *
 * Unlike the SideNav, the TopBar has no per-capability gates: its actions are
 * purely auth-based (identity link + Logout when signed in, Help icon +
 * "Log in" link when anonymous). These render tests verify that gates, that no
 * role string leaks into the markup, and that the search form has a real
 * submit control so it is operable without JavaScript.
 */

function makeMe(overrides: Partial<Me> = {}): Me {
	return {
		public_id: 'u-test',
		username: 'student_001',
		display_name: 'Student One',
		role: 'user',
		status: 'active',
		can_submit: true,
		can_moderate: false,
		unread_moderation_count: 0,
		...overrides
	};
}

function renderBar(me: Me | null, query = ''): string {
	const { body } = render(TopBar, { props: { me, query } });
	return body;
}

describe('TopBar anonymous state', () => {
	it('shows login and help affordances without identity or logout', () => {
		const html = renderBar(null);
		expect(html).toContain('href="/login"');
		expect(html).toContain('href="/about/help"');
		expect(html).toContain('aria-label="Help"');
		expect(html).not.toContain('action="/logout"');
		expect(html).not.toContain('href="/u/');
	});

	it('renders a real submit button so the search form is operable', () => {
		const html = renderBar(null);
		/*
		 * Implicit Enter-key submission works but is not enough: an explicit
		 * submit button lets keyboard users tab to it and submit.
		 */
		expect(html).toContain('type="submit"');
		expect(html.toLowerCase()).toContain('>search<');
	});

	it('does not leak the role string or a moderator-only control', () => {
		const html = renderBar(null);
		expect(html).not.toContain('role="user"');
		expect(html).not.toContain('role="moderator"');
		expect(html).not.toContain('role="administrator"');
		expect(html).not.toContain('href="/moderation/');
		expect(html).not.toContain('href="/account/inbox"');
	});
});

describe('TopBar authenticated state', () => {
	it('shows identity, hides login and the icon help affordance', () => {
		const me = makeMe({ username: 'jordan', display_name: 'Jordan Rao' });
		const html = renderBar(me);
		expect(html).toContain('href="/u/jordan"');
		expect(html).toContain('Jordan Rao');
		expect(html).toContain('action="/logout"');
		expect(html).toContain('Log out');
		/*
		 * The /about/help URL also appears in the always-visible topbar-links
		 * nav, so the meaningful anonymous-only check is the help *icon* link
		 * rendered with aria-label="Help" inside topbar-actions.
		 */
		expect(html).not.toContain('aria-label="Help"');
		expect(html).not.toContain('class="topbar-login"');
		expect(html).not.toContain(' href="/login"');
	});

	it('does not leak a role string for authenticated users', () => {
		const html = renderBar(makeMe({ role: 'moderator' }));
		/* The user is moderator, but the role never appears in the markup. */
		expect(html).not.toContain('role="moderator"');
	});

	it('keeps the search form usable for authenticated users', () => {
		const html = renderBar(makeMe());
		expect(html).toContain('action="/search"');
		expect(html).toContain('type="search"');
		expect(html).toContain('name="q"');
		expect(html).toContain('type="submit"');
	});

	it('reflects the current search query in the input value', () => {
		const me = makeMe();
		const html = renderBar(me, 'arrays');
		expect(html).toContain('value="arrays"');
	});
});