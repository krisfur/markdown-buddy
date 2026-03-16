# Markdown Buddy Demo

Markdown Buddy is a small native editor for writing notes with a live preview, a section list, and a simple desktop workflow.

## What It Shows Off

This file is meant to exercise the preview with **bold text**, *italic text*, `inline code`, and [external links](https://github.com/krisfur/markdown-buddy).

> The goal is not to be exhaustive markdown documentation.
> It is a readable sample that touches the features the app supports today.

## Everyday Writing

Imagine this as the start of a project note.

You might describe a feature in plain language, call out an important API like `mb_process_document`, and link to a design reference in [the repository](https://github.com/krisfur/markdown-buddy).

The preview should keep paragraphs readable and spaced apart, while the sidebar should pick up each heading as a navigable section.

### Quick Checklist

- Open an existing markdown file
- Start a *new* note from scratch
- Save changes with **Ctrl+S**
- Jump between sections from the left sidebar
- Close the window and get a save prompt if the document is dirty

### A Small Quote

> Good tools do not just work.
> They make the common path feel calm.

## Technical Notes

The application currently uses an Odin backend with a C ABI and a GTK4 frontend on Linux.

That split means markdown interpretation can stay centralized while native frontends focus on platform behavior, menus, file dialogs, and rendering.

### Shared Model Ideas

- Keep markdown parsing in **Odin**
- Keep native interactions in *GTK4* for Linux
- Reuse the same preview model for future frontends

### Code Sample

```text
mb_process_document(input_utf8, input_length, &result)
mb_free_document_result(&result)
```

The fenced block above should read as a distinct code block rather than a normal paragraph.

## Writing Patterns

Sometimes a note needs a mix of explanatory prose and quick fragments of code like `build-linux.sh` or `./dist/markdown-buddy-gtk4 example.md`.

Other times it needs a stronger emphasis:

- **Important:** file handling belongs in the native shell.
- *Useful:* parsing and preview semantics belong in the backend.
- `Practical:` short commands and identifiers should stand out visually.

## Longer Section

This section is here mostly to make the document feel a bit more real.

When you scroll through a longer file, the preview should continue to preserve separation between headings, paragraphs, lists, quotes, and code blocks. The sidebar should also remain useful once the document has enough structure to warrant jumping around.

> A section list becomes much more valuable once the document stops fitting comfortably on one screen.

### Nested Thoughts

There is no nested list support yet in the preview model, but this sample still gives enough structure to test navigation, formatting, and visual rhythm.

You can also use this file to test:

- opening from the CLI
- opening from the File menu
- save and save as flows
- undo and redo from the Edit menu

## Closing Notes

If this file looks good in the preview, the app is already covering a pleasant and surprisingly useful subset of markdown writing.

The next wave of polish can focus on editor commands, richer transformations, and eventually more advanced preview behavior.
