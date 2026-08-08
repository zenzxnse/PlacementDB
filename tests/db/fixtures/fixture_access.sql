/*
 * Disposable fixture role credentials.
 * Supply app_password and migration_password as psql variables at runtime.
 * Never store either value in the repository or psql history.
 */

\if :{?app_password}
\else
  \echo 'app_password is required'
  \quit 3
\endif

\if :{?migration_password}
\else
  \echo 'migration_password is required'
  \quit 3
\endif

ALTER ROLE placedb_app PASSWORD :'app_password';
ALTER ROLE placedb_migrate PASSWORD :'migration_password';
