// Flipper Zero — English Dictionary App (with Random feature)
// Author: Lynnet (+ Random by ChatGPT)

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/buffered_file_stream.h>

#define APP_NAME "Dictionary"
#define VERSION  "V1"

#define DICTIONARY_APP_ASSETS_PATH EXT_PATH("apps_assets/dictionary")
#define DICTIONARY_IDX_PATH        DICTIONARY_APP_ASSETS_PATH "/engdict.idx"
#define DICTIONARY_DAT_PATH        DICTIONARY_APP_ASSETS_PATH "/engdict.dat"
#define DICTIONARY_HISTORY_PATH    DICTIONARY_APP_ASSETS_PATH "/history.txt" // History file path
#define MAX_HISTORY_ITEMS          10
#define MAX_WORD_LENGTH            50

// --- Enums for Views and Menu Items ---
typedef enum {
    DictionaryViewMainMenu = 0,
    DictionaryViewSearchInput,
    DictionaryViewResult,
    DictionaryViewHistory, // View for history
    DictionaryViewAbout, // View for About page
} DictionaryViewId;

typedef enum {
    DictionaryMenuSearch = 0,
    DictionaryMenuHistory,
    DictionaryMenuRandom,
    DictionaryMenuSettings, // (reserved)
    DictionaryMenuAbout, // About menu item
} DictionaryMenuId;

// --- Application State Structure ---
typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* submenu;
    TextInput* text_input;
    TextBox* text_box;
    Submenu* history_submenu; // Submenu for history view
    TextBox* about_box; // TextBox for the About page

    char* search_buffer;
    size_t search_buffer_size;

    FuriString* result_text;

    uint32_t current_view;

    // History data
    char history_words[MAX_HISTORY_ITEMS][MAX_WORD_LENGTH];
    uint8_t history_count;
} DictionaryApp;

// --- Forward Declarations ---
static void dictionary_search_done_cb(void* context);
static bool dictionary_navigation_event_callback(void* context);
static bool dictionary_app_search_word(DictionaryApp* app, const char* word_to_find);
static void dictionary_app_save_history(DictionaryApp* app);
static void dictionary_history_menu_cb(void* context, uint32_t index);
static void dictionary_app_add_to_history(DictionaryApp* app, const char* word);
// NEW: random-word picker
static bool dictionary_app_random_word(DictionaryApp* app);

// Helper: split phonetic and defs, then format output
static void format_result_with_phonetic(FuriString* out, const char* word, const char* raw) {
    furi_string_reset(out);
    if(!raw) return;

    const char* defs = raw;
    if(raw[0] == '[') {
        const char* closing = strchr(raw, ']');
        if(closing) {
            size_t phon_len = closing - raw + 1;
            furi_string_cat_printf(out, "%s %.*s", word, (int)phon_len, raw);
            furi_string_push_back(out, '\n');
            defs = closing + 1;
            while(*defs && isspace((unsigned char)*defs))
                defs++;
        }
    } else {
        furi_string_cat_printf(out, "%s", word);
        furi_string_push_back(out, '\n');
    }

    size_t raw_len = strlen(defs);
    char* buf = malloc(raw_len + 1);
    if(!buf) {
        furi_string_cat(out, defs);
        return;
    }
    memcpy(buf, defs, raw_len + 1);

    unsigned count = 0;
    char* token = strtok(buf, ";");
    while(token) {
        while(*token && isspace((unsigned char)*token))
            token++;
        size_t tlen = strlen(token);
        while(tlen > 0 && isspace((unsigned char)token[tlen - 1])) {
            token[--tlen] = '\0';
        }
        if(tlen > 0) {
            furi_string_cat_printf(out, "%u. %s\n", ++count, token);
        }
        token = strtok(NULL, ";");
    }

    if(count == 0) {
        furi_string_cat(out, defs);
    }

    free(buf);
}

// --- History Management Functions ---

static void dictionary_app_load_history(DictionaryApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* file_stream = buffered_file_stream_alloc(storage);
    app->history_count = 0;

    if(buffered_file_stream_open(
           file_stream, DICTIONARY_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        while(app->history_count < MAX_HISTORY_ITEMS && stream_read_line(file_stream, line)) {
            furi_string_trim(line);
            if(furi_string_size(line) > 0) {
                strncpy(
                    app->history_words[app->history_count],
                    furi_string_get_cstr(line),
                    MAX_WORD_LENGTH - 1);
                app->history_words[app->history_count][MAX_WORD_LENGTH - 1] = '\0';
                app->history_count++;
            }
        }
        furi_string_free(line);
    }

    buffered_file_stream_close(file_stream);
    stream_free(file_stream);
    furi_record_close(RECORD_STORAGE);
}

static void dictionary_app_save_history(DictionaryApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* file_stream = buffered_file_stream_alloc(storage);

    if(buffered_file_stream_open(
           file_stream, DICTIONARY_HISTORY_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(uint8_t i = 0; i < app->history_count; ++i) {
            stream_write_format(file_stream, "%s\n", app->history_words[i]);
        }
    }

    buffered_file_stream_close(file_stream);
    stream_free(file_stream);
    furi_record_close(RECORD_STORAGE);
}

static void dictionary_app_add_to_history(DictionaryApp* app, const char* word) {
    if(!word || strlen(word) == 0) return;

    // Check for duplicates and remove if found
    int found_index = -1;
    for(uint8_t i = 0; i < app->history_count; ++i) {
        if(strcasecmp(app->history_words[i], word) == 0) {
            found_index = i;
            break;
        }
    }

    if(found_index != -1) {
        // If found, move it to the top (most recent)
        char temp[MAX_WORD_LENGTH];
        strncpy(temp, app->history_words[found_index], MAX_WORD_LENGTH);
        for(int i = found_index; i > 0; --i) {
            strncpy(app->history_words[i], app->history_words[i - 1], MAX_WORD_LENGTH);
        }
        strncpy(app->history_words[0], temp, MAX_WORD_LENGTH);
    } else {
        // If not found, add it to the top
        if(app->history_count < MAX_HISTORY_ITEMS) {
            app->history_count++;
        }
        // Shift existing items down
        for(int i = app->history_count - 1; i > 0; --i) {
            strncpy(app->history_words[i], app->history_words[i - 1], MAX_WORD_LENGTH);
        }
        // Add the new word at the top
        strncpy(app->history_words[0], word, MAX_WORD_LENGTH - 1);
        app->history_words[0][MAX_WORD_LENGTH - 1] = '\0';
    }

    dictionary_app_save_history(app);
}

// --- UI Callback Functions ---

static void dictionary_menu_cb(void* context, uint32_t index) {
    DictionaryApp* app = context;
    switch(index) {
    case DictionaryMenuSearch:
        memset(app->search_buffer, 0, app->search_buffer_size);
        text_input_set_header_text(app->text_input, "Search for a word");
        text_input_set_result_callback(
            app->text_input,
            dictionary_search_done_cb,
            app,
            app->search_buffer,
            app->search_buffer_size,
            true);
        app->current_view = DictionaryViewSearchInput;
        view_dispatcher_switch_to_view(app->vd, DictionaryViewSearchInput);
        break;

    case DictionaryMenuHistory:
        submenu_reset(app->history_submenu);
        submenu_set_header(app->history_submenu, "Search History");
        for(uint8_t i = 0; i < app->history_count; ++i) {
            submenu_add_item(
                app->history_submenu, app->history_words[i], i, dictionary_history_menu_cb, app);
        }
        app->current_view = DictionaryViewHistory;
        view_dispatcher_switch_to_view(app->vd, DictionaryViewHistory);
        break;

    case DictionaryMenuRandom: {
        bool found = dictionary_app_random_word(app);

        text_box_reset(app->text_box);
        text_box_set_font(app->text_box, TextBoxFontText);

        if(found) {
            text_box_set_text(app->text_box, furi_string_get_cstr(app->result_text));
            // 从首行提取真实单词（避免把 [phonetic] 带进历史）
            const char* cstr = furi_string_get_cstr(app->result_text);
            char word[MAX_WORD_LENGTH] = {0};
            size_t i = 0;
            while(cstr[i] && cstr[i] != '\n' && cstr[i] != ' ' && i < MAX_WORD_LENGTH - 1) {
                word[i] = cstr[i];
                i++;
            }
            dictionary_app_add_to_history(app, word);
        } else {
            furi_string_set(app->result_text, "Error: Failed to pick a random word.");
            text_box_set_text(app->text_box, furi_string_get_cstr(app->result_text));
        }

        app->current_view = DictionaryViewResult;
        view_dispatcher_switch_to_view(app->vd, DictionaryViewResult);
    } break;

    case DictionaryMenuAbout:
        text_box_reset(app->about_box);
        text_box_set_font(app->about_box, TextBoxFontText);
        text_box_set_text(
            app->about_box,
            "Author - Lynnet\n\nDictionary File:\n - google-10000-english\n - WordNet\n - CMUdict");
        app->current_view = DictionaryViewAbout;
        view_dispatcher_switch_to_view(app->vd, DictionaryViewAbout);
        break;
    }
}

static void dictionary_history_menu_cb(void* context, uint32_t index) {
    DictionaryApp* app = context;
    if(index < app->history_count) {
        // Perform search with the selected history word
        bool found = dictionary_app_search_word(app, app->history_words[index]);
        text_box_reset(app->text_box);
        text_box_set_font(app->text_box, TextBoxFontText);

        if(found) {
            text_box_set_text(app->text_box, furi_string_get_cstr(app->result_text));
            // Add to history again to move it to the top
            dictionary_app_add_to_history(app, app->history_words[index]);
        } else {
            furi_string_printf(
                app->result_text, "Word not found:\n\"%s\"", app->history_words[index]);
            text_box_set_text(app->text_box, furi_string_get_cstr(app->result_text));
        }

        app->current_view = DictionaryViewResult;
        view_dispatcher_switch_to_view(app->vd, DictionaryViewResult);
    }
}

static void dictionary_search_done_cb(void* context) {
    DictionaryApp* app = context;
    size_t len = strlen(app->search_buffer);
    while(len > 0 && isspace((unsigned char)app->search_buffer[len - 1])) {
        app->search_buffer[--len] = '\0';
    }

    bool found = dictionary_app_search_word(app, app->search_buffer);
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);

    if(found) {
        text_box_set_text(app->text_box, furi_string_get_cstr(app->result_text));
        // Add successful search to history
        dictionary_app_add_to_history(app, app->search_buffer);
    } else {
        furi_string_printf(app->result_text, "Word not found:\n\"%s\"", app->search_buffer);
        text_box_set_text(app->text_box, furi_string_get_cstr(app->result_text));
    }

    app->current_view = DictionaryViewResult;
    view_dispatcher_switch_to_view(app->vd, DictionaryViewResult);
}

// Back button handling: go back to previous view instead of exiting
static bool dictionary_navigation_event_callback(void* context) {
    DictionaryApp* app = context;
    if(app->current_view == DictionaryViewSearchInput ||
       app->current_view == DictionaryViewResult || app->current_view == DictionaryViewHistory ||
       app->current_view == DictionaryViewAbout) {
        app->current_view = DictionaryViewMainMenu;
        view_dispatcher_switch_to_view(app->vd, DictionaryViewMainMenu);
        return true;
    }
    return false;
}

// --- Core Search Logic ---
static bool dictionary_app_search_word(DictionaryApp* app, const char* word_to_find) {
    bool found = false;
    furi_string_reset(app->result_text);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* idx_file = storage_file_alloc(storage);
    File* dat_file = storage_file_alloc(storage);

    if(!storage_file_open(idx_file, DICTIONARY_IDX_PATH, FSAM_READ, FSOM_OPEN_EXISTING) ||
       !storage_file_open(dat_file, DICTIONARY_DAT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        furi_string_set(app->result_text, "Error: Dictionary files not found on SD card.");
        goto cleanup;
    }

    uint64_t file_size = storage_file_size(idx_file);
    uint64_t low = 0, high = file_size;

    while(low < high) {
        uint64_t mid = low + (high - low) / 2;
        storage_file_seek(idx_file, low, true);
        uint64_t pivot_pos = low;

        while(storage_file_tell(idx_file) < mid) {
            pivot_pos = storage_file_tell(idx_file);
            uint16_t temp_len;
            if(storage_file_read(idx_file, &temp_len, sizeof(temp_len)) != sizeof(temp_len)) {
                goto search_failed;
            }
            if(!storage_file_seek(idx_file, temp_len + 6, false)) {
                goto search_failed;
            }
        }
        storage_file_seek(idx_file, pivot_pos, true);

        uint16_t key_len;
        if(storage_file_read(idx_file, &key_len, sizeof(key_len)) != sizeof(key_len)) break;

        char key_buffer[MAX_WORD_LENGTH];
        if(key_len >= sizeof(key_buffer)) key_len = sizeof(key_buffer) - 1;
        if(storage_file_read(idx_file, key_buffer, key_len) != key_len) break;
        key_buffer[key_len] = '\0';

        int cmp = strcasecmp(word_to_find, key_buffer);
        if(cmp == 0) {
            uint32_t offset;
            uint16_t length;
            if(storage_file_read(idx_file, &offset, sizeof(offset)) != sizeof(offset) ||
               storage_file_read(idx_file, &length, sizeof(length)) != sizeof(length))
                break;

            storage_file_seek(dat_file, offset, true);
            char* def_buffer = malloc(length + 1);
            if(!def_buffer) {
                furi_string_set(
                    app->result_text, "Error: Out of memory while reading definition.");
                goto cleanup;
            }
            if(storage_file_read(dat_file, def_buffer, length) != length) {
                free(def_buffer);
                furi_string_set(app->result_text, "Error: Failed to read definition data.");
                goto cleanup;
            }
            def_buffer[length] = '\0';

            format_result_with_phonetic(app->result_text, key_buffer, def_buffer);
            free(def_buffer);
            found = true;
            goto cleanup;
        } else if(cmp < 0) {
            high = pivot_pos;
        } else {
            low = storage_file_tell(idx_file) + sizeof(uint32_t) + sizeof(uint16_t);
        }
    }

search_failed:
    if(!found) {
        // leave result text empty, will be set by caller
    }

cleanup:
    storage_file_close(idx_file);
    storage_file_free(idx_file);
    storage_file_close(dat_file);
    storage_file_free(dat_file);
    furi_record_close(RECORD_STORAGE);
    return found;
}

// --- Random Word Picker ---
static bool dictionary_app_random_word(DictionaryApp* app) {
    bool ok = false;
    furi_string_reset(app->result_text);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* idx_file = storage_file_alloc(storage);
    File* dat_file = storage_file_alloc(storage);

    if(!storage_file_open(idx_file, DICTIONARY_IDX_PATH, FSAM_READ, FSOM_OPEN_EXISTING) ||
       !storage_file_open(dat_file, DICTIONARY_DAT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        furi_string_set(app->result_text, "Error: Dictionary files not found on SD card.");
        goto cleanup;
    }

    // First pass: count records
    uint32_t count = 0;
    storage_file_seek(idx_file, 0, true);
    while(true) {
        uint16_t key_len;
        if(storage_file_read(idx_file, &key_len, sizeof(key_len)) != sizeof(key_len)) break;
        if(!storage_file_seek(idx_file, (uint32_t)key_len + 6, false)) break; // skip key + 4 + 2
        count++;
    }

    if(count == 0) {
        furi_string_set(app->result_text, "Error: Dictionary index is empty.");
        goto cleanup;
    }

    // Generate pseudo-random number (seeded by tick)
    uint32_t seed = furi_get_tick();
    srand((unsigned int)(seed ^ (seed << 13) ^ (seed >> 7)));
    uint32_t r = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
    uint32_t target = r % count;

    // Second pass: seek to target
    storage_file_seek(idx_file, 0, true);
    for(uint32_t i = 0; i < target; i++) {
        uint16_t key_len_skip;
        if(storage_file_read(idx_file, &key_len_skip, sizeof(key_len_skip)) !=
               sizeof(key_len_skip) ||
           !storage_file_seek(idx_file, (uint32_t)key_len_skip + 6, false)) {
            furi_string_set(app->result_text, "Error: Random seek failed.");
            goto cleanup;
        }
    }

    // Read the target record
    uint16_t key_len;
    if(storage_file_read(idx_file, &key_len, sizeof(key_len)) != sizeof(key_len)) {
        furi_string_set(app->result_text, "Error: Read key length failed.");
        goto cleanup;
    }

    char key_buffer[MAX_WORD_LENGTH];
    if(key_len >= sizeof(key_buffer)) key_len = sizeof(key_buffer) - 1;
    if(storage_file_read(idx_file, key_buffer, key_len) != key_len) {
        furi_string_set(app->result_text, "Error: Read key failed.");
        goto cleanup;
    }
    key_buffer[key_len] = '\0';

    uint32_t offset;
    uint16_t length;
    if(storage_file_read(idx_file, &offset, sizeof(offset)) != sizeof(offset) ||
       storage_file_read(idx_file, &length, sizeof(length)) != sizeof(length)) {
        furi_string_set(app->result_text, "Error: Read pointer failed.");
        goto cleanup;
    }

    // Fetch definition from .dat
    storage_file_seek(dat_file, offset, true);
    char* def_buffer = malloc((size_t)length + 1);
    if(!def_buffer) {
        furi_string_set(app->result_text, "Error: Out of memory.");
        goto cleanup;
    }
    if(storage_file_read(dat_file, def_buffer, length) != length) {
        free(def_buffer);
        furi_string_set(app->result_text, "Error: Read definition failed.");
        goto cleanup;
    }
    def_buffer[length] = '\0';

    // Format nicely
    format_result_with_phonetic(app->result_text, key_buffer, def_buffer);
    free(def_buffer);
    ok = true;

cleanup:
    storage_file_close(idx_file);
    storage_file_free(idx_file);
    storage_file_close(dat_file);
    storage_file_free(dat_file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// --- App Allocation and Freeing ---

static DictionaryApp* dictionary_app_alloc(void) {
    DictionaryApp* app = malloc(sizeof(DictionaryApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    view_dispatcher_set_navigation_event_callback(app->vd, dictionary_navigation_event_callback);
    view_dispatcher_set_event_callback_context(app->vd, app);

    // Main Menu
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "EngDict " VERSION);
    submenu_add_item(app->submenu, "Search", DictionaryMenuSearch, dictionary_menu_cb, app);
    submenu_add_item(app->submenu, "History", DictionaryMenuHistory, dictionary_menu_cb, app);
    // Random item
    submenu_add_item(app->submenu, "Random", DictionaryMenuRandom, dictionary_menu_cb, app);
    // About item
    submenu_add_item(app->submenu, "About", DictionaryMenuAbout, dictionary_menu_cb, app);
    view_dispatcher_add_view(app->vd, DictionaryViewMainMenu, submenu_get_view(app->submenu));

    // Search Input View
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->vd, DictionaryViewSearchInput, text_input_get_view(app->text_input));
    app->search_buffer_size = MAX_WORD_LENGTH;
    app->search_buffer = malloc(app->search_buffer_size);

    // Result View
    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->vd, DictionaryViewResult, text_box_get_view(app->text_box));

    // History View
    app->history_submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->vd, DictionaryViewHistory, submenu_get_view(app->history_submenu));

    // About View
    app->about_box = text_box_alloc();
    view_dispatcher_add_view(app->vd, DictionaryViewAbout, text_box_get_view(app->about_box));

    app->result_text = furi_string_alloc();
    app->current_view = DictionaryViewMainMenu;

    // Load history from file
    dictionary_app_load_history(app);

    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void dictionary_app_free(DictionaryApp* app) {
    if(!app) return;
    view_dispatcher_remove_view(app->vd, DictionaryViewAbout);
    text_box_free(app->about_box);
    view_dispatcher_remove_view(app->vd, DictionaryViewHistory);
    submenu_free(app->history_submenu);
    view_dispatcher_remove_view(app->vd, DictionaryViewResult);
    text_box_free(app->text_box);
    furi_string_free(app->result_text);
    view_dispatcher_remove_view(app->vd, DictionaryViewSearchInput);
    text_input_free(app->text_input);
    free(app->search_buffer);
    view_dispatcher_remove_view(app->vd, DictionaryViewMainMenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);
    free(app);
}

// --- Main Application Entry Point ---
int32_t dictionary_app_main(void* p) {
    UNUSED(p);
    DictionaryApp* app = dictionary_app_alloc();
    view_dispatcher_switch_to_view(app->vd, DictionaryViewMainMenu);
    view_dispatcher_run(app->vd);
    dictionary_app_free(app);
    return 0;
}
