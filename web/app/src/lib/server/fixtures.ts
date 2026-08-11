import type { Experience, Question, Topic } from '$lib/types';

export const FIXTURE_AS_OF = '2026-08-07T12:00:00Z';

const acme = { slug: 'acme-corp', name: 'Acme Corp' };
const globex = { slug: 'globex', name: 'Globex Industries' };
const initech = { slug: 'initech', name: 'Initech Solutions' };
const northwind = { slug: 'northwind-robotics', name: 'Northwind Robotics' };
const bluepeak = { slug: 'bluepeak-systems', name: 'Bluepeak Systems' };

const arrays: Topic = { slug: 'arrays', name: 'arrays' };
const trees: Topic = { slug: 'trees', name: 'trees' };
const graphs: Topic = { slug: 'graphs', name: 'graphs' };
const dp: Topic = { slug: 'dynamic-programming', name: 'dynamic programming' };
const systemDesign: Topic = { slug: 'system-design', name: 'system design' };
const sql: Topic = { slug: 'sql', name: 'sql' };
const ood: Topic = { slug: 'object-oriented-design', name: 'object oriented design' };
const os: Topic = { slug: 'operating-systems', name: 'operating systems' };

const studentOne = { username: 'student_001', display_name: 'Student One' };
const studentTwo = { username: 'student_002', display_name: 'Student Two' };
const studentThree = { username: 'student_003', display_name: 'Student Three' };

export const fixtureQuestions: Question[] = [
	{
		public_id: 'q0000000-0000-4000-8000-000000000001',
		slug: 'acme-two-sum-variant-2025',
		title: 'Find two elements in a sorted array that add up to a target sum',
		prompt:
			'You are given a sorted array of integers and a target value. Find the indices of two distinct elements whose sum equals the target. The interviewer first asked for a brute force solution, then pushed for a linear pass with constant extra memory, and finally asked what changes if the array is not sorted.',
		answer_guidance:
			'Start with the two pointer approach from both ends of the array. Move the left pointer right when the sum is too small and the right pointer left when it is too large. For the unsorted variant, discuss the hash set trade-off and mention that sorting first costs an extra O(n log n).',
		company: acme,
		role: { slug: 'backend-engineer', name: 'Backend Engineer' },
		round: 'technical',
		source_year: 2025,
		topics: [arrays],
		author: studentOne,
		published_at: '2026-07-28T09:00:00Z',
		difficulty: { mean: 2.25, vote_count: 12 }
	},
	{
		public_id: 'q0000000-0000-4000-8000-000000000002',
		slug: 'globex-bst-lowest-common-ancestor-2025',
		title: 'Lowest common ancestor in a binary search tree',
		prompt:
			'Given the root of a binary search tree and two node values, find their lowest common ancestor. The interviewer asked me to explain why the BST property lets you avoid searching both subtrees, and then asked what happens with duplicate values in the tree.',
		answer_guidance:
			'Walk down from the root. If both values are smaller than the current node, move left. If both are larger, move right. Otherwise the current node is the split point and the answer. Duplicates need a defined placement rule before the argument holds.',
		company: globex,
		role: { slug: 'software-development-engineer', name: 'Software Development Engineer' },
		round: 'technical',
		source_year: 2025,
		topics: [trees],
		author: studentTwo,
		published_at: '2026-07-21T14:30:00Z',
		difficulty: { mean: 2.8, vote_count: 10 }
	},
	{
		public_id: 'q0000000-0000-4000-8000-000000000003',
		slug: 'initech-edit-distance-2024',
		title: 'Compute the edit distance between two strings',
		prompt:
			'Given two strings, find the minimum number of insertions, deletions, and substitutions needed to transform one into the other. The interviewer started with the recursive definition, then asked for the table formulation, and finished by asking how much memory the table really needs.',
		answer_guidance:
			'Define the subproblem on prefixes of both strings. Each cell takes the minimum of three neighbours plus a substitution cost. Only the previous row is needed, so memory drops to the shorter string length.',
		company: initech,
		role: { slug: 'software-development-engineer', name: 'Software Development Engineer' },
		round: 'online_assessment',
		source_year: 2024,
		topics: [dp],
		author: studentThree,
		published_at: '2026-06-30T11:00:00Z',
		difficulty: { mean: 3.6, vote_count: 15 }
	},
	{
		public_id: 'q0000000-0000-4000-8000-000000000004',
		slug: 'acme-url-shortener-design-2025',
		title: 'Design a URL shortener service',
		prompt:
			'Design a service that takes a long URL and returns a short link that redirects back to it. The interviewer cared about capacity estimates, how short codes are generated, read versus write traffic, and what happens when the same long URL is submitted twice.',
		answer_guidance:
			'Estimate requests per second first. Compare a base62 counter against hashing the long URL, and talk through collisions. A cache in front of the redirect path covers the heavy read side. Deduplication is a product decision, so ask about it.',
		company: acme,
		role: { slug: 'backend-engineer', name: 'Backend Engineer' },
		round: 'system_design',
		source_year: 2025,
		topics: [systemDesign],
		author: null,
		published_at: '2026-08-02T08:15:00Z',
		difficulty: { mean: 3.2, vote_count: 9 }
	},
	{
		public_id: 'q0000000-0000-4000-8000-000000000005',
		slug: 'northwind-sql-second-highest-salary-2026',
		title: 'Second highest salary without using LIMIT',
		prompt:
			'Write a query that returns the second highest distinct salary from an employee table. The interviewer banned LIMIT and OFFSET, then asked for a version that handles ties, and finally asked how the query plan changes with a million rows.',
		answer_guidance:
			'A correlated subquery counting distinct higher salaries works without LIMIT. DENSE_RANK over the distinct salaries handles ties cleanly. An index on the salary column turns the sort into an ordered scan.',
		company: northwind,
		role: { slug: 'data-engineer', name: 'Data Engineer' },
		round: 'online_assessment',
		source_year: 2026,
		topics: [sql],
		author: studentOne,
		published_at: '2026-08-05T16:45:00Z',
		difficulty: { mean: 2.0, vote_count: 7 }
	},
	{
		public_id: 'q0000000-0000-4000-8000-000000000006',
		slug: 'bluepeak-graph-cycle-detection-2025',
		title: 'Detect a cycle in a directed graph',
		prompt:
			'Given a directed graph as an adjacency list, determine whether it contains a cycle. The interviewer asked for both a depth first search solution and the Kahn topological ordering approach, and wanted me to explain which one fits a build system dependency check better.',
		answer_guidance:
			'Track three colours during depth first search: unvisited, in the current path, and done. A back edge into the current path is a cycle. The Kahn ordering approach detects a cycle when the queue empties with unprocessed nodes left, which suits topological jobs like build ordering.',
		company: bluepeak,
		role: { slug: 'software-development-engineer', name: 'Software Development Engineer' },
		round: 'technical',
		source_year: 2025,
		topics: [graphs],
		author: studentTwo,
		published_at: '2026-07-12T10:20:00Z',
		difficulty: { mean: 3.4, vote_count: 11 }
	},
	{
		public_id: 'q0000000-0000-4000-8000-000000000007',
		slug: 'initech-parking-lot-ood-2024',
		title: 'Design the classes for a parking lot system',
		prompt:
			'Model a multi-floor parking lot with different vehicle sizes, hourly pricing, and multiple entry gates. The interviewer focused on which responsibilities belong to which class, how a ticket moves through its lifecycle, and where concurrency would appear.',
		answer_guidance:
			'Separate the physical layout from the pricing policy and the ticket lifecycle. A strategy object for pricing keeps rate rules out of the core classes. Gate allocation is the natural concurrency point, so name it explicitly.',
		company: initech,
		role: { slug: 'software-development-engineer', name: 'Software Development Engineer' },
		round: 'technical',
		source_year: 2024,
		topics: [ood, systemDesign],
		author: studentThree,
		published_at: '2026-06-18T13:00:00Z',
		difficulty: { mean: 2.6, vote_count: 6 }
	},
	{
		public_id: 'q0000000-0000-4000-8000-000000000008',
		slug: 'globex-process-vs-thread-2026',
		title: 'Processes versus threads, and where context switches hurt',
		prompt:
			'The interviewer asked me to compare processes and threads, then walked through what the kernel saves during a context switch, and finished by asking why a database server might still prefer processes despite the extra cost.',
		answer_guidance:
			'Cover address space isolation first. A context switch saves registers and flushes caches, and switching between processes also switches page tables. Isolation after a crash is the usual reason a database keeps one process per connection.',
		company: globex,
		role: { slug: 'software-development-engineer', name: 'Software Development Engineer' },
		round: 'hr',
		source_year: 2026,
		topics: [os],
		author: null,
		published_at: '2026-08-06T07:40:00Z',
		difficulty: { mean: 3.0, vote_count: 0 }
	}
];

export const fixtureExperiences: Experience[] = [
	{
		public_id: 'e0000000-0000-4000-8000-000000000001',
		slug: 'acme-backend-intern-2026',
		title: 'Acme Corp backend intern interview, three rounds',
		company: acme,
		role: { slug: 'backend-engineer-intern', name: 'Backend Engineer Intern' },
		source_year: 2026,
		outcome: 'offered',
		outcome_visible: true,
		author: studentOne,
		published_at: '2026-08-01T09:30:00Z',
		narrative:
			'The process started with an online assessment on a proctored platform. Two questions: a sliding window problem and a SQL join across three tables. Both felt close to the practice sets shared on campus.\n\nThe technical round was video based. We spent most of the time on one array problem where the interviewer kept adding constraints: first duplicates, then a memory cap, then a follow-up about streaming input. They cared more about how I tested edge cases than about speed.\n\nThe HR round was short. Standard questions about why the company and where I see myself, plus one detailed question about my mini project. The offer came four days later with a clear joining window.',
		rounds: [
			{ ordinal: 1, round: 'online_assessment', notes: 'Online assessment: Sliding window problem plus a three table SQL join, 90 minutes.' },
			{ ordinal: 2, round: 'technical', notes: 'Technical interview: One array problem with escalating constraints, heavy focus on edge cases.' },
			{ ordinal: 3, round: 'hr', notes: 'HR interview: Motivation questions and a walkthrough of a mini project.' }
		]
	},
	{
		public_id: 'e0000000-0000-4000-8000-000000000002',
		slug: 'globex-sde-campus-2025',
		title: 'Globex Industries campus SDE process, rejected after design round',
		company: globex,
		role: { slug: 'software-development-engineer', name: 'Software Development Engineer' },
		source_year: 2025,
		outcome: 'rejected',
		outcome_visible: true,
		author: studentTwo,
		published_at: '2026-07-15T12:00:00Z',
		narrative:
			'The first two rounds were straightforward coding: a graph traversal and a string parsing question. I finished both with time left and felt confident.\n\nThe design round asked for a notification service. I jumped into the database schema before estimating traffic, and the interviewer redirected me twice. In hindsight the structure was standard: estimate, API, storage, delivery, failure cases. I had only practised the algorithm side.\n\nRejection came the next morning with a polite note. The takeaway is simple: practise saying numbers out loud, even rough ones, before touching a schema.',
		rounds: [
			{ ordinal: 1, round: 'technical', notes: 'Coding round one: Graph traversal with a memory constraint.' },
			{ ordinal: 2, round: 'technical', notes: 'Coding round two: String parsing with malformed input cases.' },
			{ ordinal: 3, round: 'system_design', notes: 'System design round: Notification service design, weak capacity estimates cost me the round.' }
		]
	},
	{
		public_id: 'e0000000-0000-4000-8000-000000000003',
		slug: 'initech-data-engineer-2026',
		title: 'Initech Solutions data engineer interview, withdrew mid-process',
		company: initech,
		role: { slug: 'data-engineer', name: 'Data Engineer' },
		source_year: 2026,
		outcome: 'withdrew',
		outcome_visible: true,
		author: null,
		published_at: '2026-07-25T17:10:00Z',
		narrative:
			'The first round was a take-home SQL exercise: clean a messy event log and aggregate weekly retention. It was realistic and clearly scoped.\n\nDuring the second round the interviewer described the day to day differently from the posting: mostly on-site support tickets with some pipeline work. That was not what I was looking for, so I withdrew before the final round.\n\nI am sharing this because the interview quality itself was good. Ask early what the weekly work actually looks like.',
		rounds: [
			{ ordinal: 1, round: 'online_assessment', notes: 'Take-home SQL exercise: Clean a messy event log and compute weekly retention.' },
			{ ordinal: 2, round: 'technical', notes: 'Technical interview: Pipeline walkthrough; role expectations surfaced here.' }
		]
	},
	{
		public_id: 'e0000000-0000-4000-8000-000000000004',
		slug: 'northwind-robotics-sde-2026',
		title: 'Northwind Robotics SDE interview, offer after a tight final round',
		company: northwind,
		role: { slug: 'software-development-engineer', name: 'Software Development Engineer' },
		source_year: 2026,
		outcome: 'offered',
		outcome_visible: true,
		author: studentThree,
		published_at: '2026-08-04T10:00:00Z',
		narrative:
			'Round one was a standard pair of algorithm questions with a shared editor. Round two was operating systems: scheduling, deadlocks, and one question about real time constraints that tied into their robotics work.\n\nRound three was different from anything I had practised: a broken build and a failing test suite on a shared machine, and I had to find two seeded defects. It tested reading unfamiliar code calmly more than any algorithm did.\n\nThe final round was values and motivation, including how I handle disagreeing with a teammate. Offer in three days.',
		rounds: [
			{ ordinal: 1, round: 'technical', notes: 'Algorithms round: Two problems in a shared editor, one dynamic programming.' },
			{ ordinal: 2, round: 'technical', notes: 'Operating systems round: Scheduling, deadlocks, and a real time constraint question.' },
			{ ordinal: 3, round: 'technical', notes: 'Debugging round: Find two seeded defects in a failing test suite.' },
			{ ordinal: 4, round: 'hr', notes: 'Values round: Teamwork and disagreement handling.' }
		]
	},
	{
		public_id: 'e0000000-0000-4000-8000-000000000005',
		slug: 'bluepeak-systems-intern-2025',
		title: 'Bluepeak Systems intern interview, single long technical round',
		company: bluepeak,
		role: { slug: 'software-development-engineer-intern', name: 'Software Development Engineer Intern' },
		source_year: 2025,
		outcome: 'unknown',
		outcome_visible: true,
		author: studentOne,
		published_at: '2026-06-22T15:30:00Z',
		narrative:
			'Bluepeak ran a single long round instead of separate stages. The first half was a coding problem about merging intervals with a twist: some intervals arrive out of order and you cannot store all of them.\n\nThe second half was rapid fire theory: hash table worst cases, TCP handshakes, and why virtual memory matters for a compiler.\n\nNo follow-up email ever arrived despite two polite pings. If you interview there, ask for a decision timeline up front.',
		rounds: [
			{ ordinal: 1, round: 'technical', notes: 'Combined technical round: Interval merging with streaming constraints, then rapid fire theory.' }
		]
	}
];
