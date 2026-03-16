#include <gtk/gtk.h>
#include <string.h>

#include "markdown_buddy.h"

typedef struct AppWidgets {
    GtkTextBuffer *editor_buffer;
    GtkWidget *editor_view;
    GtkWidget *preview_label;
    GtkStringList *sections_model;
    GArray *section_offsets;
    guint refresh_source_id;
} AppWidgets;

static const char *APP_CSS =
    ".root-shell { background: linear-gradient(180deg, #192330 0%, #131a24 100%); }"
    ".sidebar-panel, .content-panel { background: rgba(41, 52, 72, 0.96); border: 1px solid rgba(129, 161, 193, 0.22); box-shadow: 0 16px 40px rgba(7, 11, 16, 0.28); }"
    ".sidebar-panel { border-right-color: rgba(129, 161, 193, 0.3); }"
    ".content-panel { border-radius: 16px; }"
    ".panel-header { padding: 12px 16px; border-bottom: 1px solid rgba(129, 161, 193, 0.14); background: rgba(44, 57, 79, 0.94); border-top-left-radius: 16px; border-top-right-radius: 16px; }"
    ".panel-title { font-family: 'IBM Plex Sans', sans-serif; font-size: 11px; font-weight: 700; letter-spacing: 0.16em; text-transform: uppercase; color: #9daccb; }"
    ".panel-body { padding: 10px; }"
    ".sidebar-list { padding: 6px; background: transparent; color: #c5d1e6; }"
    ".editor-view, .preview-view { background: #273142; color: #d6deeb; caret-color: #a3be8c; }"
    ".editor-view text selection, .preview-view selection { background-color: rgba(129, 161, 193, 0.35); }"
    "paned > separator { background: rgba(129, 161, 193, 0.2); min-width: 2px; }";

static const char *DEFAULT_DOCUMENT =
    "# Welcome to Markdown Buddy\n"
    "\n"
    "A native markdown workspace with **live preview**, *fast notes*, and handy section jumps.\n"
    "\n"
    "## Live Preview\n"
    "\n"
    "The Odin backend keeps the section list fresh while the GTK shell renders [the project repo](https://github.com/krisfur/markdown-buddy).\n"
    "\n"
    "> Write in plain markdown and get a calmer reading view beside the editor.\n"
    "\n"
    "## Next Steps\n"
    "\n"
    "- Open and save files\n"
    "- Polish more markdown details like **bold**, *italic*, and `inline code`\n"
    "- Keep section navigation feeling instant\n"
    "\n"
    "```\n"
    "markdown-buddy --build linux\n"
    "```\n";

static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, APP_CSS);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void clear_sections(AppWidgets *widgets) {
    while (g_list_model_get_n_items(G_LIST_MODEL(widgets->sections_model)) > 0) {
        gtk_string_list_remove(widgets->sections_model, 0);
    }

    if (widgets->section_offsets != NULL) {
        g_array_set_size(widgets->section_offsets, 0);
    }
}

static void append_section_label(AppWidgets *widgets, const MbSection *section) {
    GString *label = g_string_new(NULL);
    gint offset = section->source_offset;

    for (gint i = 1; i < section->level; ++i) {
        g_string_append(label, "  ");
    }

    if (section->title.data != NULL && section->title.length > 0) {
        g_string_append_len(label, section->title.data, (gssize) section->title.length);
    } else {
        g_string_append(label, "(untitled)");
    }

    gtk_string_list_append(widgets->sections_model, label->str);
    g_array_append_val(widgets->section_offsets, offset);
    g_string_free(label, TRUE);
}

static void jump_to_section(AppWidgets *widgets, guint position) {
    GtkTextIter start;
    GtkTextIter end;
    GtkTextIter iter;
    GtkTextMark *mark;
    gchar *text;
    gint byte_offset;
    gsize clamped;
    gint char_offset;

    if (widgets->section_offsets == NULL || position >= widgets->section_offsets->len) {
        return;
    }

    byte_offset = g_array_index(widgets->section_offsets, gint, position);
    gtk_text_buffer_get_bounds(widgets->editor_buffer, &start, &end);
    text = gtk_text_buffer_get_text(widgets->editor_buffer, &start, &end, FALSE);
    if (text == NULL) {
        return;
    }

    clamped = MIN((gsize) byte_offset, strlen(text));
    char_offset = g_utf8_pointer_to_offset(text, text + clamped);
    gtk_text_buffer_get_iter_at_offset(widgets->editor_buffer, &iter, char_offset);
    gtk_text_buffer_place_cursor(widgets->editor_buffer, &iter);
    mark = gtk_text_buffer_get_insert(widgets->editor_buffer);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(widgets->editor_view), mark);
    gtk_widget_grab_focus(widgets->editor_view);
    g_free(text);
}

static void append_escaped_markup(GString *markup, const char *text) {
    gchar *escaped = g_markup_escape_text(text != NULL ? text : "", -1);
    g_string_append(markup, escaped);
    g_free(escaped);
}

static void append_span_markup(GString *markup, const MbInlineSpan *span) {
    gchar *text = g_strndup(span->text.data != NULL ? span->text.data : "", span->text.length);

    switch (span->kind) {
    case MB_INLINE_EMPHASIS:
        g_string_append(markup, "<i>");
        append_escaped_markup(markup, text);
        g_string_append(markup, "</i>");
        break;
    case MB_INLINE_STRONG:
        g_string_append(markup, "<b>");
        append_escaped_markup(markup, text);
        g_string_append(markup, "</b>");
        break;
    case MB_INLINE_CODE:
        g_string_append(markup, "<span foreground='#8caaee' background='#39465e'><tt>");
        append_escaped_markup(markup, text);
        g_string_append(markup, "</tt></span>");
        break;
    case MB_INLINE_LINK: {
        gchar *href = g_strndup(span->href.data != NULL ? span->href.data : "", span->href.length);
        gchar *escaped_href = g_markup_escape_text(href, -1);
        g_string_append_printf(markup, "<span foreground='#7fc1ff'><a href=\"%s\">", escaped_href);
        append_escaped_markup(markup, text);
        g_string_append(markup, "</a></span>");
        g_free(escaped_href);
        g_free(href);
        break;
    }
    case MB_INLINE_TEXT:
    default:
        append_escaped_markup(markup, text);
        break;
    }

    g_free(text);
}

static void append_block_inline_markup(GString *markup, const MbDocumentResult *result, const MbPreviewBlock *block) {
    size_t end = block->span_start + block->span_count;
    for (size_t i = block->span_start; i < end && i < result->span_count; ++i) {
        append_span_markup(markup, &result->spans[i]);
    }
}

static gboolean block_is_tight(const MbPreviewBlock *block) {
    return block->kind == MB_BLOCK_LIST_ITEM || block->kind == MB_BLOCK_BLOCKQUOTE;
}

static void append_block_gap(GString *markup, const MbPreviewBlock *previous, const MbPreviewBlock *current) {
    if (previous == NULL) {
        return;
    }

    if (block_is_tight(previous) && block_is_tight(current) && previous->kind == current->kind) {
        g_string_append(markup, "\n");
        return;
    }

    if (previous->kind == MB_BLOCK_HEADING || current->kind == MB_BLOCK_HEADING) {
        g_string_append(markup, "\n\n");
        return;
    }

    if (previous->kind == MB_BLOCK_CODE_BLOCK || current->kind == MB_BLOCK_CODE_BLOCK) {
        g_string_append(markup, "\n\n");
        return;
    }

    if (block_is_tight(previous) || block_is_tight(current)) {
        g_string_append(markup, "\n\n");
        return;
    }

    g_string_append(markup, "\n\n");
}

static void render_preview(GtkLabel *label, const MbDocumentResult *result) {
    GString *markup;

    markup = g_string_new(NULL);

    if (result->block_count == 0) {
        g_string_append(markup, "<span foreground='#7f8da3'><i>Start writing markdown in the editor.</i></span>");
        gtk_label_set_markup(label, markup->str);
        g_string_free(markup, TRUE);
        return;
    }

    for (size_t i = 0; i < result->block_count; ++i) {
        const MbPreviewBlock *block = &result->blocks[i];
        const MbPreviewBlock *previous = i > 0 ? &result->blocks[i-1] : NULL;

        append_block_gap(markup, previous, block);

        switch (block->kind) {
        case MB_BLOCK_HEADING:
            if (block->level <= 1) {
                g_string_append(markup, "<span foreground='#d6deeb'><big><big><b>");
            } else if (block->level == 2) {
                g_string_append(markup, "<span foreground='#c7d2e0'><big><b>");
            } else {
                g_string_append(markup, "<span foreground='#b8c4d8'><b>");
            }
            append_block_inline_markup(markup, result, block);
            if (block->level <= 1) {
                g_string_append(markup, "</b></big></big></span>");
            } else if (block->level == 2) {
                g_string_append(markup, "</b></big></span>");
            } else {
                g_string_append(markup, "</b></span>");
            }
            break;
        case MB_BLOCK_LIST_ITEM:
            g_string_append(markup, "<span foreground='#81a1c1'>•</span> ");
            append_block_inline_markup(markup, result, block);
            break;
        case MB_BLOCK_BLOCKQUOTE:
            g_string_append(markup, "<span foreground='#719cd6'>│</span> <span foreground='#9fb4d0'><i>");
            append_block_inline_markup(markup, result, block);
            g_string_append(markup, "</i></span>");
            break;
        case MB_BLOCK_CODE_BLOCK: {
            gchar *text = g_strndup(block->text.data != NULL ? block->text.data : "", block->text.length);
            g_string_append(markup, "<span foreground='#a7c080' background='#1f2837'><tt>");
            append_escaped_markup(markup, text);
            g_string_append(markup, "</tt></span>");
            g_free(text);
            break;
        }
        case MB_BLOCK_PARAGRAPH:
        default:
            append_block_inline_markup(markup, result, block);
            break;
        }
    }

    gtk_label_set_markup(label, markup->str);
    g_string_free(markup, TRUE);
}

static void section_item_setup(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_widget_set_halign(label, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_lines(GTK_LABEL(label), 4);
    gtk_list_item_set_child(list_item, label);
    (void) factory;
    (void) user_data;
}

static void section_item_bind(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GtkWidget *label = gtk_list_item_get_child(list_item);
    GtkStringObject *item = GTK_STRING_OBJECT(gtk_list_item_get_item(list_item));
    gtk_label_set_text(GTK_LABEL(label), gtk_string_object_get_string(item));
    (void) factory;
    (void) user_data;
}

static void section_activated(GtkListView *view, guint position, gpointer user_data) {
    AppWidgets *widgets = user_data;
    (void) view;
    jump_to_section(widgets, position);
}

static void refresh_from_backend(AppWidgets *widgets) {
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;
    MbDocumentResult result = {0};
    MbStatus status;

    gtk_text_buffer_get_bounds(widgets->editor_buffer, &start, &end);
    text = gtk_text_buffer_get_text(widgets->editor_buffer, &start, &end, FALSE);
    status = mb_process_document(text, text != NULL ? strlen(text) : 0, &result);

    clear_sections(widgets);

    if (status == MB_STATUS_OK) {
        for (size_t i = 0; i < result.section_count; ++i) {
            append_section_label(widgets, &result.sections[i]);
        }
        render_preview(GTK_LABEL(widgets->preview_label), &result);
        mb_free_document_result(&result);
    } else {
        gtk_string_list_append(widgets->sections_model, "Backend error");
        gtk_label_set_markup(GTK_LABEL(widgets->preview_label), "Unable to render preview.");
    }

    g_free(text);
}

static gboolean delayed_refresh(gpointer user_data) {
    AppWidgets *widgets = user_data;
    widgets->refresh_source_id = 0;
    refresh_from_backend(widgets);
    return G_SOURCE_REMOVE;
}

static void schedule_refresh(AppWidgets *widgets) {
    if (widgets->refresh_source_id != 0) {
        g_source_remove(widgets->refresh_source_id);
    }

    widgets->refresh_source_id = g_timeout_add(120, delayed_refresh, widgets);
}

static void editor_changed(GtkTextBuffer *buffer, gpointer user_data) {
    AppWidgets *widgets = user_data;
    (void) buffer;
    schedule_refresh(widgets);
}

static GtkWidget *wrap_panel(const char *title_text, GtkWidget *child, const char *panel_class) {
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *title = gtk_label_new(title_text);

    gtk_widget_add_css_class(panel, panel_class);
    gtk_widget_add_css_class(header, "panel-header");
    gtk_widget_add_css_class(title, "panel-title");
    gtk_box_append(GTK_BOX(header), title);
    gtk_box_append(GTK_BOX(panel), header);
    gtk_box_append(GTK_BOX(panel), child);
    return panel;
}

static GtkWidget *build_sections_sidebar(AppWidgets *widgets) {
    GtkNoSelection *selection;
    GtkListItemFactory *factory;
    GtkWidget *list_view;
    GtkWidget *scroll;

    widgets->sections_model = gtk_string_list_new(NULL);
    widgets->section_offsets = g_array_new(FALSE, FALSE, sizeof(gint));
    selection = gtk_no_selection_new(G_LIST_MODEL(widgets->sections_model));
    factory = gtk_signal_list_item_factory_new();

    g_signal_connect(factory, "setup", G_CALLBACK(section_item_setup), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(section_item_bind), NULL);

    list_view = gtk_list_view_new(GTK_SELECTION_MODEL(selection), GTK_LIST_ITEM_FACTORY(factory));
    gtk_list_view_set_single_click_activate(GTK_LIST_VIEW(list_view), TRUE);
    g_signal_connect(list_view, "activate", G_CALLBACK(section_activated), widgets);

    scroll = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(scroll, FALSE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_add_css_class(scroll, "panel-body");
    gtk_widget_add_css_class(list_view, "sidebar-list");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list_view);

    return wrap_panel("Sections", scroll, "sidebar-panel");
}

static GtkWidget *build_editor_pane(AppWidgets *widgets) {
    GtkWidget *view = gtk_text_view_new();
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_add_css_class(view, "editor-view");
    gtk_widget_add_css_class(scroll, "panel-body");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);

    widgets->editor_view = view;
    widgets->editor_buffer = buffer;
    g_signal_connect(buffer, "changed", G_CALLBACK(editor_changed), widgets);
    return scroll;
}

static GtkWidget *build_preview_pane(AppWidgets *widgets) {
    GtkWidget *view = gtk_label_new(NULL);
    GtkWidget *scroll = gtk_scrolled_window_new();

    gtk_label_set_wrap(GTK_LABEL(view), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(view), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_xalign(GTK_LABEL(view), 0.0f);
    gtk_label_set_yalign(GTK_LABEL(view), 0.0f);
    gtk_label_set_selectable(GTK_LABEL(view), TRUE);
    gtk_widget_set_margin_top(view, 18);
    gtk_widget_set_margin_bottom(view, 18);
    gtk_widget_set_margin_start(view, 18);
    gtk_widget_set_margin_end(view, 18);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_add_css_class(view, "preview-view");
    gtk_widget_add_css_class(scroll, "panel-body");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);

    widgets->preview_label = view;
    return scroll;
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppWidgets *widgets = user_data;
    GtkWidget *window;
    GtkWidget *outer;
    GtkWidget *sidebar;
    GtkWidget *panes;
    GtkWidget *editor;
    GtkWidget *preview;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Markdown Buddy");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 800);
    load_css();

    outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(outer, TRUE);
    gtk_widget_set_vexpand(outer, TRUE);
    gtk_widget_add_css_class(outer, "root-shell");

    sidebar = build_sections_sidebar(widgets);
    gtk_widget_set_size_request(sidebar, 240, -1);
    gtk_box_append(GTK_BOX(outer), sidebar);

    panes = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_hexpand(panes, TRUE);
    gtk_widget_set_vexpand(panes, TRUE);
    gtk_widget_set_margin_start(panes, 12);
    gtk_widget_set_margin_top(panes, 12);
    gtk_widget_set_margin_end(panes, 12);
    gtk_widget_set_margin_bottom(panes, 12);
    gtk_paned_set_wide_handle(GTK_PANED(panes), TRUE);

    editor = wrap_panel("Editor", build_editor_pane(widgets), "content-panel");
    preview = wrap_panel("Preview", build_preview_pane(widgets), "content-panel");

    gtk_paned_set_start_child(GTK_PANED(panes), editor);
    gtk_paned_set_end_child(GTK_PANED(panes), preview);
    gtk_paned_set_position(GTK_PANED(panes), 530);

    gtk_box_append(GTK_BOX(outer), panes);
    gtk_window_set_child(GTK_WINDOW(window), outer);

    gtk_text_buffer_set_text(widgets->editor_buffer, DEFAULT_DOCUMENT, -1);
    refresh_from_backend(widgets);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    AppWidgets widgets = {0};
    GtkApplication *app;
    int status;

    app = gtk_application_new("dev.markdownbuddy.linux", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &widgets);
    status = g_application_run(G_APPLICATION(app), argc, argv);

    if (widgets.refresh_source_id != 0) {
        g_source_remove(widgets.refresh_source_id);
    }
    if (widgets.section_offsets != NULL) {
        g_array_unref(widgets.section_offsets);
    }
    if (widgets.sections_model != NULL && G_IS_OBJECT(widgets.sections_model)) {
        g_object_unref(widgets.sections_model);
    }
    if (app != NULL && G_IS_OBJECT(app)) {
        g_object_unref(app);
    }

    return status;
}
