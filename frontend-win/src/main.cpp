#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#define NOMINMAX

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include "markdown_buddy.h"
}

namespace {

constexpr wchar_t kWindowClassName[] = L"MarkdownBuddyWin32Window";
constexpr UINT_PTR kRefreshTimerId = 1;
constexpr UINT kRefreshDelayMs = 120;
constexpr int kSidebarWidth = 252;
constexpr int kOuterPadding = 12;
constexpr int kPanelGap = 12;
constexpr int kHeaderHeight = 40;
constexpr int kInnerPadding = 14;

constexpr int kIdSectionsHeader = 100;
constexpr int kIdSectionsList = 101;
constexpr int kIdEditorHeader = 102;
constexpr int kIdEditor = 103;
constexpr int kIdPreviewHeader = 104;
constexpr int kIdPreview = 105;

constexpr UINT kCommandNew = 40001;
constexpr UINT kCommandOpen = 40002;
constexpr UINT kCommandSave = 40003;
constexpr UINT kCommandSaveAs = 40004;
constexpr UINT kCommandQuit = 40005;
constexpr UINT kCommandUndo = 40006;
constexpr UINT kCommandRedo = 40007;
constexpr UINT kCommandCut = 40008;
constexpr UINT kCommandCopy = 40009;
constexpr UINT kCommandPaste = 40010;
constexpr UINT kCommandSelectAll = 40011;
constexpr UINT kCommandBold = 40012;
constexpr UINT kCommandItalic = 40013;
constexpr UINT kCommandAbout = 40014;
constexpr UINT kCommandRepository = 40015;

constexpr COLORREF kShellTop = RGB(25, 35, 48);
constexpr COLORREF kShellBottom = RGB(19, 26, 36);
constexpr COLORREF kSidebarFill = RGB(41, 52, 72);
constexpr COLORREF kPanelFill = RGB(39, 49, 66);
constexpr COLORREF kPanelHeaderFill = RGB(44, 57, 79);
constexpr COLORREF kBorderColor = RGB(77, 97, 122);
constexpr COLORREF kHeaderText = RGB(157, 172, 203);
constexpr COLORREF kBodyText = RGB(214, 222, 235);
constexpr COLORREF kMutedText = RGB(127, 141, 163);
constexpr COLORREF kLinkText = RGB(127, 193, 255);
constexpr COLORREF kCodeText = RGB(140, 170, 238);
constexpr COLORREF kCaretColor = RGB(163, 190, 140);
constexpr COLORREF kQuoteText = RGB(166, 184, 212);
constexpr COLORREF kCodeBackground = RGB(31, 40, 56);

struct UiBrushes {
    HBRUSH shell_top = nullptr;
    HBRUSH shell_bottom = nullptr;
    HBRUSH sidebar_fill = nullptr;
    HBRUSH panel_fill = nullptr;
    HBRUSH panel_header_fill = nullptr;
};

struct Fonts {
    HFONT ui = nullptr;
    HFONT ui_small = nullptr;
    HFONT mono = nullptr;
};

using ProcessDocumentFn = MbStatus (*)(const char *, size_t, MbDocumentResult *);
using FreeDocumentFn = void (*)(MbDocumentResult *);
using ApplyEditFn = MbStatus (*)(const char *, size_t, int32_t, int32_t, int32_t, MbEditResult *);
using FreeEditFn = void (*)(MbEditResult *);

struct BackendApi {
    HMODULE module = nullptr;
    ProcessDocumentFn process_document = nullptr;
    FreeDocumentFn free_document = nullptr;
    ApplyEditFn apply_edit = nullptr;
    FreeEditFn free_edit = nullptr;
    std::wstring status_message;

    bool available() const {
        return process_document != nullptr && free_document != nullptr && apply_edit != nullptr && free_edit != nullptr;
    }
};

struct SectionEntry {
    std::wstring label;
    int32_t source_offset = 0;
};

struct WindowLayout {
    RECT sidebar;
    RECT editor;
    RECT preview;
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HWND sections_header = nullptr;
    HWND sections_list = nullptr;
    HWND editor_header = nullptr;
    HWND editor = nullptr;
    HWND preview_header = nullptr;
    HWND preview = nullptr;
    HMENU menu = nullptr;
    HACCEL accelerators = nullptr;
    UiBrushes brushes;
    Fonts fonts;
    BackendApi backend;
    std::vector<SectionEntry> sections;
    std::wstring current_path;
    bool has_path = false;
    bool is_dirty = false;
    bool is_programmatic_change = false;
    HMODULE rich_edit_module = nullptr;
};

AppState *g_app = nullptr;

std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }

    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }

    std::wstring wide(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), needed);
    return wide;
}

std::string narrow(const std::wstring &wide) {
    if (wide.empty()) {
        return {};
    }

    int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }

    std::string utf8(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), needed, nullptr, nullptr);
    return utf8;
}

std::wstring view_to_wstring(const MbString &value) {
    if (value.data == nullptr || value.length == 0) {
        return {};
    }
    return widen(std::string_view(value.data, value.length));
}

bool should_use_tight_block_spacing(int32_t previous_kind, int32_t current_kind) {
    return (previous_kind == MB_BLOCK_LIST_ITEM && current_kind == MB_BLOCK_LIST_ITEM) ||
           (previous_kind == MB_BLOCK_BLOCKQUOTE && current_kind == MB_BLOCK_BLOCKQUOTE) ||
           (previous_kind == MB_BLOCK_CODE_BLOCK && current_kind == MB_BLOCK_CODE_BLOCK);
}

CHARFORMAT2W make_text_format(COLORREF color, const wchar_t *face_name, LONG height_twips, DWORD effects = 0, DWORD mask_extra = 0, COLORREF back_color = kPanelFill) {
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_BACKCOLOR | mask_extra;
    format.dwEffects = effects;
    format.crTextColor = color;
    format.crBackColor = back_color;
    format.yHeight = height_twips;
    wcscpy_s(format.szFaceName, face_name);
    return format;
}

PARAFORMAT2 make_paragraph_format(LONG start_indent = 0, LONG right_indent = 0) {
    PARAFORMAT2 format{};
    format.cbSize = sizeof(format);
    format.dwMask = 0;
    if (start_indent != 0) {
        format.dwMask |= PFM_STARTINDENT;
        format.dxStartIndent = start_indent;
    }
    if (right_indent != 0) {
        format.dwMask |= PFM_RIGHTINDENT;
        format.dxRightIndent = right_indent;
    }
    return format;
}

void append_preview_chunk(HWND control, const std::wstring &text, const CHARFORMAT2W &char_format, const PARAFORMAT2 *paragraph_format = nullptr) {
    if (text.empty()) {
        return;
    }

    CHARRANGE end_range{-1, -1};
    SendMessageW(control, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&end_range));
    if (paragraph_format != nullptr && paragraph_format->dwMask != 0) {
        SendMessageW(control, EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(paragraph_format));
    }
    SendMessageW(control, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&char_format));
    SendMessageW(control, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
}

CHARFORMAT2W block_base_format(const MbPreviewBlock &block) {
    if (block.kind == MB_BLOCK_HEADING) {
        if (block.level <= 1) {
            return make_text_format(RGB(214, 222, 235), L"Segoe UI Variable Text", 28 * 20, CFE_BOLD, CFM_BOLD);
        }
        if (block.level == 2) {
            return make_text_format(RGB(199, 210, 224), L"Segoe UI Variable Text", 22 * 20, CFE_BOLD, CFM_BOLD);
        }
        return make_text_format(RGB(184, 196, 216), L"Segoe UI Variable Text", 18 * 20, CFE_BOLD, CFM_BOLD);
    }
    if (block.kind == MB_BLOCK_BLOCKQUOTE) {
        return make_text_format(RGB(159, 180, 208), L"Segoe UI Variable Text", 18 * 20, CFE_ITALIC, CFM_ITALIC);
    }
    if (block.kind == MB_BLOCK_CODE_BLOCK) {
        return make_text_format(RGB(167, 192, 128), L"Consolas", 17 * 20, 0, 0, RGB(31, 40, 55));
    }
    return make_text_format(kBodyText, L"Segoe UI Variable Text", 18 * 20);
}

CHARFORMAT2W span_format_for_block(const MbPreviewBlock &block, const MbInlineSpan &span) {
    CHARFORMAT2W format = block_base_format(block);
    switch (span.kind) {
    case MB_INLINE_EMPHASIS:
        format.dwMask |= CFM_ITALIC;
        format.dwEffects |= CFE_ITALIC;
        break;
    case MB_INLINE_STRONG:
        format.dwMask |= CFM_BOLD;
        format.dwEffects |= CFE_BOLD;
        break;
    case MB_INLINE_CODE:
        format = make_text_format(RGB(140, 170, 238), L"Consolas", 17 * 20, 0, 0, RGB(57, 70, 94));
        break;
    case MB_INLINE_LINK:
        format.dwMask |= CFM_UNDERLINE;
        format.dwEffects |= CFE_UNDERLINE;
        format.crTextColor = RGB(127, 193, 255);
        break;
    case MB_INLINE_TEXT:
    default:
        break;
    }
    return format;
}

void append_block_inline_content(HWND control, const MbDocumentResult &result, const MbPreviewBlock &block) {
    if (block.span_count > 0 && result.spans != nullptr) {
        size_t end = std::min(result.span_count, block.span_start + block.span_count);
        for (size_t i = block.span_start; i < end; ++i) {
            const MbInlineSpan &span = result.spans[i];
            append_preview_chunk(control, view_to_wstring(span.text), span_format_for_block(block, span));
        }
        return;
    }

    append_preview_chunk(control, view_to_wstring(block.text), block_base_format(block));
}

void render_preview_document(AppState &app, const MbDocumentResult &result) {
    SendMessageW(app.preview, WM_SETREDRAW, FALSE, 0);
    SetWindowTextW(app.preview, L"");

    if (result.block_count == 0) {
        append_preview_chunk(app.preview, L"Start writing markdown in the editor.", make_text_format(kMutedText, L"Segoe UI Variable Text", 18 * 20, CFE_ITALIC, CFM_ITALIC));
        SendMessageW(app.preview, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(app.preview, nullptr, TRUE);
        return;
    }

    int32_t previous_kind = -1;
    for (size_t i = 0; i < result.block_count; ++i) {
        const MbPreviewBlock &block = result.blocks[i];
        if (i > 0) {
            append_preview_chunk(app.preview, should_use_tight_block_spacing(previous_kind, block.kind) ? L"\r\n" : L"\r\n\r\n", make_text_format(kBodyText, L"Segoe UI Variable Text", 18 * 20));
        }

        if (block.kind == MB_BLOCK_LIST_ITEM) {
            append_preview_chunk(app.preview, L"• ", make_text_format(RGB(129, 161, 193), L"Segoe UI Variable Text", 18 * 20));
        } else if (block.kind == MB_BLOCK_BLOCKQUOTE) {
            append_preview_chunk(app.preview, L"│ ", make_text_format(RGB(113, 156, 214), L"Segoe UI Variable Text", 18 * 20));
        }

        if (block.kind == MB_BLOCK_CODE_BLOCK) {
            PARAFORMAT2 code_para = make_paragraph_format(220, 160);
            append_preview_chunk(app.preview, view_to_wstring(block.text), block_base_format(block), &code_para);
        } else {
            append_block_inline_content(app.preview, result, block);
        }

        previous_kind = block.kind;
    }

    SendMessageW(app.preview, EM_SETSEL, 0, 0);
    SendMessageW(app.preview, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(app.preview, nullptr, TRUE);
}

std::wstring copy_edit_text(HWND control) {
    int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    if (length > 0) {
        GetWindowTextW(control, text.data(), length + 1);
    }
    text.resize(static_cast<size_t>(length));
    return text;
}

std::wstring file_display_name(const AppState &app) {
    if (!app.has_path || app.current_path.empty()) {
        return L"Untitled";
    }
    return std::filesystem::path(app.current_path).filename().wstring();
}

void set_window_title(AppState &app) {
    std::wstring title = file_display_name(app);
    if (app.is_dirty) {
        title += L" *";
    }
    title += L" - Markdown Buddy";
    SetWindowTextW(app.window, title.c_str());
}

void set_dirty(AppState &app, bool dirty) {
    app.is_dirty = dirty;
    set_window_title(app);
}

void show_message_box(HWND owner, const wchar_t *title, const std::wstring &message, UINT flags = MB_OK | MB_ICONINFORMATION) {
    MessageBoxW(owner, message.c_str(), title, flags);
}

bool is_running_under_wine() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

bool copy_text_to_clipboard(HWND owner, const std::wstring &text) {
    if (!OpenClipboard(owner)) {
        return false;
    }

    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
        CloseClipboard();
        return false;
    }

    void *buffer = GlobalLock(memory);
    memcpy(buffer, text.c_str(), bytes);
    GlobalUnlock(memory);
    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

bool launch_unix_command_via_wine(const std::wstring &program, const std::wstring &arguments) {
    std::wstring command_line = L"cmd.exe /c start \"\" /unix \"" + program + L"\" " + arguments;
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    BOOL created = CreateProcessW(
        nullptr,
        mutable_command.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process
    );
    if (created) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
    }
    return false;
}

bool confirm_discard_changes(AppState &app) {
    if (!app.is_dirty) {
        return true;
    }

    int result = MessageBoxW(
        app.window,
        L"Your current document has unsaved changes. Continue without saving?",
        L"Unsaved Changes",
        MB_YESNOCANCEL | MB_ICONWARNING
    );
    return result == IDYES;
}

void clear_refresh_timer(AppState &app) {
    KillTimer(app.window, kRefreshTimerId);
}

void schedule_refresh(AppState &app) {
    KillTimer(app.window, kRefreshTimerId);
    SetTimer(app.window, kRefreshTimerId, kRefreshDelayMs, nullptr);
}

std::wstring executable_directory() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return std::filesystem::path(path).parent_path().wstring();
}

void unload_backend(BackendApi &backend) {
    if (backend.module != nullptr) {
        FreeLibrary(backend.module);
    }
    backend = {};
}

void load_backend(BackendApi &backend) {
    unload_backend(backend);

    std::filesystem::path dll_path = std::filesystem::path(executable_directory()) / L"markdown_buddy.dll";
    HMODULE module = LoadLibraryW(dll_path.c_str());
    if (module == nullptr) {
        backend.status_message = L"Preview backend unavailable. Build or place markdown_buddy.dll next to the executable.";
        return;
    }

    backend.module = module;
    backend.process_document = reinterpret_cast<ProcessDocumentFn>(GetProcAddress(module, "mb_process_document"));
    backend.free_document = reinterpret_cast<FreeDocumentFn>(GetProcAddress(module, "mb_free_document_result"));
    backend.apply_edit = reinterpret_cast<ApplyEditFn>(GetProcAddress(module, "mb_apply_edit_command"));
    backend.free_edit = reinterpret_cast<FreeEditFn>(GetProcAddress(module, "mb_free_edit_result"));
    if (!backend.available()) {
        unload_backend(backend);
        backend.status_message = L"Preview backend DLL is present, but required exported symbols were not found.";
        return;
    }

    backend.status_message = L"";
}

size_t utf8_bytes_for_codepoint(wchar_t first, wchar_t second, int *units_used) {
    *units_used = 1;

    if (first >= 0xD800 && first <= 0xDBFF && second >= 0xDC00 && second <= 0xDFFF) {
        *units_used = 2;
        return 4;
    }
    if (first <= 0x7F) {
        return 1;
    }
    if (first <= 0x7FF) {
        return 2;
    }
    return 3;
}

int utf8_byte_offset_from_utf16_index(const std::wstring &text, int utf16_index) {
    size_t bytes = 0;
    int index = 0;
    while (index < utf16_index && index < static_cast<int>(text.size())) {
        int units_used = 1;
        wchar_t second = index + 1 < static_cast<int>(text.size()) ? text[static_cast<size_t>(index + 1)] : 0;
        bytes += utf8_bytes_for_codepoint(text[static_cast<size_t>(index)], second, &units_used);
        index += units_used;
    }
    return static_cast<int>(bytes);
}

int utf16_index_from_utf8_byte_offset(const std::wstring &text, int target_bytes) {
    size_t bytes = 0;
    int index = 0;
    while (index < static_cast<int>(text.size()) && static_cast<int>(bytes) < target_bytes) {
        int units_used = 1;
        wchar_t second = index + 1 < static_cast<int>(text.size()) ? text[static_cast<size_t>(index + 1)] : 0;
        size_t codepoint_bytes = utf8_bytes_for_codepoint(text[static_cast<size_t>(index)], second, &units_used);
        if (static_cast<int>(bytes + codepoint_bytes) > target_bytes) {
            break;
        }
        bytes += codepoint_bytes;
        index += units_used;
    }
    return index;
}

void set_preview_plain_text(AppState &app, const std::wstring &text, COLORREF text_color) {
    SetWindowTextW(app.preview, text.c_str());

    CHARFORMAT2W base{};
    base.cbSize = sizeof(base);
    base.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    base.crTextColor = text_color;
    base.yHeight = 18 * 20;
    wcscpy_s(base.szFaceName, L"Segoe UI Variable Text");
    SendMessageW(app.preview, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&base));
    SendMessageW(app.preview, EM_SETSEL, 0, 0);
}

void populate_sections(AppState &app, const MbDocumentResult &result) {
    app.sections.clear();
    SendMessageW(app.sections_list, LB_RESETCONTENT, 0, 0);

    for (size_t i = 0; i < result.section_count; ++i) {
        const MbSection &section = result.sections[i];
        SectionEntry entry;
        for (int indent = 1; indent < section.level; ++indent) {
            entry.label += L"  ";
        }
        std::wstring title = view_to_wstring(section.title);
        entry.label += title.empty() ? L"(untitled)" : title;
        entry.source_offset = section.source_offset;

        app.sections.push_back(entry);
        SendMessageW(app.sections_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(entry.label.c_str()));
    }
}

void refresh_from_backend(AppState &app) {
    clear_refresh_timer(app);

    std::wstring text = copy_edit_text(app.editor);
    if (!app.backend.available()) {
        set_preview_plain_text(app, app.backend.status_message, kMutedText);
        SendMessageW(app.sections_list, LB_RESETCONTENT, 0, 0);
        app.sections.clear();
        return;
    }

    std::string utf8 = narrow(text);
    MbDocumentResult result{};
    MbStatus status = app.backend.process_document(utf8.data(), utf8.size(), &result);
    if (status != MB_STATUS_OK) {
        set_preview_plain_text(app, L"Unable to render preview from the backend.", kMutedText);
        SendMessageW(app.sections_list, LB_RESETCONTENT, 0, 0);
        app.sections.clear();
        return;
    }

    populate_sections(app, result);
    render_preview_document(app, result);
    app.backend.free_document(&result);
}

void set_editor_text(AppState &app, const std::wstring &text, bool mark_dirty) {
    app.is_programmatic_change = true;
    SetWindowTextW(app.editor, text.c_str());
    app.is_programmatic_change = false;
    if (mark_dirty) {
        set_dirty(app, true);
    }
    schedule_refresh(app);
}

void set_current_path(AppState &app, const std::wstring *path) {
    if (path == nullptr) {
        app.current_path.clear();
        app.has_path = false;
    } else {
        app.current_path = *path;
        app.has_path = true;
    }
    set_window_title(app);
}

bool load_file_contents(const std::wstring &path, std::wstring &text) {
    FILE *file = _wfopen(path.c_str(), L"rb");
    if (file == nullptr) {
        return false;
    }
    std::vector<char> bytes;
    char buffer[4096];
    while (true) {
        size_t count = fread(buffer, 1, sizeof(buffer), file);
        if (count > 0) {
            bytes.insert(bytes.end(), buffer, buffer + count);
        }
        if (count < sizeof(buffer)) {
            break;
        }
    }
    fclose(file);
    text = widen(std::string_view(bytes.data(), bytes.size()));
    return true;
}

bool load_document_from_path(AppState &app, const std::wstring &path) {
    std::wstring text;
    if (!load_file_contents(path, text)) {
        show_message_box(app.window, L"Open Failed", L"Unable to open the selected file.", MB_OK | MB_ICONERROR);
        return false;
    }

    std::filesystem::path resolved = std::filesystem::absolute(std::filesystem::path(path));
    std::wstring resolved_path = resolved.wstring();
    set_editor_text(app, text, false);
    set_current_path(app, &resolved_path);
    set_dirty(app, false);
    return true;
}

bool save_file_contents(const std::wstring &path, const std::wstring &text) {
    std::string utf8 = narrow(text);
    FILE *file = _wfopen(path.c_str(), L"wb");
    if (file == nullptr) {
        return false;
    }
    size_t written = fwrite(utf8.data(), 1, utf8.size(), file);
    fclose(file);
    return written == utf8.size();
}

bool choose_path(HWND owner, bool saving, std::wstring &path) {
    path.clear();

    IFileDialog *dialog = nullptr;
    HRESULT hr = saving
        ? CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))
        : CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || dialog == nullptr) {
        return false;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(saving ? L"Save Markdown File" : L"Open Markdown File");

    COMDLG_FILTERSPEC filters[] = {
        {L"Markdown files", L"*.md;*.markdown"},
        {L"All files", L"*.*"},
    };
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
    dialog->SetFileTypeIndex(1);
    if (saving) {
        dialog->SetDefaultExtension(L"md");
        dialog->SetFileName(L"untitled.md");
    }

    hr = dialog->Show(owner);
    if (SUCCEEDED(hr)) {
        IShellItem *item = nullptr;
        hr = dialog->GetResult(&item);
        if (SUCCEEDED(hr) && item != nullptr) {
            PWSTR raw_path = nullptr;
            hr = item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path);
            if (SUCCEEDED(hr) && raw_path != nullptr) {
                path = raw_path;
                CoTaskMemFree(raw_path);
                item->Release();
                dialog->Release();
                return true;
            }
            item->Release();
        }
    }

    dialog->Release();
    return false;
}

bool save_document_as(AppState &app);

bool save_document(AppState &app) {
    if (!app.has_path) {
        return save_document_as(app);
    }
    std::wstring text = copy_edit_text(app.editor);
    if (!save_file_contents(app.current_path, text)) {
        show_message_box(app.window, L"Save Failed", L"Unable to save the current file.", MB_OK | MB_ICONERROR);
        return false;
    }
    set_dirty(app, false);
    return true;
}

bool save_document_as(AppState &app) {
    std::wstring path;
    if (!choose_path(app.window, true, path)) {
        return false;
    }
    std::wstring text = copy_edit_text(app.editor);
    if (!save_file_contents(path, text)) {
        show_message_box(app.window, L"Save Failed", L"Unable to save the selected file.", MB_OK | MB_ICONERROR);
        return false;
    }
    set_current_path(app, &path);
    set_dirty(app, false);
    return true;
}

void new_document(AppState &app) {
    if (!confirm_discard_changes(app)) {
        return;
    }
    set_editor_text(app, L"", false);
    set_current_path(app, nullptr);
    set_dirty(app, false);
}

void open_document(AppState &app) {
    if (!confirm_discard_changes(app)) {
        return;
    }

    std::wstring path;
    if (!choose_path(app.window, false, path)) {
        return;
    }

    load_document_from_path(app, path);
}

void jump_to_section(AppState &app, int index) {
    if (index < 0 || index >= static_cast<int>(app.sections.size())) {
        return;
    }
    std::wstring text = copy_edit_text(app.editor);
    int utf16_index = utf16_index_from_utf8_byte_offset(text, app.sections[static_cast<size_t>(index)].source_offset);
    SendMessageW(app.editor, EM_SETSEL, utf16_index, utf16_index);
    SendMessageW(app.editor, EM_SCROLLCARET, 0, 0);
    SetFocus(app.editor);
}

void apply_backend_edit(AppState &app, int32_t command) {
    if (!app.backend.available()) {
        show_message_box(app.window, L"Formatting Unavailable", app.backend.status_message, MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring text = copy_edit_text(app.editor);
    DWORD start = 0;
    DWORD end = 0;
    SendMessageW(app.editor, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));

    std::string utf8 = narrow(text);
    int32_t start_bytes = utf8_byte_offset_from_utf16_index(text, static_cast<int>(start));
    int32_t end_bytes = utf8_byte_offset_from_utf16_index(text, static_cast<int>(end));
    MbEditResult result{};
    MbStatus status = app.backend.apply_edit(utf8.data(), utf8.size(), start_bytes, end_bytes, command, &result);
    if (status != MB_STATUS_OK) {
        show_message_box(app.window, L"Formatting Failed", L"Unable to apply markdown formatting.", MB_OK | MB_ICONERROR);
        return;
    }

    std::wstring updated = widen(std::string_view(result.text.data != nullptr ? result.text.data : "", result.text.length));
    int32_t selection_start = result.selection_start;
    int32_t selection_end = result.selection_end;
    app.backend.free_edit(&result);

    app.is_programmatic_change = true;
    SetWindowTextW(app.editor, updated.c_str());
    app.is_programmatic_change = false;
    int new_start = utf16_index_from_utf8_byte_offset(updated, selection_start);
    int new_end = utf16_index_from_utf8_byte_offset(updated, selection_end);
    SendMessageW(app.editor, EM_SETSEL, new_start, new_end);
    SendMessageW(app.editor, EM_SCROLLCARET, 0, 0);
    SetFocus(app.editor);
    set_dirty(app, true);
    schedule_refresh(app);
}

void open_repository(AppState &app) {
    const std::wstring url = L"https://github.com/krisfur/markdown-buddy";
    HINSTANCE result = ShellExecuteW(app.window, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) > 32) {
        return;
    }

    if (is_running_under_wine()) {
        if (launch_unix_command_via_wine(L"Z:\\usr\\bin\\gio", L"open \"" + url + L"\"")) {
            return;
        }
        if (launch_unix_command_via_wine(L"Z:\\usr\\bin\\xdg-open", L"\"" + url + L"\"")) {
            return;
        }
    }

    std::wstring message = L"Unable to open the repository automatically.\n\n" + url;
    if (copy_text_to_clipboard(app.window, url)) {
        message += L"\n\nThe URL has been copied to the clipboard.";
    }
    show_message_box(app.window, L"Open Repository", message, MB_OK | MB_ICONINFORMATION);
}

void show_about(AppState &app) {
    show_message_box(
        app.window,
        L"About Markdown Buddy",
        L"Native markdown editor with a live preview and section list.\n\nBackend: Odin C ABI\nFrontend: Win32 + Zig C++"
    );
}

void update_menu_state(AppState &app) {
    UINT backend_state = app.backend.available() ? MF_ENABLED : MF_GRAYED;
    EnableMenuItem(app.menu, kCommandBold, MF_BYCOMMAND | backend_state);
    EnableMenuItem(app.menu, kCommandItalic, MF_BYCOMMAND | backend_state);
    DrawMenuBar(app.window);
}

void create_fonts(AppState &app) {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);

    LOGFONTW ui_font = metrics.lfMessageFont;
    wcscpy_s(ui_font.lfFaceName, L"Segoe UI Variable Text");
    ui_font.lfHeight = -16;
    app.fonts.ui = CreateFontIndirectW(&ui_font);

    LOGFONTW small_font = ui_font;
    small_font.lfHeight = -12;
    small_font.lfWeight = FW_BOLD;
    app.fonts.ui_small = CreateFontIndirectW(&small_font);

    LOGFONTW mono_font = ui_font;
    wcscpy_s(mono_font.lfFaceName, L"Consolas");
    mono_font.lfHeight = -16;
    mono_font.lfWeight = FW_NORMAL;
    app.fonts.mono = CreateFontIndirectW(&mono_font);
}

void create_brushes(AppState &app) {
    app.brushes.shell_top = CreateSolidBrush(kShellTop);
    app.brushes.shell_bottom = CreateSolidBrush(kShellBottom);
    app.brushes.sidebar_fill = CreateSolidBrush(kSidebarFill);
    app.brushes.panel_fill = CreateSolidBrush(kPanelFill);
    app.brushes.panel_header_fill = CreateSolidBrush(kPanelHeaderFill);
}

void destroy_resources(AppState &app) {
    DestroyAcceleratorTable(app.accelerators);
    DeleteObject(app.brushes.shell_top);
    DeleteObject(app.brushes.shell_bottom);
    DeleteObject(app.brushes.sidebar_fill);
    DeleteObject(app.brushes.panel_fill);
    DeleteObject(app.brushes.panel_header_fill);
    DeleteObject(app.fonts.ui);
    DeleteObject(app.fonts.ui_small);
    DeleteObject(app.fonts.mono);
    unload_backend(app.backend);
    if (app.rich_edit_module != nullptr) {
        FreeLibrary(app.rich_edit_module);
    }
}

RECT panel_rect_for_body(const RECT &outer) {
    RECT body = outer;
    body.top += kHeaderHeight;
    return body;
}

WindowLayout compute_layout(int width, int height) {
    WindowLayout layout{};
    layout.sidebar = {kOuterPadding, kOuterPadding, kOuterPadding + kSidebarWidth, height - kOuterPadding};

    RECT content = {layout.sidebar.right + kPanelGap, kOuterPadding, width - kOuterPadding, height - kOuterPadding};
    const int content_width = content.right - content.left;
    const int editor_width = (content_width - kPanelGap) / 2;

    layout.editor = {content.left, content.top, content.left + editor_width, content.bottom};
    layout.preview = {layout.editor.right + kPanelGap, content.top, content.right, content.bottom};
    return layout;
}

void move_controls(AppState &app, int width, int height) {
    const WindowLayout layout = compute_layout(width, height);

    MoveWindow(app.sections_header, layout.sidebar.left + 1, layout.sidebar.top + 1, layout.sidebar.right - layout.sidebar.left - 2, kHeaderHeight - 1, TRUE);
    RECT sidebar_body = panel_rect_for_body(layout.sidebar);
    MoveWindow(app.sections_list, sidebar_body.left + kInnerPadding, sidebar_body.top + kInnerPadding / 2, sidebar_body.right - sidebar_body.left - kInnerPadding * 2, sidebar_body.bottom - sidebar_body.top - kInnerPadding, TRUE);

    MoveWindow(app.editor_header, layout.editor.left + 1, layout.editor.top + 1, layout.editor.right - layout.editor.left - 2, kHeaderHeight - 1, TRUE);
    RECT editor_body = panel_rect_for_body(layout.editor);
    MoveWindow(app.editor, editor_body.left + kInnerPadding, editor_body.top + kInnerPadding / 2, editor_body.right - editor_body.left - kInnerPadding * 2, editor_body.bottom - editor_body.top - kInnerPadding, TRUE);

    MoveWindow(app.preview_header, layout.preview.left + 1, layout.preview.top + 1, layout.preview.right - layout.preview.left - 2, kHeaderHeight - 1, TRUE);
    RECT preview_body = panel_rect_for_body(layout.preview);
    MoveWindow(app.preview, preview_body.left + kInnerPadding, preview_body.top + kInnerPadding / 2, preview_body.right - preview_body.left - kInnerPadding * 2, preview_body.bottom - preview_body.top - kInnerPadding, TRUE);
}

void fill_vertical_gradient(HDC dc, const RECT &rect, COLORREF top, COLORREF bottom) {
    TRIVERTEX vertices[2] = {
        {rect.left, rect.top, static_cast<COLOR16>(GetRValue(top) << 8), static_cast<COLOR16>(GetGValue(top) << 8), static_cast<COLOR16>(GetBValue(top) << 8), 0xFF00},
        {rect.right, rect.bottom, static_cast<COLOR16>(GetRValue(bottom) << 8), static_cast<COLOR16>(GetGValue(bottom) << 8), static_cast<COLOR16>(GetBValue(bottom) << 8), 0xFF00},
    };
    GRADIENT_RECT gradient = {0, 1};
    GradientFill(dc, vertices, 2, &gradient, 1, GRADIENT_FILL_RECT_V);
}

void draw_panel(HDC dc, const RECT &rect, HBRUSH fill_brush) {
    FillRect(dc, &rect, fill_brush);
    HPEN pen = CreatePen(PS_SOLID, 1, kBorderColor);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
}

void paint_shell(AppState &app) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(app.window, &ps);
    RECT client{};
    GetClientRect(app.window, &client);
    fill_vertical_gradient(dc, client, kShellTop, kShellBottom);

    const WindowLayout layout = compute_layout(client.right, client.bottom);

    draw_panel(dc, layout.sidebar, app.brushes.sidebar_fill);
    RECT sidebar_header = layout.sidebar;
    sidebar_header.bottom = layout.sidebar.top + kHeaderHeight;
    FillRect(dc, &sidebar_header, app.brushes.panel_header_fill);

    draw_panel(dc, layout.editor, app.brushes.panel_fill);
    RECT editor_header = layout.editor;
    editor_header.bottom = layout.editor.top + kHeaderHeight;
    FillRect(dc, &editor_header, app.brushes.panel_header_fill);

    draw_panel(dc, layout.preview, app.brushes.panel_fill);
    RECT preview_header = layout.preview;
    preview_header.bottom = layout.preview.top + kHeaderHeight;
    FillRect(dc, &preview_header, app.brushes.panel_header_fill);
    EndPaint(app.window, &ps);
}

HACCEL create_accelerators() {
    ACCEL entries[] = {
        {FVIRTKEY | FCONTROL, 'N', static_cast<WORD>(kCommandNew)},
        {FVIRTKEY | FCONTROL, 'O', static_cast<WORD>(kCommandOpen)},
        {FVIRTKEY | FCONTROL, 'S', static_cast<WORD>(kCommandSave)},
        {FVIRTKEY | FCONTROL | FSHIFT, 'S', static_cast<WORD>(kCommandSaveAs)},
        {FVIRTKEY | FCONTROL, 'Q', static_cast<WORD>(kCommandQuit)},
        {FVIRTKEY | FCONTROL, 'Z', static_cast<WORD>(kCommandUndo)},
        {FVIRTKEY | FCONTROL | FSHIFT, 'Z', static_cast<WORD>(kCommandRedo)},
        {FVIRTKEY | FCONTROL, 'X', static_cast<WORD>(kCommandCut)},
        {FVIRTKEY | FCONTROL, 'C', static_cast<WORD>(kCommandCopy)},
        {FVIRTKEY | FCONTROL, 'V', static_cast<WORD>(kCommandPaste)},
        {FVIRTKEY | FCONTROL, 'A', static_cast<WORD>(kCommandSelectAll)},
        {FVIRTKEY | FCONTROL, 'B', static_cast<WORD>(kCommandBold)},
        {FVIRTKEY | FCONTROL, 'I', static_cast<WORD>(kCommandItalic)},
        {FVIRTKEY, VK_F1, static_cast<WORD>(kCommandRepository)},
    };
    return CreateAcceleratorTableW(entries, static_cast<int>(std::size(entries)));
}

HMENU create_app_menu() {
    HMENU root = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU edit = CreatePopupMenu();
    HMENU help = CreatePopupMenu();

    AppendMenuW(file, MF_STRING, kCommandNew, L"&New\tCtrl+N");
    AppendMenuW(file, MF_STRING, kCommandOpen, L"&Open...\tCtrl+O");
    AppendMenuW(file, MF_STRING, kCommandSave, L"&Save\tCtrl+S");
    AppendMenuW(file, MF_STRING, kCommandSaveAs, L"Save &As...\tCtrl+Shift+S");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kCommandQuit, L"&Quit\tCtrl+Q");

    AppendMenuW(edit, MF_STRING, kCommandUndo, L"&Undo\tCtrl+Z");
    AppendMenuW(edit, MF_STRING, kCommandRedo, L"&Redo\tCtrl+Shift+Z");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kCommandCut, L"Cu&t\tCtrl+X");
    AppendMenuW(edit, MF_STRING, kCommandCopy, L"&Copy\tCtrl+C");
    AppendMenuW(edit, MF_STRING, kCommandPaste, L"&Paste\tCtrl+V");
    AppendMenuW(edit, MF_STRING, kCommandSelectAll, L"Select &All\tCtrl+A");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kCommandBold, L"&Bold\tCtrl+B");
    AppendMenuW(edit, MF_STRING, kCommandItalic, L"&Italic\tCtrl+I");

    AppendMenuW(help, MF_STRING, kCommandAbout, L"&About Markdown Buddy");
    AppendMenuW(help, MF_STRING, kCommandRepository, L"&Repository\tF1");

    AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"&Edit");
    AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"&Help");
    return root;
}

HWND create_label(AppState &app, int id, const wchar_t *text) {
    HWND label = CreateWindowExW(
        0,
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE,
        0,
        0,
        0,
        0,
        app.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        app.instance,
        nullptr
    );
    SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(app.fonts.ui_small), TRUE);
    return label;
}

HWND create_controls(AppState &app) {
    app.sections_header = create_label(app, kIdSectionsHeader, L"SECTIONS");
    app.sections_list = CreateWindowExW(
        0,
        L"LISTBOX",
        nullptr,
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL,
        0,
        0,
        0,
        0,
        app.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSectionsList)),
        app.instance,
        nullptr
    );
    SendMessageW(app.sections_list, WM_SETFONT, reinterpret_cast<WPARAM>(app.fonts.ui), TRUE);

    app.editor_header = create_label(app, kIdEditorHeader, L"EDITOR");
    app.editor = CreateWindowExW(
        0,
        L"EDIT",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL | WS_VSCROLL,
        0,
        0,
        0,
        0,
        app.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEditor)),
        app.instance,
        nullptr
    );
    SendMessageW(app.editor, WM_SETFONT, reinterpret_cast<WPARAM>(app.fonts.mono), TRUE);
    SendMessageW(app.editor, EM_SETLIMITTEXT, 0, 0);

    app.preview_header = create_label(app, kIdPreviewHeader, L"PREVIEW");
    app.preview = CreateWindowExW(
        0,
        MSFTEDIT_CLASS,
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_NOHIDESEL | WS_VSCROLL,
        0,
        0,
        0,
        0,
        app.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPreview)),
        app.instance,
        nullptr
    );
    SendMessageW(app.preview, WM_SETFONT, reinterpret_cast<WPARAM>(app.fonts.ui), TRUE);
    SendMessageW(app.preview, EM_SETBKGNDCOLOR, 0, kPanelFill);
    return app.editor;
}

void perform_command(AppState &app, UINT command) {
    switch (command) {
    case kCommandNew:
        new_document(app);
        break;
    case kCommandOpen:
        open_document(app);
        break;
    case kCommandSave:
        save_document(app);
        break;
    case kCommandSaveAs:
        save_document_as(app);
        break;
    case kCommandQuit:
        SendMessageW(app.window, WM_CLOSE, 0, 0);
        break;
    case kCommandUndo:
        SendMessageW(app.editor, WM_UNDO, 0, 0);
        break;
    case kCommandRedo:
        break;
    case kCommandCut:
        SendMessageW(app.editor, WM_CUT, 0, 0);
        break;
    case kCommandCopy:
        SendMessageW(app.editor, WM_COPY, 0, 0);
        break;
    case kCommandPaste:
        SendMessageW(app.editor, WM_PASTE, 0, 0);
        break;
    case kCommandSelectAll:
        SendMessageW(app.editor, EM_SETSEL, 0, -1);
        SetFocus(app.editor);
        break;
    case kCommandBold:
        apply_backend_edit(app, MB_EDIT_BOLD);
        break;
    case kCommandItalic:
        apply_backend_edit(app, MB_EDIT_ITALIC);
        break;
    case kCommandAbout:
        show_about(app);
        break;
    case kCommandRepository:
        open_repository(app);
        break;
    default:
        break;
    }
}

LRESULT handle_ctl_color(AppState &app, HDC dc, HWND control) {
    SetBkMode(dc, TRANSPARENT);
    if (control == app.sections_header || control == app.editor_header || control == app.preview_header) {
        SetTextColor(dc, kHeaderText);
        return reinterpret_cast<LRESULT>(app.brushes.panel_header_fill);
    }
    if (control == app.sections_list) {
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, kSidebarFill);
        SetTextColor(dc, kBodyText);
        return reinterpret_cast<LRESULT>(app.brushes.sidebar_fill);
    }
    if (control == app.editor) {
        SetBkColor(dc, kPanelFill);
        SetTextColor(dc, kBodyText);
        return reinterpret_cast<LRESULT>(app.brushes.panel_fill);
    }
    if (control == app.preview) {
        SetBkColor(dc, kPanelFill);
        SetTextColor(dc, app.backend.available() ? kBodyText : kMutedText);
        return reinterpret_cast<LRESULT>(app.brushes.panel_fill);
    }
    return 0;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    if (message == WM_NCCREATE) {
        auto *create = reinterpret_cast<CREATESTRUCTW *>(l_param);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return DefWindowProcW(window, message, w_param, l_param);
    }

    auto *app_ptr = reinterpret_cast<AppState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (app_ptr == nullptr) {
        return DefWindowProcW(window, message, w_param, l_param);
    }
    AppState &app = *app_ptr;

    switch (message) {
    case WM_SIZE:
        move_controls(app, LOWORD(l_param), HIWORD(l_param));
        return 0;
    case WM_COMMAND: {
        UINT command = LOWORD(w_param);
        UINT notify = HIWORD(w_param);
        HWND source = reinterpret_cast<HWND>(l_param);
        if (source == app.editor && notify == EN_CHANGE && !app.is_programmatic_change) {
            set_dirty(app, true);
            schedule_refresh(app);
            return 0;
        }
        if (source == app.sections_list && notify == LBN_DBLCLK) {
            int index = static_cast<int>(SendMessageW(app.sections_list, LB_GETCURSEL, 0, 0));
            jump_to_section(app, index);
            return 0;
        }
        perform_command(app, command);
        return 0;
    }
    case WM_TIMER:
        if (w_param == kRefreshTimerId) {
            refresh_from_backend(app);
            return 0;
        }
        break;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC:
        return handle_ctl_color(app, reinterpret_cast<HDC>(w_param), reinterpret_cast<HWND>(l_param));
    case WM_PAINT:
        paint_shell(app);
        return 0;
    case WM_CLOSE:
        if (!confirm_discard_changes(app)) {
            return 0;
        }
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        clear_refresh_timer(app);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, w_param, l_param);
}

bool register_window_class(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = kWindowClassName;
    return RegisterClassExW(&wc) != 0;
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command) {
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!register_window_class(instance)) {
        return 1;
    }

    AppState app;
    app.instance = instance;
    app.rich_edit_module = LoadLibraryW(L"Msftedit.dll");
    if (app.rich_edit_module == nullptr) {
        CoUninitialize();
        return 1;
    }
    create_brushes(app);
    create_fonts(app);
    load_backend(app.backend);

    app.window = CreateWindowExW(
        0,
        kWindowClassName,
        L"Markdown Buddy",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1320,
        860,
        nullptr,
        nullptr,
        instance,
        &app
    );
    if (app.window == nullptr) {
        destroy_resources(app);
        CoUninitialize();
        return 1;
    }
    g_app = &app;

    app.menu = create_app_menu();
    SetMenu(app.window, app.menu);
    app.accelerators = create_accelerators();
    create_controls(app);
    set_current_path(app, nullptr);
    set_dirty(app, false);
    update_menu_state(app);
    set_preview_plain_text(app, app.backend.available() ? L"Start writing markdown in the editor." : app.backend.status_message, app.backend.available() ? kBodyText : kMutedText);

    ShowWindow(app.window, show_command);
    UpdateWindow(app.window);

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv != nullptr) {
        if (argc > 1) {
            load_document_from_path(app, argv[1]);
        }
        LocalFree(argv);
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (app.accelerators != nullptr && TranslateAcceleratorW(app.window, app.accelerators, &message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    destroy_resources(app);
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
