# VISTA starter templates

Choose a `.vista` file, compile it, then open the generated `form.html`. These are demonstration templates: they do not submit information, reserve tickets, connect to a railway service, or store data.

| Template | Source | What it demonstrates |
|---|---|---|
| Scholarship application | `scholarship_application.vista` | Applicant and study sections, income validation, and a certificate shown only for the reserved category |
| Train ticket booking | `train_booking.vista` | Passenger details, origin/destination, date, passenger count, and travel class |
| Event registration | `event_registration.vista` | Attendee and ticket details, attendance type, and conditional accessibility-support details |

List the templates, then compile one by its ID from the repository root:

```bash
./build/vista --list-templates
./build/vista --template scholarship --emit all --out-dir out/scholarship
./build/vista --template train-booking --emit all --out-dir out/train-booking
./build/vista --template event-registration --emit all --out-dir out/event-registration
```

Then open the corresponding `out/<name>/form.html`. To compile your own form instead, pass its `.vista` path directly:

```bash
./build/vista path/to/my_form.vista --emit all --out-dir out/my-form
```

Templates do not restrict or change the compiler language.

These starter templates use illustrative fields and options. Adjust them to match the actual organization, route, event, eligibility rules, and privacy requirements before using them outside a demonstration.
