#include "svc_query.h"

#include "str_ext.h"

#include <cprocess.h>
#include <clist.h>
#include <cfile.h>
#include <libstr.h>
#include <libpath.h>
#include <print.h>

#include <ctype.h>

// SvcEntry -------------------------------------------------------------------

typedef struct _SvcEntry SvcEntry;
SvcEntry* sve_new();
void sve_free(SvcEntry *entry);
bool sve_parse(SvcEntry *entry, CString *line);
void sve_print(SvcEntry *entry, CString *buffer);

// SvcList --------------------------------------------------------------------

typedef struct _SvcList SvcList;
SvcList* svl_new();
void svl_free(SvcList *list);
bool svl_query(SvcList *list);
bool svl_print(SvcList *list);

// SvcEntry -------------------------------------------------------------------

struct _SvcEntry
{
    CString *unit;
    bool running;
    CString *description;
};

SvcEntry* sve_new()
{
    SvcEntry *entry = malloc(sizeof(SvcEntry));

    entry->unit = cstr_new_size(32);
    entry->running = false;
    entry->description = cstr_new_size(32);

    return entry;
}

void sve_free(SvcEntry *entry)
{
    if (!entry)
        return;

    cstr_free(entry->unit);
    cstr_free(entry->description);

    free(entry);
}

bool sve_parse(SvcEntry *entry, CString *line)
{
    if (cstr_isempty(line))
        return false;

    int nparts = 5;

    char *ptr = cstr_data(line);
    char *part;
    int len;

    int count = 1;
    while (count < nparts && str_getpart(&ptr, &part, &len))
    {
        part[len] = '\0';

        switch (count)
        {
        case 1:
        {
            cstr_copy(entry->unit, part);
            const char *ext = path_ext(c_str(entry->unit), true);
            if (ext)
                cstr_terminate(entry->unit, ext - c_str(entry->unit));
            break;
        }
        case 4:
        {
            if (strcmp("running", part) == 0)
                entry->running = true;
            break;
        }
        }

        ++count;
    }

    while(isspace(*ptr)) ++ptr;

    // end of buffer ?
    if (*ptr == '\0')
        return false;

    cstr_copy(entry->description, ptr);

    return true;
}

void sve_print(SvcEntry *entry, CString *buffer)
{
    cstr_ellipsize(buffer, c_str(entry->unit), 26);
    printf("%s", c_str(buffer));

    if (entry->running)
        cstr_ellipsize(buffer, "running", 7);
    else
        cstr_ellipsize(buffer, "       ", 7);
    printf(" %s", c_str(buffer));

    cstr_ellipsize(buffer, c_str(entry->description), 40);
    printf(" %s", c_str(buffer));

    printf("\n");
}

// SvcList --------------------------------------------------------------------

struct _SvcList
{
    CList *entryList;
};

SvcList* svl_new()
{
    SvcList *list = malloc(sizeof(SvcList));

    list->entryList = clist_new(64, (CDeleteFunc) sve_free);

    return list;
}

void svl_free(SvcList *list)
{
    if (!list)
        return;

    clist_free(list->entryList);

    free(list);
}

#define SvcListAuto GC_CLEANUP(_freeSvcList) SvcList
GC_UNUSED static inline void _freeSvcList(SvcList **obj)
{
    svl_free(*obj);
}

bool svl_query(SvcList *list)
{
    CStringAuto *cmd = cstr_new("systemctl --no-pager list-units --type=service");

    CProcessAuto *cpr = cprocess_new();

    if (!cprocess_start(cpr, c_str(cmd), CP_PIPEOUT))
    {
        print("start failed");

        return -1;
    }

    int status = cprocess_exitstatus(cpr);

    if (status != 0)
    {
        print("program returned : %d", status);

        return -1;
    }

    CString *outbuff = cprocess_outbuff(cpr);
    const char *ptr = cstr_data(outbuff);
    CStringAuto *line = cstr_new_size(64);

    if (!file_getline(&ptr, line))
        return false;

    while (file_getline(&ptr, line))
    {
        if (!cstr_startswith(line, "  ", true))
            continue;

        SvcEntry *entry = sve_new();

        if (!sve_parse(entry, line))
        {
            sve_free(entry);
            continue;
        }

        clist_append(list->entryList, entry);
    }

    return true;
}

bool svl_print(SvcList *list)
{
    CStringAuto *buff = cstr_new_size(64);

    int size = clist_size(list->entryList);

    for (int i = 0; i < size; ++i)
    {
        SvcEntry *entry = (SvcEntry*) clist_at(list->entryList, i);

        if (entry->running)
            sve_print(entry, buff);
    }

    for (int i = 0; i < size; ++i)
    {
        SvcEntry *entry = (SvcEntry*) clist_at(list->entryList, i);

        if (!entry->running)
            sve_print(entry, buff);
    }

    return true;
}

bool svc_query()
{
    SvcListAuto *list = svl_new();

    svl_query(list);
    svl_print(list);

    return true;
}


