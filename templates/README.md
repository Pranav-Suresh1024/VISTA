# VISTA starter template catalogue

These editable `.vista` files demonstrate domain-specific forms using the same VISTA compiler. Compile a template from the repository root with the command shown below. Each `--emit all` command writes the seven-file compiler evidence bundle, including a standalone `form.html`.

The generated pages validate fields in the browser only. They do not submit applications, reserve places, upload documents to a server, or store personal information.

## Scholarship

Purpose: a sample scholarship application covering applicant contact, institution and course, study year, GPA, household income, category, statement, supporting documents, and a declaration. The category certificate appears and becomes required only for the reserved category.

Source: `templates/scholarship_application.vista`

```bash
./build/vista templates/scholarship_application.vista --emit all --out-dir out/phase3-templates/scholarship
```

## College admission

Purpose: a sample application covering personal/contact details, previous education and score, program selection, transcript, applicant statement, and declaration. Choosing `other` for the program reveals a required program-name field.

Source: `templates/college_admission.vista`

```bash
./build/vista templates/college_admission.vista --emit all --out-dir out/phase3-templates/college-admission
```

## Event registration

Purpose: a sample registration covering attendee/contact details, event, attendance mode, attendee count, optional dietary and accessibility details, and confirmation. Choosing `workshop` reveals a required workshop track; dietary requirements appear only for in-person attendance.

Source: `templates/event_registration.vista`

```bash
./build/vista templates/event_registration.vista --emit all --out-dir out/phase3-templates/event-registration
```

## List the sources

From the repository root, run:

```bash
./build/vista --list-templates
```

The CLI prints each editable source path and a matching compile command. Copy a source file and edit it, or write a new `.vista` file; these templates are examples, not a closed catalogue.
