/*
 * V2GUARD - keeps the 3dfx Voodoo2 driver quiet when no Voodoo2 is in the PC
 * Windows 95/98/ME. (c) Bits und Bolts.
 *
 * The 3dfx Voodoo2 driver (e.g. 3.02.02) hooks into Windows in two places:
 *   1. HKLM\Software\Microsoft\Windows\CurrentVersion\Run  "Voodoo2"
 *        = rundll32.exe 3dfxv2ps.dll,UpdateRegSettings
 *      -> "_GlideInitEnvironment: glide2x.dll expected Voodoo^2, none detected" at every start
 *   2. HKLM\...\Controls Folder\Display\shellex\PropertySheetHandlers\Voodoo2  = {CLSID of 3dfxv2ps.dll}
 *      -> the "3dfx V2-1000" tab in Display Properties (slow / freezes without the card)
 *
 * V2GUARD takes the driver's Run entry over and runs at every Windows start instead:
 *   - it looks for a Voodoo2 among the devices Windows found this boot (HKEY_DYN_DATA\Config Manager\Enum,
 *     HardWareKey PCI\VEN_121A&DEV_0002 - the old entry under HKLM\Enum does not count);
 *   - Voodoo2 there:  the tab entry is put back (if it was taken away) and the driver's own command runs;
 *   - no Voodoo2:     the tab entry is taken away (kept in V2GUARD's key) and the command does not run.
 * The driver's files are never changed. A driver installed again later puts its Run entry back: V2GUARD
 * takes it over again at the next start.
 *
 *   V2GUARD             what runs at Windows start (silent; writes V2GUARD.LOG next to the program)
 *   V2GUARD /INSTALL    take over the driver's entries and run at every start
 *   V2GUARD /UNINSTALL  give the driver its entries back, remove V2GUARD from the start
 *   V2GUARD /STATUS     show what V2GUARD knows and sees
 *   /Q                  no message box (for batch files; the result is in V2GUARD.LOG and the exit code)
 *   /ID=PCI\VEN_xxxx&DEV_xxxx   test only: treat this device as "the Voodoo2"
 *
 * Exit codes: 0 ok, 1 nothing to do / not installed, 2 registry error, 3 bad command line
 * Build: OpenWatcom  wcl386 -bt=nt -l=nt_win -zq -s -os V2GUARD.C
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define VERSION   "1.0"
#define VOODOO2   "PCI\\VEN_121A&DEV_0002"
#define RUN_KEY   "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define RUN_DRV   "Voodoo2"                     /* the driver's Run value */
#define RUN_ME    "V2Guard"                     /* ours */
#define SHEET_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Controls Folder\\Display\\shellex\\PropertySheetHandlers\\Voodoo2"
#define MY_KEY    "Software\\Bits und Bolts\\V2Guard"
#ifndef HKEY_DYN_DATA
#define HKEY_DYN_DATA ((HKEY)0x80000006)
#endif

static char g_log[4096];
static int  g_quiet;
static char g_id[128] = VOODOO2;

static void say(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(line, sizeof line - 3, fmt, ap);
    va_end(ap);
    line[sizeof line - 3] = 0;
    strcat(line, "\r\n");
    if (strlen(g_log) + strlen(line) < sizeof g_log)
        strcat(g_log, line);
}

/* ---------------------------------------------------------------- registry helpers */
static int get_str(HKEY root, const char *key, const char *name, char *out, DWORD size)
{
    HKEY h;
    DWORD type, n = size;
    out[0] = 0;
    if (RegOpenKeyExA(root, key, 0, KEY_READ, &h) != ERROR_SUCCESS)
        return 0;
    if (RegQueryValueExA(h, name, NULL, &type, (BYTE *)out, &n) != ERROR_SUCCESS || type != REG_SZ) {
        RegCloseKey(h);
        out[0] = 0;
        return 0;
    }
    RegCloseKey(h);
    out[size - 1] = 0;
    return 1;
}

static int set_str(HKEY root, const char *key, const char *name, const char *val)
{
    HKEY h;
    DWORD disp;
    LONG r;
    if (RegCreateKeyExA(root, key, 0, NULL, 0, KEY_WRITE, NULL, &h, &disp) != ERROR_SUCCESS)
        return 0;
    r = RegSetValueExA(h, name, 0, REG_SZ, (const BYTE *)val, (DWORD)strlen(val) + 1);
    RegCloseKey(h);
    return r == ERROR_SUCCESS;
}

static void del_val(HKEY root, const char *key, const char *name)
{
    HKEY h;
    if (RegOpenKeyExA(root, key, 0, KEY_WRITE, &h) == ERROR_SUCCESS) {
        RegDeleteValueA(h, name);
        RegCloseKey(h);
    }
}

static int key_exists(HKEY root, const char *key)
{
    HKEY h;
    if (RegOpenKeyExA(root, key, 0, KEY_READ, &h) != ERROR_SUCCESS)
        return 0;
    RegCloseKey(h);
    return 1;
}

/* ---------------------------------------------------------------- is a Voodoo2 there? */
static int voodoo2_present(char *found, DWORD size)
{
    HKEY root;
    char name[64], hw[256], path[160];
    DWORD i, n;
    size_t idlen = strlen(g_id);
    found[0] = 0;
    if (RegOpenKeyExA(HKEY_DYN_DATA, "Config Manager\\Enum", 0, KEY_READ, &root) != ERROR_SUCCESS)
        return -1;
    for (i = 0;; i++) {
        n = sizeof name;
        if (RegEnumKeyExA(root, i, name, &n, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;
        sprintf(path, "Config Manager\\Enum\\%s", name);
        if (!get_str(HKEY_DYN_DATA, path, "HardWareKey", hw, sizeof hw))
            continue;
        if (strnicmp(hw, g_id, idlen) == 0) {
            strncpy(found, hw, size - 1);
            found[size - 1] = 0;
            RegCloseKey(root);
            return 1;
        }
    }
    RegCloseKey(root);
    return 0;
}

/* ---------------------------------------------------------------- take the driver's entries over */
static void adopt(void)
{
    char v[512];
    if (get_str(HKEY_LOCAL_MACHINE, RUN_KEY, RUN_DRV, v, sizeof v)) {
        set_str(HKEY_LOCAL_MACHINE, MY_KEY, "DriverCommand", v);
        del_val(HKEY_LOCAL_MACHINE, RUN_KEY, RUN_DRV);
        say("Took over the driver's start entry: %s", v);
    }
    if (get_str(HKEY_LOCAL_MACHINE, SHEET_KEY, "", v, sizeof v) && v[0]) {
        set_str(HKEY_LOCAL_MACHINE, MY_KEY, "DisplayTab", v);
    }
}

/* ---------------------------------------------------------------- what runs at every start */
static int guard(void)
{
    char found[256], cmd[512], tab[128];
    int there;
    adopt();
    get_str(HKEY_LOCAL_MACHINE, MY_KEY, "DriverCommand", cmd, sizeof cmd);
    get_str(HKEY_LOCAL_MACHINE, MY_KEY, "DisplayTab", tab, sizeof tab);
    if (!cmd[0] && !tab[0]) {
        say("No Voodoo2 driver entries known - nothing to do.");
        return 1;
    }
    there = voodoo2_present(found, sizeof found);
    if (there < 0) {
        say("Cannot read the device list (HKEY_DYN_DATA) - left everything as it is.");
        return 2;
    }
    if (there) {
        say("Voodoo2 found: %s", found);
        if (tab[0] && !key_exists(HKEY_LOCAL_MACHINE, SHEET_KEY)) {
            if (set_str(HKEY_LOCAL_MACHINE, SHEET_KEY, "", tab))
                say("Display Properties tab put back.");
            else
                say("Could not put the Display Properties tab back.");
        }
        if (cmd[0]) {
            UINT r = WinExec(cmd, SW_SHOWNORMAL);
            say("Ran the driver's command: %s (%s)", cmd, r > 31 ? "started" : "could not start");
        }
    } else {
        say("No Voodoo2 in this PC (%s not among the devices found).", g_id);
        if (key_exists(HKEY_LOCAL_MACHINE, SHEET_KEY)) {
            if (RegDeleteKeyA(HKEY_LOCAL_MACHINE, SHEET_KEY) == ERROR_SUCCESS)
                say("Display Properties tab taken away (kept: %s).", tab);
            else
                say("Could not take the Display Properties tab away.");
        }
        say("The driver's command was not run.");
    }
    return 0;
}

static void my_path(char *out, DWORD size)
{
    GetModuleFileNameA(NULL, out, size);
    out[size - 1] = 0;
}

static int install(void)
{
    char me[MAX_PATH], q[MAX_PATH + 2];
    my_path(me, sizeof me);
    sprintf(q, "\"%s\"", me);
    if (!set_str(HKEY_LOCAL_MACHINE, RUN_KEY, RUN_ME, q)) {
        say("Could not write the start entry.");
        return 2;
    }
    say("V2GUARD %s runs at every Windows start: %s", VERSION, me);
    return guard() == 2 ? 2 : 0;
}

static int uninstall(void)
{
    char cmd[512], tab[128];
    get_str(HKEY_LOCAL_MACHINE, MY_KEY, "DriverCommand", cmd, sizeof cmd);
    get_str(HKEY_LOCAL_MACHINE, MY_KEY, "DisplayTab", tab, sizeof tab);
    if (cmd[0] && set_str(HKEY_LOCAL_MACHINE, RUN_KEY, RUN_DRV, cmd))
        say("The driver's start entry is back: %s", cmd);
    if (tab[0] && !key_exists(HKEY_LOCAL_MACHINE, SHEET_KEY) && set_str(HKEY_LOCAL_MACHINE, SHEET_KEY, "", tab))
        say("The Display Properties tab is back.");
    del_val(HKEY_LOCAL_MACHINE, RUN_KEY, RUN_ME);
    RegDeleteKeyA(HKEY_LOCAL_MACHINE, MY_KEY);      /* "Software\Bits und Bolts" stays: other tools may use it */
    say("V2GUARD no longer runs at Windows start. The driver is as it was installed.");
    return 0;
}

static int status(void)
{
    char v[512], found[256];
    int there;
    say("V2GUARD %s", VERSION);
    say("Runs at start:     %s", get_str(HKEY_LOCAL_MACHINE, RUN_KEY, RUN_ME, v, sizeof v) ? v : "no");
    say("Driver's command:  %s", get_str(HKEY_LOCAL_MACHINE, MY_KEY, "DriverCommand", v, sizeof v) ? v : "(not known)");
    say("Driver's own entry still in Run: %s",
        get_str(HKEY_LOCAL_MACHINE, RUN_KEY, RUN_DRV, v, sizeof v) ? "yes" : "no");
    say("Display tab kept:  %s", get_str(HKEY_LOCAL_MACHINE, MY_KEY, "DisplayTab", v, sizeof v) ? v : "(not known)");
    say("Display tab shown: %s", key_exists(HKEY_LOCAL_MACHINE, SHEET_KEY) ? "yes" : "no");
    there = voodoo2_present(found, sizeof found);
    say("Voodoo2 in this PC: %s", there > 0 ? found : there < 0 ? "cannot tell" : "no");
    return 0;
}

static void write_log(const char *what)
{
    char path[MAX_PATH];
    char *p;
    HANDLE f;
    DWORD n;
    SYSTEMTIME t;
    char head[128];
    my_path(path, sizeof path);
    p = strrchr(path, '\\');
    strcpy(p ? p + 1 : path, "V2GUARD.LOG");
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    GetLocalTime(&t);
    sprintf(head, "V2GUARD %s %s, %04u-%02u-%02u %02u:%02u:%02u\r\n", VERSION, what,
            t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    WriteFile(f, head, (DWORD)strlen(head), &n, NULL);
    WriteFile(f, g_log, (DWORD)strlen(g_log), &n, NULL);
    CloseHandle(f);
}

int PASCAL WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmdline, int show)
{
    char args[512];
    char *a;
    int mode = 0, rc;                       /* 0 start, 1 install, 2 uninstall, 3 status */
    const char *what[] = { "start", "install", "uninstall", "status" };
    (void)hi; (void)hp; (void)show;
    strncpy(args, cmdline ? cmdline : "", sizeof args - 1);
    args[sizeof args - 1] = 0;
    for (a = strtok(args, " \t"); a; a = strtok(NULL, " \t")) {
        if (stricmp(a, "/INSTALL") == 0) mode = 1;
        else if (stricmp(a, "/UNINSTALL") == 0) mode = 2;
        else if (stricmp(a, "/STATUS") == 0) mode = 3;
        else if (stricmp(a, "/Q") == 0) g_quiet = 1;
        else if (strnicmp(a, "/ID=", 4) == 0 && a[4]) {
            strncpy(g_id, a + 4, sizeof g_id - 1);
            g_id[sizeof g_id - 1] = 0;
        } else {
            MessageBoxA(NULL, "V2GUARD [/INSTALL | /UNINSTALL | /STATUS] [/Q]\n\n"
                        "Keeps the 3dfx Voodoo2 driver quiet when no Voodoo2 is in the PC.",
                        "V2GUARD " VERSION, MB_OK | MB_ICONINFORMATION);
            return 3;
        }
    }
    if (strcmp(g_id, VOODOO2) != 0)
        say("TEST: treating %s as the Voodoo2.", g_id);
    rc = mode == 1 ? install() : mode == 2 ? uninstall() : mode == 3 ? status() : guard();
    write_log(what[mode]);
    if (mode != 0 && !g_quiet)
        MessageBoxA(NULL, g_log, "V2GUARD " VERSION, MB_OK | (rc >= 2 ? MB_ICONSTOP : MB_ICONINFORMATION));
    return rc;
}
