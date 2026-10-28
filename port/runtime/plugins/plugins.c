/* The mods' native plugins (plugins.h). */
#include "plugins.h"
#include "interfaces.h"

#include "../scripts/folders.h"
#include "../scripts/manifest.h"

#include <kh/kh_mod.h>

#include <SDL3/SDL.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef KHDAYS_VERSION
#define KHDAYS_VERSION "unknown"
#endif

/* One plugin: what the port knows of it (the view), and once loaded its library, descriptor and host table. The
 * KhPlugin handle a plugin is given is this record. */
struct KhPlugin {
    KhdaysPluginView view;
    char *folder, *mod_id, *name, *version, *author, *library, *why;
    char *path;     /* its library on disk */
    char *requires; /* mod.ini's `requires`, NULL none */
    SDL_SharedObject *object;
    const KhPluginDescriptor *descriptor;
    KhHost host;
    int started_order; /* its place among the started (the last started stops first) */
};
typedef struct KhPlugin Plugin;

static Plugin **plugins;
static int plugin_count, plugin_room, started_count;
static int native_code, safe_mode;
static KhdaysPluginsLog say;
static SDL_AtomicInt stop_asked;
static SDL_Semaphore *stopped; /* signalled once the game's thread has stopped them for khdays_plugins_quit */

const char *khdays_plugins_platform(void)
{
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    return "windows-x64";
#elif defined(__linux__) && defined(__x86_64__)
    return "linux-x64";
#elif defined(__linux__) && defined(__aarch64__)
    return "linux-arm64";
#else
    return "unknown";
#endif
}

void khdays_plugins_set_native_code(int on)
{
    native_code = on != 0;
}

void khdays_plugins_set_safe_mode(int on)
{
    safe_mode = on != 0;
}

static char *copy(const char *s)
{
    const size_t n = strlen(s) + 1;
    char *c = malloc(n);
    if (c != NULL) {
        memcpy(c, s, n);
    }
    return c;
}

static char *joined(const char *a, const char *b)
{
    const size_t n = strlen(a) + 1 + strlen(b) + 1;
    char *s = malloc(n);
    if (s != NULL) {
        snprintf(s, n, "%s/%s", a, b);
    }
    return s;
}

static void tell(const Plugin *p, const char *format, ...)
{
    char text[600];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (say != NULL) {
        say(p->mod_id, text);
    }
}

/* A path inside the mod's folder: relative, no "..", no drive, no backslash. */
static int inside(const char *path)
{
    if (*path == '\0' || *path == '/' || strchr(path, ':') != NULL || strchr(path, '\\') != NULL) {
        return 0;
    }
    for (const char *p = path; *p; ++p) {
        if (p[0] == '.' && p[1] == '.' && (p == path || p[-1] == '/') && (p[2] == '/' || p[2] == '\0')) {
            return 0;
        }
    }
    return 1;
}

static void set_why(Plugin *p, int state, const char *why)
{
    free(p->why);
    p->why = copy(why);
    p->view.state = state;
    p->view.why = p->why != NULL ? p->why : "";
}

static void host_log(KhPlugin *plugin, KhLogLevel level, const char *text)
{
    static const char *const levels[] = {"", "warning: ", "error: "};
    if (plugin == NULL || text == NULL) {
        return;
    }
    tell(plugin, "plugin %s%s", levels[level >= KH_LOG_INFO && level <= KH_LOG_ERROR ? level : KH_LOG_INFO], text);
}

static const void *host_find(KhPlugin *plugin, const char *name, uint32_t version)
{
    return khdays_plugins_interface(plugin, name, version);
}

void khdays_plugins_tell(KhPlugin *plugin, const char *text)
{
    tell(plugin, "%s", text);
}

const char *khdays_plugins_mod_id(const KhPlugin *plugin)
{
    return plugin != NULL && plugin->mod_id != NULL ? plugin->mod_id : "";
}

/* A mod folder: its plugin when its mod.ini names its SDK. */
static void found(const char *folder_name, void *context)
{
    const char *mods = context;
    char *folder = joined(mods, folder_name), *ini = folder != NULL ? joined(folder, "mod.ini") : NULL, *text = NULL;
    char key[64], *sdk = NULL, *library = NULL;
    size_t size = 0;
    Plugin *p = NULL;
    if (ini == NULL || (text = khdays_folders_read(ini, &size)) == NULL ||
        (sdk = khdays_manifest_value(text, size, "sdk")) == NULL) {
        goto done; /* no mod.ini, or no plugin */
    }
    if (plugin_count == plugin_room) {
        const int room = plugin_room > 0 ? plugin_room * 2 : 8;
        Plugin **grown = realloc(plugins, (size_t)room * sizeof(*grown));
        if (grown == NULL) {
            goto done;
        }
        plugins = grown;
        plugin_room = room;
    }
    p = calloc(1, sizeof(*p));
    if (p == NULL) {
        goto done;
    }
    plugins[plugin_count++] = p;
    {
        char *id = khdays_manifest_value(text, size, "id"), *name = khdays_manifest_value(text, size, "name");
        char *version = khdays_manifest_value(text, size, "version");
        char *author = khdays_manifest_value(text, size, "author");
        p->folder = copy(folder_name);
        p->mod_id = id != NULL ? id : copy(folder_name);
        p->name = name != NULL ? name : copy(folder_name);
        p->version = version != NULL ? version : copy("");
        p->author = author != NULL ? author : copy("");
    }
    snprintf(key, sizeof(key), "native.%s", khdays_plugins_platform());
    library = khdays_manifest_value(text, size, key);
    p->library = copy(library != NULL ? library : "");
    p->requires = khdays_manifest_value(text, size, "requires");
    p->view.folder = p->folder != NULL ? p->folder : "";
    p->view.mod_id = p->mod_id != NULL ? p->mod_id : "";
    p->view.name = p->name != NULL ? p->name : "";
    p->view.version = p->version != NULL ? p->version : "";
    p->view.author = p->author != NULL ? p->author : "";
    p->view.library = p->library != NULL ? p->library : "";
    set_why(p, KHDAYS_PLUGIN_WAITING, "");
    {
        char *end;
        const long major = strtol(sdk, &end, 10);
        char why[200];
        if (end == sdk || *end != '\0') {
            snprintf(why, sizeof(why), "mod.ini: sdk = %.40s is not a version", sdk);
            set_why(p, KHDAYS_PLUGIN_REFUSED, why);
        } else if (major != KH_ABI_MAJOR) {
            snprintf(why, sizeof(why), "built for SDK %ld, this port loads SDK %d", major, KH_ABI_MAJOR);
            set_why(p, KHDAYS_PLUGIN_REFUSED, why);
        } else if (library == NULL) {
            snprintf(why, sizeof(why), "no library for %s (mod.ini: %s)", khdays_plugins_platform(), key);
            set_why(p, KHDAYS_PLUGIN_REFUSED, why);
        } else if (!inside(library)) {
            snprintf(why, sizeof(why), "mod.ini: %s = %.200s is not a file inside the mod", key, library);
            set_why(p, KHDAYS_PLUGIN_REFUSED, why);
        } else if ((p->path = joined(folder, library)) == NULL || !khdays_folders_is_file(p->path)) {
            snprintf(why, sizeof(why), "%.200s is not there", library);
            set_why(p, KHDAYS_PLUGIN_REFUSED, why);
        }
    }
done:
    free(library);
    free(sdk);
    free(text);
    free(ini);
    free(folder);
}

/* Whether mod `id`'s plugin has started: 1 yes, 0 not (yet), -1 it has no plugin (whether it is in mods/ is the
 * caller's to see). */
static int ready(const char *id)
{
    for (int i = 0; i < plugin_count; ++i) {
        if (strcmp(plugins[i]->mod_id, id) == 0) {
            return plugins[i]->view.state == KHDAYS_PLUGIN_STARTED;
        }
    }
    return -1; /* not a plugin's mod: present or not is told by the caller */
}

/* A mod folder's id: its mod.ini's, else its name; whether it is `want`. */
typedef struct Seek {
    const char *mods, *want;
    int found;
} Seek;

static void seek(const char *folder_name, void *context)
{
    Seek *s = context;
    char *folder = joined(s->mods, folder_name), *ini = folder != NULL ? joined(folder, "mod.ini") : NULL, *text;
    size_t size = 0;
    if (ini != NULL && (text = khdays_folders_read(ini, &size)) != NULL) {
        char *id = khdays_manifest_value(text, size, "id");
        s->found |= strcmp(id != NULL ? id : folder_name, s->want) == 0;
        free(id);
        free(text);
    }
    free(ini);
    free(folder);
}

/* Whether every mod `p` requires is there and, when it has a plugin, started: 1 yes, 0 not yet, -1 never (`why`). */
static int requirements(const char *mods, const Plugin *p, char *why, size_t why_size)
{
    int result = 1;
    char *list = p->requires != NULL ? copy(p->requires) : NULL;
    for (char *next = list, *one; next != NULL && result != -1;) {
        one = next;
        next = strchr(next, ',');
        if (next != NULL) {
            *next++ = '\0';
        }
        while (*one == ' ' || *one == '\t') {
            ++one;
        }
        for (char *end = one + strlen(one); end > one && (end[-1] == ' ' || end[-1] == '\t');) {
            *--end = '\0';
        }
        if (*one == '\0') {
            continue;
        }
        const int plugin = ready(one);
        if (plugin == 0) {
            int state = KHDAYS_PLUGIN_WAITING;
            for (int i = 0; i < plugin_count; ++i) {
                state = strcmp(plugins[i]->mod_id, one) == 0 ? plugins[i]->view.state : state;
            }
            if (state == KHDAYS_PLUGIN_REFUSED || state == KHDAYS_PLUGIN_FAILED) {
                snprintf(why, why_size, "requires %.100s, whose plugin did not start", one);
                result = -1;
            } else {
                result = 0;
            }
        } else if (plugin < 0) {
            Seek s = {mods, one, 0};
            khdays_folders_list(mods, seek, &s);
            if (!s.found) {
                snprintf(why, why_size, "requires %.100s, which is not in mods/", one);
                result = -1;
            }
        }
    }
    free(list);
    return result;
}

static void load(Plugin *p)
{
    char why[300];
    KhPluginQuery query;
    const KhPluginDescriptor *d;
    KhResult r;
    p->object = SDL_LoadObject(p->path);
    if (p->object == NULL) {
        snprintf(why, sizeof(why), "its library cannot be loaded (%.200s)", SDL_GetError());
        set_why(p, KHDAYS_PLUGIN_REFUSED, why);
        return;
    }
    query = (KhPluginQuery)SDL_LoadFunction(p->object, KH_PLUGIN_QUERY);
    d = query != NULL ? query() : NULL;
    if (d == NULL || d->size < offsetof(KhPluginDescriptor, stop) + sizeof(d->stop) || d->start == NULL ||
        d->stop == NULL || d->abi_major != KH_ABI_MAJOR || d->abi_minor > KH_ABI_MINOR) {
        if (d == NULL) {
            snprintf(why, sizeof(why), "its library has no %s", KH_PLUGIN_QUERY);
        } else if (d->abi_major != KH_ABI_MAJOR || d->abi_minor > KH_ABI_MINOR) {
            snprintf(why, sizeof(why), "built for SDK %u.%u, this port has %d.%d", d->abi_major, d->abi_minor,
                     KH_ABI_MAJOR, KH_ABI_MINOR);
        } else {
            snprintf(why, sizeof(why), "its descriptor is not as the SDK's");
        }
        SDL_UnloadObject(p->object);
        p->object = NULL;
        set_why(p, KHDAYS_PLUGIN_REFUSED, why);
        return;
    }
    p->descriptor = d;
    p->host.size = sizeof(p->host);
    p->host.abi_major = KH_ABI_MAJOR;
    p->host.abi_minor = KH_ABI_MINOR;
    p->host.plugin = p;
    p->host.port_version = KHDAYS_VERSION;
    p->host.log = host_log;
    p->host.find = host_find;
    tell(p, "plugin %.100s (%.100s, by %.100s) starts", d->name != NULL ? d->name : "", p->library,
         *p->author != '\0' ? p->author : "no one named");
    r = d->start(&p->host);
    if (r != KH_OK) {
        khdays_plugins_forget(p); /* what it registered before failing */
        SDL_UnloadObject(p->object);
        p->object = NULL;
        p->descriptor = NULL;
        snprintf(why, sizeof(why), "its start failed (%d)", (int)r);
        set_why(p, KHDAYS_PLUGIN_FAILED, why);
        return;
    }
    p->started_order = started_count++;
    set_why(p, KHDAYS_PLUGIN_STARTED, "");
}

void khdays_plugins_start(const char *mods, KhdaysPluginsLog log)
{
    say = log;
    khdays_plugins_mark_game_thread();
    khdays_folders_list(mods, found, (void *)mods);
    if (plugin_count == 0) {
        return;
    }
    if (!native_code || safe_mode) {
        for (int i = 0; i < plugin_count; ++i) {
            if (plugins[i]->view.state == KHDAYS_PLUGIN_WAITING) {
                tell(plugins[i], safe_mode ? "plugin not loaded: the game started in safe mode"
                                           : "plugin not loaded: native code is off ([mods] native_code)");
            }
        }
        return;
    }
    if (stopped == NULL) {
        stopped = SDL_CreateSemaphore(0);
    }
    /* the mods they require first: passes until none moves on */
    for (int moved = 1; moved;) {
        moved = 0;
        for (int i = 0; i < plugin_count; ++i) {
            Plugin *p = plugins[i];
            char why[200] = "";
            int r;
            if (p->view.state != KHDAYS_PLUGIN_WAITING) {
                continue;
            }
            r = requirements(mods, p, why, sizeof(why));
            if (r < 0) {
                set_why(p, KHDAYS_PLUGIN_REFUSED, why);
                moved = 1;
            } else if (r > 0) {
                load(p);
                moved = 1;
            }
        }
    }
    for (int i = 0; i < plugin_count; ++i) {
        Plugin *p = plugins[i];
        if (p->view.state == KHDAYS_PLUGIN_WAITING) {
            set_why(p, KHDAYS_PLUGIN_REFUSED, "its required mods wait for one another");
        }
        if (p->view.state == KHDAYS_PLUGIN_REFUSED || p->view.state == KHDAYS_PLUGIN_FAILED) {
            tell(p, "plugin not loaded: %s", p->why);
        }
    }
}

void khdays_plugins_stop(void)
{
    for (int order = started_count - 1; order >= 0; --order) {
        for (int i = 0; i < plugin_count; ++i) {
            Plugin *p = plugins[i];
            if (p->view.state != KHDAYS_PLUGIN_STARTED || p->started_order != order || p->descriptor == NULL) {
                continue;
            }
            p->descriptor->stop();
            khdays_plugins_forget(p); /* what it left registered */
            SDL_UnloadObject(p->object);
            p->object = NULL;
            p->descriptor = NULL;
            set_why(p, KHDAYS_PLUGIN_STOPPED, "");
            tell(p, "plugin stopped and released");
        }
    }
    started_count = 0;
}

void khdays_plugins_frame(void)
{
    if (SDL_GetAtomicInt(&stop_asked)) {
        SDL_SetAtomicInt(&stop_asked, 0);
        khdays_plugins_stop();
        SDL_SignalSemaphore(stopped);
    }
}

void khdays_plugins_quit(int ms)
{
    if (started_count == 0 || stopped == NULL) {
        return;
    }
    SDL_SetAtomicInt(&stop_asked, 1);
    SDL_WaitSemaphoreTimeout(stopped, ms);
}

int khdays_plugins_count(void)
{
    return plugin_count;
}

const KhdaysPluginView *khdays_plugins_at(int index)
{
    return index >= 0 && index < plugin_count ? &plugins[index]->view : NULL;
}
