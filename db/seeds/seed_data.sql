/*
 * Synthetic seed data for development.
 * Status: prepared, not yet verified as working fixture data.
 * This file has not been applied against an empty-to-latest schema.
 * It becomes working fixture data only after the authorized disposable
 * replay succeeds.
 * All students are synthetic. No real identities or copied interview content.
 * Idempotent: fixed public IDs and conflict handling prevent duplicates.
 */

INSERT INTO companies (
    public_id, slug, canonical_name, created_at, updated_at
) VALUES
    ('a0000000-0000-4000-8000-000000000001', 'acme-corp', 'Acme Corp',
     TIMESTAMPTZ '2026-07-01 00:00:00+00', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('a0000000-0000-4000-8000-000000000002', 'globex', 'Globex Industries',
     TIMESTAMPTZ '2026-07-01 00:00:00+00', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('a0000000-0000-4000-8000-000000000003', 'initech', 'Initech Solutions',
     TIMESTAMPTZ '2026-07-01 00:00:00+00', TIMESTAMPTZ '2026-07-01 00:00:00+00')
ON CONFLICT (public_id) DO NOTHING;

INSERT INTO topics (name, slug, created_at) VALUES
    ('arrays', 'arrays', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('trees', 'trees', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('graphs', 'graphs', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('dynamic_programming', 'dynamic-programming', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('system_design', 'system-design', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('sql', 'sql', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('object_oriented_design', 'object-oriented-design', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('operating_systems', 'operating-systems', TIMESTAMPTZ '2026-07-01 00:00:00+00')
ON CONFLICT (name) DO NOTHING;

INSERT INTO job_roles (public_id, slug, name, created_at) VALUES
    ('a1000000-0000-4000-8000-000000000001', 'software-engineer',
     'Software Engineer', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('a1000000-0000-4000-8000-000000000002', 'backend-developer',
     'Backend Developer', TIMESTAMPTZ '2026-07-01 00:00:00+00'),
    ('a1000000-0000-4000-8000-000000000003',
     'software-development-engineer', 'Software Development Engineer',
     TIMESTAMPTZ '2026-07-01 00:00:00+00')
ON CONFLICT (public_id) DO NOTHING;

INSERT INTO users (
    public_id, username, email, password_hash, display_name, role_id, status,
    password_changed_at, created_at, updated_at
)
SELECT 'b0000000-0000-4000-8000-000000000001', 'student_001',
       'student_001@example.test',
       '$argon2id$v=19$m=65536,t=3,p=4$c29tZXNhbHR0ZXh0$fakehashvalue1',
       'Student One',
       r.id, 'active',
       TIMESTAMPTZ '2026-07-01 00:00:00+00',
       TIMESTAMPTZ '2026-07-01 00:00:00+00',
       TIMESTAMPTZ '2026-07-01 00:00:00+00'
FROM roles r WHERE r.name = 'user'
ON CONFLICT (public_id) DO NOTHING;

INSERT INTO users (
    public_id, username, email, password_hash, display_name, role_id, status,
    password_changed_at, created_at, updated_at
)
SELECT 'b0000000-0000-4000-8000-000000000002', 'student_002',
       'student_002@example.test',
       '$argon2id$v=19$m=65536,t=3,p=4$c29tZXNhbHR0ZXh0$fakehashvalue2',
       'Student Two',
       r.id, 'active',
       TIMESTAMPTZ '2026-07-01 00:00:00+00',
       TIMESTAMPTZ '2026-07-01 00:00:00+00',
       TIMESTAMPTZ '2026-07-01 00:00:00+00'
FROM roles r WHERE r.name = 'user'
ON CONFLICT (public_id) DO NOTHING;

INSERT INTO users (
    public_id, username, email, password_hash, display_name, role_id, status,
    password_changed_at, created_at, updated_at
)
SELECT 'b0000000-0000-4000-8000-000000000003', 'moderator_001',
       'moderator_001@example.test',
       '$argon2id$v=19$m=65536,t=3,p=4$c29tZXNhbHR0ZXh0$fakehashvalue3',
       'Moderator One',
       r.id, 'active',
       TIMESTAMPTZ '2026-07-01 00:00:00+00',
       TIMESTAMPTZ '2026-07-01 00:00:00+00',
       TIMESTAMPTZ '2026-07-01 00:00:00+00'
FROM roles r WHERE r.name = 'moderator'
ON CONFLICT (public_id) DO NOTHING;

INSERT INTO questions (public_id, slug, title, author_id, company_id, role_title, job_role_id,
                       prompt, round, source_year, state, published_at,
                       created_at, updated_at)
SELECT 'c0000000-0000-4000-8000-000000000001',
       'how-does-hash-map-handle-collisions',
       'How a hash map handles collisions',
       u.id, c.id, 'Software Engineer', jr.id,
       'Explain how a hash map handles collisions. Describe at least two strategies and their time complexity trade-offs.',
       'technical', 2024, 'published',
       TIMESTAMPTZ '2026-07-29 08:00:00+00',
       TIMESTAMPTZ '2026-07-28 08:00:00+00',
       TIMESTAMPTZ '2026-07-29 08:00:00+00'
FROM users u, companies c, job_roles jr
WHERE u.username = 'student_001' AND c.slug = 'acme-corp'
  AND jr.slug = 'software-engineer'
ON CONFLICT (public_id) DO NOTHING;

INSERT INTO questions (public_id, slug, title, author_id, company_id, role_title, job_role_id,
                       prompt, round, source_year, state, published_at,
                       created_at, updated_at)
SELECT 'c0000000-0000-4000-8000-000000000002',
       'design-a-url-shortener',
       'Design a URL shortener',
       u.id, c.id, 'Backend Developer', jr.id,
       'Design a URL shortening service that supports 100 million new URLs per month. Discuss storage, read-write ratio, and cache strategy.',
       'system_design', 2024, 'published',
       TIMESTAMPTZ '2026-07-25 10:30:00+00',
       TIMESTAMPTZ '2026-07-24 10:30:00+00',
       TIMESTAMPTZ '2026-07-25 10:30:00+00'
FROM users u, companies c, job_roles jr
WHERE u.username = 'student_001' AND c.slug = 'globex'
  AND jr.slug = 'backend-developer'
ON CONFLICT (public_id) DO NOTHING;

INSERT INTO questions (public_id, slug, title, author_id, company_id, role_title, job_role_id,
                       prompt, round, source_year, state, published_at,
                       created_at, updated_at)
SELECT 'c0000000-0000-4000-8000-000000000003',
       'binary-search-tree-operations',
       'Binary search tree operations',
       u.id, c.id, 'Software Engineer', jr.id,
       'Implement insert, search, and delete for a binary search tree. What are the worst-case time complexities and how can they be improved?',
       'coding', 2025, 'published',
       TIMESTAMPTZ '2026-07-30 14:15:00+00',
       TIMESTAMPTZ '2026-07-30 13:00:00+00',
       TIMESTAMPTZ '2026-07-30 14:15:00+00'
FROM users u, companies c, job_roles jr
WHERE u.username = 'student_002' AND c.slug = 'initech'
  AND jr.slug = 'software-engineer'
ON CONFLICT (public_id) DO NOTHING;

INSERT INTO question_topics (question_id, topic_id)
SELECT q.id, t.id
FROM questions q, topics t
WHERE q.public_id = 'c0000000-0000-4000-8000-000000000001'
  AND t.slug = 'arrays'
ON CONFLICT DO NOTHING;

INSERT INTO question_topics (question_id, topic_id)
SELECT q.id, t.id
FROM questions q, topics t
WHERE q.public_id = 'c0000000-0000-4000-8000-000000000002'
  AND t.slug = 'system-design'
ON CONFLICT DO NOTHING;

INSERT INTO question_topics (question_id, topic_id)
SELECT q.id, t.id
FROM questions q, topics t
WHERE q.public_id = 'c0000000-0000-4000-8000-000000000003'
  AND t.slug = 'trees'
ON CONFLICT DO NOTHING;

INSERT INTO difficulty_votes (question_id, user_id, value, created_at, updated_at)
SELECT q.id, u.id, 3,
       TIMESTAMPTZ '2026-07-30 09:00:00+00',
       TIMESTAMPTZ '2026-07-30 09:00:00+00'
FROM questions q, users u
WHERE q.public_id = 'c0000000-0000-4000-8000-000000000001'
  AND u.username = 'student_002'
ON CONFLICT DO NOTHING;

INSERT INTO difficulty_votes (question_id, user_id, value, created_at, updated_at)
SELECT q.id, u.id, 4,
       TIMESTAMPTZ '2026-07-27 12:00:00+00',
       TIMESTAMPTZ '2026-07-27 12:00:00+00'
FROM questions q, users u
WHERE q.public_id = 'c0000000-0000-4000-8000-000000000002'
  AND u.username = 'student_001'
ON CONFLICT DO NOTHING;

INSERT INTO difficulty_votes (question_id, user_id, value, created_at, updated_at)
SELECT q.id, u.id, 2,
       TIMESTAMPTZ '2026-07-31 07:45:00+00',
       TIMESTAMPTZ '2026-07-31 07:45:00+00'
FROM questions q, users u
WHERE q.public_id = 'c0000000-0000-4000-8000-000000000003'
  AND u.username = 'student_001'
ON CONFLICT DO NOTHING;

INSERT INTO experiences (public_id, slug, title, author_id, company_id, role_title, job_role_id,
                         narrative, outcome, outcome_visible, anonymous,
                         source_year, state, published_at, created_at, updated_at)
SELECT 'd0000000-0000-4000-8000-000000000001',
       'acme-corp-sde-interview-2024',
       'Acme Corp SDE interview, 2024',
       u.id, c.id, 'Software Development Engineer', jr.id,
       'I interviewed for an SDE role. The process had three rounds: an online coding test, a technical interview on data structures, and an HR round. The coding test focused on arrays and trees. The technical round was conversational and the interviewer gave hints.',
       'offered', true, false, 2024,
       'published', TIMESTAMPTZ '2026-07-28 11:00:00+00',
       TIMESTAMPTZ '2026-07-27 11:00:00+00',
       TIMESTAMPTZ '2026-07-28 11:00:00+00'
FROM users u, companies c, job_roles jr
WHERE u.username = 'student_001' AND c.slug = 'acme-corp'
  AND jr.slug = 'software-development-engineer'
ON CONFLICT (public_id) DO NOTHING;

INSERT INTO experiences (public_id, slug, title, author_id, company_id, role_title, job_role_id,
                         narrative, outcome, outcome_visible, anonymous,
                         source_year, state, published_at, created_at, updated_at)
SELECT 'd0000000-0000-4000-8000-000000000002',
       'globex-backend-interview-2025',
       'Globex backend interview, 2025',
       u.id, c.id, 'Backend Developer', jr.id,
       'The synthetic process had a short screening call, one SQL exercise, and a system design discussion. The interviewer explained the constraints clearly and allowed time for questions.',
       'rejected', false, true, 2025,
       'published', TIMESTAMPTZ '2026-07-26 16:00:00+00',
       TIMESTAMPTZ '2026-07-26 09:00:00+00',
       TIMESTAMPTZ '2026-07-26 16:00:00+00'
FROM users u, companies c, job_roles jr
WHERE u.username = 'student_002' AND c.slug = 'globex'
  AND jr.slug = 'backend-developer'
ON CONFLICT (public_id) DO NOTHING;

INSERT INTO experience_rounds (experience_id, ordinal, round, notes)
SELECT e.id, 1, 'coding', 'Two problems on arrays and hash maps, 45 minutes.'
FROM experiences e WHERE e.public_id = 'd0000000-0000-4000-8000-000000000001'
ON CONFLICT DO NOTHING;

INSERT INTO experience_rounds (experience_id, ordinal, round, notes)
SELECT e.id, 1, 'technical', 'A synthetic SQL task using joins and grouping.'
FROM experiences e WHERE e.public_id = 'd0000000-0000-4000-8000-000000000002'
ON CONFLICT DO NOTHING;

INSERT INTO experience_rounds (experience_id, ordinal, round, notes)
SELECT e.id, 2, 'system_design', 'Discussed a small job queue and retry behavior.'
FROM experiences e WHERE e.public_id = 'd0000000-0000-4000-8000-000000000002'
ON CONFLICT DO NOTHING;

INSERT INTO profiles (user_id, batch, branch, bio, created_at, updated_at)
SELECT u.id, '2027', 'Computer Science',
       'Synthetic profile used for local page development.',
       TIMESTAMPTZ '2026-07-20 00:00:00+00',
       TIMESTAMPTZ '2026-07-20 00:00:00+00'
FROM users u
WHERE u.username = 'student_001'
ON CONFLICT (user_id) DO NOTHING;

INSERT INTO experience_rounds (experience_id, ordinal, round, notes)
SELECT e.id, 2, 'technical', 'Discussed tree traversals and system design basics.'
FROM experiences e WHERE e.public_id = 'd0000000-0000-4000-8000-000000000001'
ON CONFLICT DO NOTHING;
