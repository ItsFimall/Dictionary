// Flipper Zero — English Dictionary App
// Step 2.6: Fix search by trimming trailing whitespace from user input.

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <ctype.h> // Required for the isspace() function

#define APP_NAME "Dictionary"
#define VERSION  "SearchFuncDBG"

// The path to our dictionary files on the SD card.
#define DICTIONARY_APP_ASSETS_PATH EXT_PATH("apps_assets/dictionary")
#define DICTIONARY_IDX_PATH        DICTIONARY_APP_ASSETS_PATH "/engdict.idx"
#define DICTIONARY_DAT_PATH        DICTIONARY_APP_ASSETS_PATH "/engdict.dat"

// An enumeration for the different views in our application.
typedef enum {
    DictionaryViewMainMenu = 0,
    DictionaryViewSearchInput,
    DictionaryViewResult,
} DictionaryViewId;

// An enumeration for the menu items.
typedef enum {
    DictionaryMenuSearch = 0,
    DictionaryMenuRandom,
    DictionaryMenuHistory,
    DictionaryMenuSettings,
    DictionaryMenuAbout,
} DictionaryMenuId;

// The main application structure.
typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* submenu;
    TextInput* text_input;
    TextBox* text_box;

    char* search_buffer;
    size_t search_buffer_size;

    FuriString* result_text;
} DictionaryApp;

// Forward declarations for our callback functions.
static void dictionary_search_done_cb(void* context);
static bool dictionary_navigation_event_callback(void* context);
static bool dictionary_app_search_word(DictionaryApp* app, const char* word_to_find);

// Main menu callback: handles actions when a menu item is selected.
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
        view_dispatcher_switch_to_view(app->vd, DictionaryViewSearchInput);
        break;
        // ... other cases remain the same for now
    }
}

// Callback for when text input is complete.
static void dictionary_search_done_cb(void* context) {
    DictionaryApp* app = context;

    // --- FIX: Trim trailing whitespace from the search buffer ---
    // This is the crucial fix. The TextInput view can sometimes add a space
    // at the end of the input, causing all search comparisons to fail.
    size_t len = strlen(app->search_buffer);
    while(len > 0 && isspace((unsigned char)app->search_buffer[len - 1])) {
        app->search_buffer[--len] = '\0';
    }
    // --- End of FIX ---

    FURI_LOG_I(APP_NAME, "Trimmed word to search: '%s'", app->search_buffer);

    // Perform the search with the cleaned string
    bool found = dictionary_app_search_word(app, app->search_buffer);

    // Prepare the result view
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);

    if(found) {
        FuriString* display_text = furi_string_alloc();
        furi_string_printf(
            display_text,
            "\e#%s\e#\n\n%s",
            app->search_buffer,
            furi_string_get_cstr(app->result_text));
        text_box_set_text(app->text_box, furi_string_get_cstr(display_text));
        furi_string_free(display_text);
    } else {
        text_box_set_text(app->text_box, furi_string_get_cstr(app->result_text));
    }

    // Switch to the result view
    view_dispatcher_switch_to_view(app->vd, DictionaryViewResult);
}

// A generic navigation event callback.
static bool dictionary_navigation_event_callback(void* context) {
    UNUSED(context);
    return false; // Let the view dispatcher handle the back button
}

// The core search logic.
static bool dictionary_app_search_word(DictionaryApp* app, const char* word_to_find) {
    bool found = false;
    furi_string_reset(app->result_text);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* idx_file = storage_file_alloc(storage);
    File* dat_file = storage_file_alloc(storage);

    if(!storage_file_open(idx_file, DICTIONARY_IDX_PATH, FSAM_READ, FSOM_OPEN_EXISTING) ||
       !storage_file_open(dat_file, DICTIONARY_DAT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(APP_NAME, "Failed to open dictionary files");
        furi_string_set(app->result_text, "Error: Dictionary files not found on SD card.");
        goto cleanup;
    }

    uint64_t file_size = storage_file_size(idx_file);
    uint64_t low = 0, high = file_size;

    while(low < high) {
        uint64_t mid = low + (high - low) / 2;
        storage_file_seek(idx_file, mid, true);

        // This complex block seeks backwards from the midpoint to find the start
        // of the previous record, ensuring we start our read from a valid boundary.
        uint16_t key_len_check;
        while(storage_file_tell(idx_file) > low) {
            uint64_t current_pos = storage_file_tell(idx_file);
            if(current_pos == 0) break;
            storage_file_seek(idx_file, current_pos - 1, true);

            if(storage_file_read(idx_file, &key_len_check, sizeof(key_len_check)) !=
               sizeof(key_len_check))
                break;

            if(storage_file_tell(idx_file) + key_len_check + 6 <= file_size) {
                uint64_t recheck_pos = storage_file_tell(idx_file);
                if(recheck_pos < 2) break;
                storage_file_seek(idx_file, recheck_pos - 2, true);
                break;
            }
        }

        uint64_t record_start_pos = storage_file_tell(idx_file);
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
               storage_file_read(idx_file, &length, sizeof(length)) != sizeof(length)) {
                break;
            }

            storage_file_seek(dat_file, offset, true);
            char* def_buffer = malloc(length + 1);
            storage_file_read(dat_file, def_buffer, length);
            def_buffer[length] = '\0';
            furi_string_set(app->result_text, def_buffer);
            free(def_buffer);

            found = true;
            goto cleanup;
        } else if(cmp < 0) {
            high = record_start_pos;
        } else {
            low = storage_file_tell(idx_file) + sizeof(uint32_t) + sizeof(uint16_t);
        }
    }

    if(!found) {
        furi_string_printf(app->result_text, "Word not found:\n\"%s\"", word_to_find);
    }

cleanup:
    storage_file_close(idx_file);
    storage_file_free(idx_file);
    storage_file_close(dat_file);
    storage_file_free(dat_file);
    furi_record_close(RECORD_STORAGE);
    return found;
}

// Allocate and initialize the application.
static DictionaryApp* dictionary_app_alloc(void) {
    DictionaryApp* app = malloc(sizeof(DictionaryApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();

    view_dispatcher_set_navigation_event_callback(app->vd, dictionary_navigation_event_callback);
    view_dispatcher_set_event_callback_context(app->vd, app);

    // --- Submenu (Main Menu) Setup ---
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "EngDict " VERSION);
    submenu_add_item(app->submenu, "Search", DictionaryMenuSearch, dictionary_menu_cb, app);
    // ... other items
    view_dispatcher_add_view(app->vd, DictionaryViewMainMenu, submenu_get_view(app->submenu));

    // --- TextInput (Search Input) Setup ---
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->vd, DictionaryViewSearchInput, text_input_get_view(app->text_input));
    app->search_buffer_size = 50;
    app->search_buffer = malloc(app->search_buffer_size);

    // --- TextBox (Result Display) Setup ---
    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->vd, DictionaryViewResult, text_box_get_view(app->text_box));
    app->result_text = furi_string_alloc();

    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

// Free all allocated resources.
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

// Application entry point.
int32_t dictionary_app_main(void* p) {
    UNUSED(p);
    DictionaryApp* app = dictionary_app_alloc();
    view_dispatcher_switch_to_view(app->vd, DictionaryViewMainMenu);
    view_dispatcher_run(app->vd);
    dictionary_app_free(app);
    return 0;
}
