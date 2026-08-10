# PlacementDB

PlacementDB is a community question bank for interview questions and candidate experiences. Browse by company, role, year, and difficulty, or share what you encountered.

![PlacementDB](assets/thumbnail.png)

![C++23](https://img.shields.io/badge/C++-23-00599C?style=flat-square&logo=cplusplus)
![Drogon](https://img.shields.io/badge/Drogon-Backend-2E8B57?style=flat-square)
![PostgreSQL](https://img.shields.io/badge/PostgreSQL-17-4169E1?style=flat-square&logo=postgresql&logoColor=white)
![SvelteKit](https://img.shields.io/badge/SvelteKit-Frontend-FF3E00?style=flat-square&logo=svelte&logoColor=white)
![Version](https://img.shields.io/badge/version-0.1.0-222222?style=flat-square)

## Run locally

```sh
./start-dev.sh
```

Open `http://127.0.0.1:3000`. Press Ctrl-C to stop the whole stack.

For a production-mode preview, use `./start-prod.sh`.

## Contributing

Any kind of bug fixesm UI improvements, tests, and data cleanup are happily welcomed. Do explain what changed, and run the relevant checks before opening a pull request.

```sh
cmake --preset linux-strict
cmake --build --preset linux-strict
ctest --preset linux-strict --output-on-failure

cd web/app
npm ci
npm test
npm run check
```

### Contributing bulk data

Use an `.xlsx` workbook with the tables and columns listed in [`tools/etl/validate_workbook.py`](tools/etl/validate_workbook.py). Keep a source URL or citation for every record and remove private personal information.

Validate it locally without printing its contents:

```sh
python3 -m venv .venv
.venv/bin/pip install -r tools/etl/requirements.txt
.venv/bin/python tools/etl/validate_workbook.py \
  placement-data.xlsx \
  --report validation-report.json
```

Then open an issue with:

- what the dataset covers and where it came from;
- confirmation that it may be redistributed;
- approximate question and experience counts;
- the generated `validation-report.json`.

Do not commit the workbook if it contains personal or restricted data. Arrange a private transfer with a maintainer instead. Accepted datasets are converted to the versioned NDJSON archive, reviewed, and imported as drafts before publication.

If you already have an accepted PlacementDB archive, preview its import plan with:

```sh
python3 tools/import/import_archive.py placement-data.zip \
  --dry-run \
  --report import-report.json
```
