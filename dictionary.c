// Flipper Zero — English Dictionary App
// Step 6.5: Back navigation using simple state tracking (no unavailable API).

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

#define APP_NAME "Dictionary"
#define VERSION  "SearchPlus"

#define DICTIONARY_APP_ASSETS_PATH EXT_PATH("apps_assets/dictionary")
#define DICTIONARY_IDX_PATH        DICTIONARY_APP_ASSETS_PATH "/engdict.idx"
#define DICTIONARY_DAT_PATH        DICTIONARY_APP_ASSETS_PATH "/engdict.dat"

typedef enum {
    DictionaryViewMainMenu = 0,
    DictionaryViewSearchInput,
    DictionaryViewResult,
} DictionaryViewId;

typedef enum {
    DictionaryMenuSearch = 0,
    DictionaryMenuRandom,
    DictionaryMenuHistory,
    DictionaryMenuSettings,
    DictionaryMenuAbout,
} DictionaryMenuId;

typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* submenu;
    TextInput* text_input;
    TextBox* text_box;

    char* search_buffer;
    size_t search_buffer_size;

    FuriString* result_text;

    uint32_t current_view;
} DictionaryApp;

static void dictionary_search_done_cb(void* context);
static bool dictionary_navigation_event_callback(void* context);
static bool dictionary_app_search_word(DictionaryApp* app, const char* word_to_find);

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
       app->current_view == DictionaryViewResult) {
        app->current_view = DictionaryViewMainMenu;
        view_dispatcher_switch_to_view(app->vd, DictionaryViewMainMenu);
        return true;
    }
    return false;
}

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

        char key_buffer[50];
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

static DictionaryApp* dictionary_app_alloc(void) {
    DictionaryApp* app = malloc(sizeof(DictionaryApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    view_dispatcher_set_navigation_event_callback(app->vd, dictionary_navigation_event_callback);
    view_dispatcher_set_event_callback_context(app->vd, app);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "EngDict " VERSION);
    submenu_add_item(app->submenu, "Search", DictionaryMenuSearch, dictionary_menu_cb, app);
    view_dispatcher_add_view(app->vd, DictionaryViewMainMenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->vd, DictionaryViewSearchInput, text_input_get_view(app->text_input));
    app->search_buffer_size = 50;
    app->search_buffer = malloc(app->search_buffer_size);

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->vd, DictionaryViewResult, text_box_get_view(app->text_box));

    app->result_text = furi_string_alloc();
    app->current_view = DictionaryViewMainMenu;

    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void dictionary_app_free(DictionaryApp* app) {
    if(!app) return;
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

int32_t dictionary_app_main(void* p) {
    UNUSED(p);
    DictionaryApp* app = dictionary_app_alloc();
    view_dispatcher_switch_to_view(app->vd, DictionaryViewMainMenu);
    view_dispatcher_run(app->vd);
    dictionary_app_free(app);
    return 0;
}
