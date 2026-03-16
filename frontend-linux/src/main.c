#include <gtk/gtk.h>
#include <string.h>

#include "markdown_buddy.h"

typedef enum PendingAction {
    PENDING_ACTION_NONE = 0,
    PENDING_ACTION_CLOSE,
    PENDING_ACTION_NEW,
    PENDING_ACTION_OPEN_DIALOG,
    PENDING_ACTION_OPEN_PATH,
} PendingAction;

typedef struct AppWidgets {
    GtkApplication *app;
    GtkWindow *window;
    GtkTextBuffer *editor_buffer;
    GtkWidget *editor_view;
    GtkWidget *preview_label;
    GtkStringList *sections_model;
    GArray *section_offsets;
    gchar *current_path;
    gchar *pending_path;
    guint refresh_source_id;
    gboolean is_dirty;
    gboolean is_loading_document;
    PendingAction pending_action;
} AppWidgets;

static const char *APP_CSS =
    ".root-shell { background: linear-gradient(180deg, #192330 0%, #131a24 100%); }"
    ".app-menu { padding: 8px 12px 0 12px; }"
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

static void refresh_from_backend(AppWidgets *widgets);
static void save_document_as(AppWidgets *widgets);
static void choose_open_file(AppWidgets *widgets);

static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, APP_CSS);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static const char *display_name_for_path(const gchar *path) {
    const gchar *name;

    if (path == NULL || *path == '\0') {
        return "Untitled";
    }

    name = g_path_get_basename(path);
    return name;
}

static void update_window_title(AppWidgets *widgets) {
    gchar *basename;
    gchar *title;

    basename = widgets->current_path != NULL ? g_path_get_basename(widgets->current_path) : g_strdup("Untitled");
    title = g_strdup_printf("%s%s - Markdown Buddy", basename, widgets->is_dirty ? " *" : "");
    gtk_window_set_title(widgets->window, title);
    g_free(title);
    g_free(basename);
}

static void set_current_path(AppWidgets *widgets, const gchar *path) {
    g_free(widgets->current_path);
    widgets->current_path = path != NULL ? g_strdup(path) : NULL;
    update_window_title(widgets);
}

static void set_dirty(AppWidgets *widgets, gboolean is_dirty) {
    widgets->is_dirty = is_dirty;
    update_window_title(widgets);
}

static void set_pending_action(AppWidgets *widgets, PendingAction action, const gchar *path) {
    widgets->pending_action = action;
    g_free(widgets->pending_path);
    widgets->pending_path = path != NULL ? g_strdup(path) : NULL;
}

static void show_error_dialog(AppWidgets *widgets, const gchar *message) {
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", "Unable to complete action");
    gtk_alert_dialog_set_detail(dialog, message);
    gtk_alert_dialog_set_modal(dialog, TRUE);
    gtk_alert_dialog_show(dialog, widgets->window);
    g_object_unref(dialog);
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
        g_string_append_len(label, section->title.data, (gssize)section->title.length);
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

    clamped = MIN((gsize)byte_offset, strlen(text));
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
    GString *markup = g_string_new(NULL);

    if (result->block_count == 0) {
        g_string_append(markup, "<span foreground='#7f8da3'><i>Start writing markdown in the editor.</i></span>");
        gtk_label_set_markup(label, markup->str);
        g_string_free(markup, TRUE);
        return;
    }

    for (size_t i = 0; i < result->block_count; ++i) {
        const MbPreviewBlock *block = &result->blocks[i];
        const MbPreviewBlock *previous = i > 0 ? &result->blocks[i - 1] : NULL;

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

static gboolean load_document_from_path(AppWidgets *widgets, const gchar *path) {
    gchar *contents = NULL;
    gsize length = 0;
    GError *error = NULL;
    gchar *resolved;

    if (!g_file_get_contents(path, &contents, &length, &error)) {
        gchar *message = g_strdup_printf("Unable to open file:\n%s", error->message);
        show_error_dialog(widgets, message);
        g_free(message);
        g_clear_error(&error);
        return FALSE;
    }

    widgets->is_loading_document = TRUE;
    gtk_text_buffer_set_text(widgets->editor_buffer, contents, (gint)length);
    widgets->is_loading_document = FALSE;

    resolved = g_canonicalize_filename(path, NULL);
    set_current_path(widgets, resolved);
    set_dirty(widgets, FALSE);
    refresh_from_backend(widgets);

    g_free(resolved);
    g_free(contents);
    return TRUE;
}

static void new_document(AppWidgets *widgets) {
    widgets->is_loading_document = TRUE;
    gtk_text_buffer_set_text(widgets->editor_buffer, "", -1);
    widgets->is_loading_document = FALSE;
    set_current_path(widgets, NULL);
    set_dirty(widgets, FALSE);
    refresh_from_backend(widgets);
}

static void perform_pending_action(AppWidgets *widgets) {
    PendingAction action = widgets->pending_action;
    gchar *path = widgets->pending_path != NULL ? g_strdup(widgets->pending_path) : NULL;

    set_pending_action(widgets, PENDING_ACTION_NONE, NULL);

    switch (action) {
    case PENDING_ACTION_CLOSE:
        gtk_window_destroy(widgets->window);
        break;
    case PENDING_ACTION_NEW:
        new_document(widgets);
        break;
    case PENDING_ACTION_OPEN_DIALOG:
        choose_open_file(widgets);
        break;
    case PENDING_ACTION_OPEN_PATH:
        if (path != NULL) {
            load_document_from_path(widgets, path);
        }
        break;
    case PENDING_ACTION_NONE:
    default:
        break;
    }

    g_free(path);
}

static gboolean save_document_to_path(AppWidgets *widgets, const gchar *path) {
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;
    GError *error = NULL;
    gchar *resolved;

    gtk_text_buffer_get_bounds(widgets->editor_buffer, &start, &end);
    text = gtk_text_buffer_get_text(widgets->editor_buffer, &start, &end, FALSE);

    if (!g_file_set_contents(path, text != NULL ? text : "", -1, &error)) {
        gchar *message = g_strdup_printf("Unable to save file:\n%s", error->message);
        show_error_dialog(widgets, message);
        g_free(message);
        g_clear_error(&error);
        g_free(text);
        return FALSE;
    }

    resolved = g_canonicalize_filename(path, NULL);
    set_current_path(widgets, resolved);
    set_dirty(widgets, FALSE);
    perform_pending_action(widgets);

    g_free(resolved);
    g_free(text);
    return TRUE;
}

static GListModel *create_markdown_filters(void) {
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    GtkFileFilter *markdown = gtk_file_filter_new();
    GtkFileFilter *all_files = gtk_file_filter_new();

    gtk_file_filter_set_name(markdown, "Markdown files");
    gtk_file_filter_add_pattern(markdown, "*.md");
    gtk_file_filter_add_pattern(markdown, "*.markdown");
    g_list_store_append(filters, markdown);

    gtk_file_filter_set_name(all_files, "All files");
    gtk_file_filter_add_pattern(all_files, "*");
    g_list_store_append(filters, all_files);

    g_object_unref(markdown);
    g_object_unref(all_files);
    return G_LIST_MODEL(filters);
}

static void save_dialog_response(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    AppWidgets *widgets = user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(dialog, result, &error);

    if (file != NULL) {
        gchar *path = g_file_get_path(file);
        save_document_to_path(widgets, path);
        g_free(path);
        g_object_unref(file);
    } else if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED)) {
        gchar *message = g_strdup_printf("Unable to choose save location:\n%s", error->message);
        show_error_dialog(widgets, message);
        g_free(message);
    } else {
        set_pending_action(widgets, PENDING_ACTION_NONE, NULL);
    }

    g_clear_error(&error);
}

static void save_document_as(AppWidgets *widgets) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    GListModel *filters = create_markdown_filters();

    gtk_file_dialog_set_title(dialog, "Save Markdown File");
    gtk_file_dialog_set_modal(dialog, TRUE);
    gtk_file_dialog_set_filters(dialog, filters);
    gtk_file_dialog_set_default_filter(dialog, GTK_FILE_FILTER(g_list_model_get_item(filters, 0)));
    gtk_file_dialog_set_initial_name(dialog, widgets->current_path != NULL ? display_name_for_path(widgets->current_path) : "untitled.md");

    if (widgets->current_path != NULL) {
        GFile *file = g_file_new_for_path(widgets->current_path);
        gtk_file_dialog_set_initial_file(dialog, file);
        g_object_unref(file);
    }

    gtk_file_dialog_save(dialog, widgets->window, NULL, save_dialog_response, widgets);
    g_object_unref(filters);
    g_object_unref(dialog);
}

static void maybe_save_document(AppWidgets *widgets) {
    if (widgets->current_path != NULL) {
        if (!save_document_to_path(widgets, widgets->current_path)) {
            set_pending_action(widgets, PENDING_ACTION_NONE, NULL);
        }
        return;
    }

    save_document_as(widgets);
}

static void open_dialog_response(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    AppWidgets *widgets = user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(dialog, result, &error);

    if (file != NULL) {
        gchar *path = g_file_get_path(file);
        load_document_from_path(widgets, path);
        g_free(path);
        g_object_unref(file);
    } else if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED)) {
        gchar *message = g_strdup_printf("Unable to open file chooser:\n%s", error->message);
        show_error_dialog(widgets, message);
        g_free(message);
    }

    g_clear_error(&error);
}

static void choose_open_file(AppWidgets *widgets) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    GListModel *filters = create_markdown_filters();

    gtk_file_dialog_set_title(dialog, "Open Markdown File");
    gtk_file_dialog_set_modal(dialog, TRUE);
    gtk_file_dialog_set_filters(dialog, filters);
    gtk_file_dialog_set_default_filter(dialog, GTK_FILE_FILTER(g_list_model_get_item(filters, 0)));

    gtk_file_dialog_open(dialog, widgets->window, NULL, open_dialog_response, widgets);
    g_object_unref(filters);
    g_object_unref(dialog);
}

static gboolean request_document_replacement(AppWidgets *widgets, PendingAction action, const gchar *path);

static void close_confirm_response(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    AppWidgets *widgets = user_data;
    GtkAlertDialog *dialog = GTK_ALERT_DIALOG(source_object);
    GError *error = NULL;
    int response = gtk_alert_dialog_choose_finish(dialog, result, &error);

    if (error != NULL) {
        g_clear_error(&error);
        return;
    }

    if (response == 2) {
        maybe_save_document(widgets);
    } else if (response == 1) {
        perform_pending_action(widgets);
    } else {
        set_pending_action(widgets, PENDING_ACTION_NONE, NULL);
    }
}

static gboolean confirm_discard_changes(AppWidgets *widgets, PendingAction action, const gchar *path) {
    GtkAlertDialog *dialog;
    const char *buttons[] = {"Cancel", "Discard", "Save", NULL};

    if (!widgets->is_dirty) {
        return FALSE;
    }

    set_pending_action(widgets, action, path);

    dialog = gtk_alert_dialog_new("%s", "Save changes before closing?");
    gtk_alert_dialog_set_detail(dialog, "Your current document has unsaved changes.");
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 2);
    gtk_alert_dialog_set_modal(dialog, TRUE);
    gtk_alert_dialog_choose(dialog, widgets->window, NULL, close_confirm_response, widgets);
    g_object_unref(dialog);
    return TRUE;
}

static gboolean request_document_replacement(AppWidgets *widgets, PendingAction action, const gchar *path) {
    if (widgets->is_dirty) {
        return confirm_discard_changes(widgets, action, path);
    }

    set_pending_action(widgets, action, path);
    perform_pending_action(widgets);
    return FALSE;
}

static void open_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    AppWidgets *widgets = user_data;
    (void)action;
    (void)parameter;
    request_document_replacement(widgets, PENDING_ACTION_OPEN_DIALOG, NULL);
}

static void new_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    AppWidgets *widgets = user_data;
    (void)action;
    (void)parameter;
    request_document_replacement(widgets, PENDING_ACTION_NEW, NULL);
}

static void save_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    AppWidgets *widgets = user_data;
    (void)action;
    (void)parameter;
    set_pending_action(widgets, PENDING_ACTION_NONE, NULL);
    maybe_save_document(widgets);
}

static void save_as_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    AppWidgets *widgets = user_data;
    (void)action;
    (void)parameter;
    set_pending_action(widgets, PENDING_ACTION_NONE, NULL);
    save_document_as(widgets);
}

static void editor_changed(GtkTextBuffer *buffer, gpointer user_data) {
    AppWidgets *widgets = user_data;
    (void)buffer;

    if (widgets->is_loading_document) {
        return;
    }

    set_dirty(widgets, TRUE);
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

static void section_item_setup(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_widget_set_halign(label, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_lines(GTK_LABEL(label), 4);
    gtk_list_item_set_child(list_item, label);
    (void)factory;
    (void)user_data;
}

static void section_item_bind(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GtkWidget *label = gtk_list_item_get_child(list_item);
    GtkStringObject *item = GTK_STRING_OBJECT(gtk_list_item_get_item(list_item));
    gtk_label_set_text(GTK_LABEL(label), gtk_string_object_get_string(item));
    (void)factory;
    (void)user_data;
}

static void section_activated(GtkListView *view, guint position, gpointer user_data) {
    AppWidgets *widgets = user_data;
    (void)view;
    jump_to_section(widgets, position);
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

static GtkWidget *build_menu_bar(void) {
    GMenu *root = g_menu_new();
    GMenu *file = g_menu_new();

    g_menu_append(file, "New", "app.new");
    g_menu_append(file, "Open", "app.open");
    g_menu_append(file, "Save", "app.save");
    g_menu_append(file, "Save As", "app.save-as");
    g_menu_append_submenu(root, "File", G_MENU_MODEL(file));

    g_object_unref(file);
    return gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(root));
}

static gboolean on_close_request(GtkWindow *window, gpointer user_data) {
    AppWidgets *widgets = user_data;
    (void)window;
    return request_document_replacement(widgets, PENDING_ACTION_CLOSE, NULL);
}

static void install_actions(AppWidgets *widgets) {
    const GActionEntry app_actions[] = {
        {"new", new_action, NULL, NULL, NULL},
        {"open", open_action, NULL, NULL, NULL},
        {"save", save_action, NULL, NULL, NULL},
        {"save-as", save_as_action, NULL, NULL, NULL},
    };

    g_action_map_add_action_entries(G_ACTION_MAP(widgets->app), app_actions, G_N_ELEMENTS(app_actions), widgets);
    gtk_application_set_accels_for_action(widgets->app, "app.new", (const char *[]) {"<Primary>n", NULL});
    gtk_application_set_accels_for_action(widgets->app, "app.open", (const char *[]) {"<Primary>o", NULL});
    gtk_application_set_accels_for_action(widgets->app, "app.save", (const char *[]) {"<Primary>s", NULL});
    gtk_application_set_accels_for_action(widgets->app, "app.save-as", (const char *[]) {"<Primary><Shift>s", NULL});
}

static void ensure_window(AppWidgets *widgets, GtkApplication *app) {
    GtkWidget *window;
    GtkWidget *shell;
    GtkWidget *menu_bar;
    GtkWidget *outer;
    GtkWidget *sidebar;
    GtkWidget *panes;
    GtkWidget *editor;
    GtkWidget *preview;

    if (widgets->window != NULL) {
        return;
    }

    widgets->app = app;
    window = gtk_application_window_new(app);
    widgets->window = GTK_WINDOW(window);

    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 800);
    load_css();
    install_actions(widgets);
    g_signal_connect(window, "close-request", G_CALLBACK(on_close_request), widgets);

    shell = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(shell, "root-shell");

    menu_bar = build_menu_bar();
    gtk_widget_add_css_class(menu_bar, "app-menu");
    gtk_box_append(GTK_BOX(shell), menu_bar);

    outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(outer, TRUE);
    gtk_widget_set_vexpand(outer, TRUE);
    gtk_box_append(GTK_BOX(shell), outer);

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

    gtk_window_set_child(GTK_WINDOW(window), shell);

    widgets->is_loading_document = TRUE;
    gtk_text_buffer_set_text(widgets->editor_buffer, "", -1);
    widgets->is_loading_document = FALSE;
    set_current_path(widgets, NULL);
    set_dirty(widgets, FALSE);
    refresh_from_backend(widgets);
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppWidgets *widgets = user_data;
    ensure_window(widgets, app);
    gtk_window_present(widgets->window);
}

static void open_files(GtkApplication *app, GFile **files, gint n_files, const gchar *hint, gpointer user_data) {
    AppWidgets *widgets = user_data;
    gchar *path;

    (void)hint;
    ensure_window(widgets, app);

    if (n_files > 1) {
        show_error_dialog(widgets, "Only one markdown file can be opened at a time.");
        gtk_window_present(widgets->window);
        return;
    }

    if (n_files == 1) {
        path = g_file_get_path(files[0]);
        if (path != NULL) {
            request_document_replacement(widgets, PENDING_ACTION_OPEN_PATH, path);
            g_free(path);
        }
    }

    gtk_window_present(widgets->window);
}

int main(int argc, char **argv) {
    AppWidgets widgets = {0};
    GtkApplication *app;
    int status;

    if (argc > 2) {
        g_printerr("Usage: %s [path-to-markdown-file]\n", argv[0]);
        return 1;
    }

    app = gtk_application_new("dev.markdownbuddy.linux", G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &widgets);
    g_signal_connect(app, "open", G_CALLBACK(open_files), &widgets);
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
    g_free(widgets.current_path);
    g_free(widgets.pending_path);
    g_object_unref(app);
    return status;
}
