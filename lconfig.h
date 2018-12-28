/*
lconfig.h - Portable, single-file, config library with config file generation based on a template

To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring
rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software.
If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

/*
lconfig supports the following three configurations:
#define LCONFIG_EXTERN
    Default, should be used when using lconfig in multiple compilation units within the same project.
#define LCONFIG_IMPLEMENTATION
    Must be defined in exactly one source file within a project for lconfig to be found by the linker.
#define LCONFIG_STATIC
    Defines all lconfig functions as static, useful if lconfig is only used in a single compilation unit.

lconfig supports the following additional options:
#define LCONFIG_PATH
    Sets the path of the config file used by lconfig to read/write config values. Default is "config.txt".
#define LCONFIG_LMAX
    Sets the maximum line length read from the config file, rarely needs to be changed. Default is 512.

lconfig init:
    All config values start out at their defaults at program startup. If you wish to read/create the config
    file immediately, you should do lconfigRead() followed by lconfigWrite(). This will cause any existing
    config file to be read, and adjusted values to be written back, thus clamping any values in the file.

lconfig double:
    Previously lconfig supported double values directly, this however proved superfluous as fractional values
    can almost always be expressed as scaled integers. For example a value in the range 0.0 to 1.0 could be
    expressed as an integer in the range 0 to 1000 or similar depending on desired resolution of the value.

lconfig template:
    A template is used to tell lconfig what config values exist, what their limits and defaults are, and
    how they are presented in the config file (along with newlines and labels, see examples further down).
    The template has to be put into the #define value LCONFIG_TEMPLATE before including the implementation.
    It consists only of LCONFIG_INT(ID, NAME, MIN, MAX, DEF) and LCONFIG_STR(ID, NAME, LEN, DEF) macros for
    config values, and LCONFIG_LINE(...) for labels/lines. Scaled int should be used for fractional values.
    ID is the int ID of a config value, and must be unique within that data type, but not across data
    types, i.e. there can be both an int and a string config value with an ID of 0. Typically enums or
    #define constants are used for these IDs. NAME is a string of the name of a config value in the file.
    MIN/MAX are the limits of int config values. LEN is the maximum length (not counting NUL terminator)
    for string config values. DEF is the default value (must be a literal of the appropriate data type).

<lconfig example template begin>
//define ID constants for ints
#define FOO_ANUMBER 0
#define FOO_BNUMBER 1
//define ID constants for strs
#define FOO_ASTRING 0
#define FOO_BSTRING 1
//define template using the macros
#define LCONFIG_TEMPLATE \
    LCONFIG_LINE("#example") \
    LCONFIG_INT(FOO_ANUMBER, "number_a", -10, 10, 0) \
    LCONFIG_LINE() \
    LCONFIG_LINE("#foobar") \
    LCONFIG_STR(FOO_ASTRING, "string_a", 32, "ABCD") \
    LCONFIG_INT(FOO_BNUMBER, "number_b", 10, 20, 15) \
    LCONFIG_STR(FOO_BSTRING, "string_b", 16, "FOO")
//include implementation after template
#define LCONFIG_IMPLEMENTATION
#include <lconfig.h>
<lconfig example template end>

<lconfig example config.txt begin>
#example
number_a 0

#foobar
string_a ABCD
number_b 15
string_b FOO
<lconfig example config.txt end>
*/

//header section
#ifndef LCONFIG_H
#define LCONFIG_H

//process configuration
#ifdef LCONFIG_STATIC
    #define LCONFIG_IMPLEMENTATION
    #define LCONDEF static
#else //LCONFIG_EXTERN
    #define LCONDEF extern
#endif

//function declarations
LCONDEF void lconfigDefault();
    //resets all config values to their defaults (does not write to file)
LCONDEF int lconfigRead();
    //reads config values from file if it exists, only changing ones in the file
    //returns 0 on success, non-zero if the file could not be read
LCONDEF int lconfigWrite();
    //writes current config values to file if possible, creating the file if needed
    //returns 0 on success, non-zero if the file could not be written
LCONDEF int lconfigGetInt(int);
    //returns the value of the given integer config value (-1 if invalid)
LCONDEF void lconfigSetInt(int, int);
    //sets the value of the given integer config value (subject to clamping)
LCONDEF const char* lconfigGetString(int);
    //returns the value of the given string config value (NULL if invalid)
LCONDEF void lconfigSetString(int, const char*);
    //sets the value of the given string config value (subject to clamping)

#endif //LCONFIG_H

//implementation section
#ifdef LCONFIG_IMPLEMENTATION
#undef LCONFIG_IMPLEMENTATION

//includes
#include <string.h> //string operations
#include <stdlib.h> //atoi and others
#include <stdio.h> //reading/writing config file

//constants
#ifndef LCONFIG_PATH
    #define LCONFIG_PATH "config.txt"
#endif
#ifndef LCONFIG_LMAX
    #define LCONFIG_LMAX 512
#endif

//structs
struct lcfg_int {
    const char* const name; //name in config file
    const int min; //min value
    const int max; //max value
    const int def; //default value
    int cur; //current value
};
struct lcfg_str {
    const char* const name; //name in config file
    const int len; //maximum length
    const char* const def; //default value
    char* const cur; //current value
};

//function declarations
static void lcfgIntRead(struct lcfg_int*, const char*);
static void lcfgStrRead(struct lcfg_str*, const char*);
static void lcfgIntPrint(struct lcfg_int*, FILE*);
static void lcfgStrPrint(struct lcfg_str*, FILE*);
static void lcfgIntSet(struct lcfg_int*, int);
static void lcfgStrSet(struct lcfg_str*, const char*);

//internal globals
#define LCONFIG_LINE(...)
#define LCONFIG_INT(ID, NAME, MIN, MAX, DEF) [ID] = {NAME " ", MIN, MAX, DEF, DEF},
#define LCONFIG_STR(ID, NAME, LEN, DEF)
static struct lcfg_int lcfg_ints[] = {{0}, LCONFIG_TEMPLATE};
#undef LCONFIG_INT
#undef LCONFIG_STR
#define LCONFIG_INT(ID, NAME, MIN, MAX, DEF)
#define LCONFIG_STR(ID, NAME, LEN, DEF) [ID] = {NAME " ", LEN, DEF, (char[LEN+1]){DEF}},
static struct lcfg_str lcfg_strs[] = {{0}, LCONFIG_TEMPLATE};
#undef LCONFIG_LINE
#undef LCONFIG_INT
#undef LCONFIG_STR

//public functions
LCONDEF void lconfigDefault () {
    for (int i = 0; i < sizeof(lcfg_ints)/sizeof(lcfg_ints[0]); i++)
        if (lcfg_ints[i].name) lcfgIntSet(&lcfg_ints[i], lcfg_ints[i].def);
    for (int i = 0; i < sizeof(lcfg_strs)/sizeof(lcfg_strs[0]); i++)
        if (lcfg_strs[i].name) lcfgStrSet(&lcfg_strs[i], lcfg_strs[i].def);
}
LCONDEF int lconfigRead () {
    FILE* cfg = fopen(LCONFIG_PATH, "r");
    if (cfg) {
        char txt[LCONFIG_LMAX];
        while (fgets(txt, LCONFIG_LMAX, cfg)) {
            for (int i = 0; i < sizeof(lcfg_ints)/sizeof(lcfg_ints[0]); i++)
                if (lcfg_ints[i].name) lcfgIntRead(&lcfg_ints[i], txt);
            for (int i = 0; i < sizeof(lcfg_strs)/sizeof(lcfg_strs[0]); i++)
                if (lcfg_strs[i].name) lcfgStrRead(&lcfg_strs[i], txt);
        }
        fclose(cfg);
        return 0;
    }
    return 1;
}
#define LCONFIG_LINE(...) fprintf(cfg, __VA_ARGS__ "\n");
#define LCONFIG_INT(ID, NAME, MIN, MAX, DEF) lcfgIntPrint(&lcfg_ints[ID], cfg);
#define LCONFIG_STR(ID, NAME, LEN, DEF) lcfgStrPrint(&lcfg_strs[ID], cfg);
LCONDEF int lconfigWrite () {
    FILE* cfg = fopen(LCONFIG_PATH, "w");
    if (cfg) {
        LCONFIG_TEMPLATE;
        fclose(cfg);
        return 0;
    }
    return 1;
}
#undef LCONFIG_LINE
#undef LCONFIG_INT
#undef LCONFIG_STR
LCONDEF int lconfigGetInt (int id) {
    if ((id >= 0)&&(id < sizeof(lcfg_ints)/sizeof(lcfg_ints[0]))&&(lcfg_ints[id].name))
        return lcfg_ints[id].cur;
    return -1;
}
LCONDEF void lconfigSetInt (int id, int val) {
    if ((id >= 0)&&(id < sizeof(lcfg_ints)/sizeof(lcfg_ints[0]))&&(lcfg_ints[id].name))
        lcfgIntSet(&lcfg_ints[id], val);
}
LCONDEF const char* lconfigGetString (int id) {
    if ((id >= 0)&&(id < sizeof(lcfg_strs)/sizeof(lcfg_strs[0]))&&(lcfg_strs[id].name))
        return lcfg_strs[id].cur;
    return NULL;
}
LCONDEF void lconfigSetString (int id, const char* val) {
    if ((id >= 0)&&(id < sizeof(lcfg_strs)/sizeof(lcfg_strs[0]))&&(lcfg_strs[id].name))
        lcfgStrSet(&lcfg_strs[id], val);
}

//internal functions
static void lcfgIntRead (struct lcfg_int* cfg, const char* txt) {
    if (strncmp(cfg->name, txt, strlen(cfg->name)) == 0)
        lcfgIntSet(cfg, atoi(&txt[strlen(cfg->name)]));
}
static void lcfgStrRead (struct lcfg_str* cfg, const char* txt) {
    if (strncmp(cfg->name, txt, strlen(cfg->name)) == 0)
        lcfgStrSet(cfg, &txt[strlen(cfg->name)]);
}
static void lcfgIntPrint (struct lcfg_int* cfg, FILE* fpt) {
    fprintf(fpt, "%s%d\n", cfg->name, cfg->cur);
}
static void lcfgStrPrint (struct lcfg_str* cfg, FILE* fpt) {
    fprintf(fpt, "%s%s\n", cfg->name, cfg->cur);
}
static void lcfgIntSet (struct lcfg_int* cfg, int val) {
    if (val < cfg->min) val = cfg->min;
    if (val > cfg->max) val = cfg->max;
    cfg->cur = val;
}
static void lcfgStrSet (struct lcfg_str* cfg, const char* val) {
    size_t len = strcspn(val, "\n"); //find first newline
    if (len > cfg->len) len = cfg->len; //clamp to value max length
    strncpy(cfg->cur, val, len); //copy string up to len characters
    cfg->cur[len] = 0; //make sure string is properly terminated
}

#endif //LCONFIG_IMPLEMENTATION