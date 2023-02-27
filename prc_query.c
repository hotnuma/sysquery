#include "prc_query.h"

#include "str_ext.h"

#include <cdirparser.h>
#include <cfile.h>
#include <clist.h>
#include <libstr.h>
#include <libmacros.h>
#include <print.h>

#include <libgen.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>

#include <assert.h>

bool _have_pss = false;
bool _have_swap_pss = false;

// PrcItem --------------------------------------------------------------------

typedef struct _PrcItem PrcItem;
PrcItem* pitem_new(long pid);
void pitem_free(PrcItem *item);
bool pitem_parse_cmd(PrcItem *item);
bool pitem_parse_mem(PrcItem *item);
static long _read_mem(char *line);
bool pitem_parse_status(PrcItem *item);
void _pitem_get_uid_name(PrcItem *item);

// PrcList --------------------------------------------------------------------

typedef struct _PrcList PrcList;
PrcList* plist_new();
void plist_free(PrcList *list);
int plist_size(PrcList *list);
PrcItem* plist_at(PrcList *list, int i);
bool plist_parse(PrcList *list);
bool plist_append(PrcList *list, long pid);
PrcItem* plist_find(PrcList *list, const char *name);
void plist_print(PrcList *list);

// PrcItem --------------------------------------------------------------------

struct _PrcItem
{
    int pid;

    CString *cmdline;
    CString *prpath;
    CString *name;

    unsigned int uid;
    CString *uid_name;

    long priv;
    long shared;
    long shared_huge;
    long swap;

    long total;
};

PrcItem* pitem_new(long pid)
{
    PrcItem *item = (PrcItem*) malloc(sizeof(PrcItem));

    item->pid = pid;
    item->cmdline = cstr_new_size(256);
    item->prpath = cstr_new_size(64);
    item->name = cstr_new_size(24);

    item->uid = 0;
    item->uid_name = cstr_new_size(24);

    item->priv = 0;
    item->shared = 0;
    item->shared_huge = 0;
    item->swap = 0;

    item->total = 0;

    return item;
}

void pitem_free(PrcItem *item)
{
    if (!item)
        return;

    cstr_free(item->cmdline);
    cstr_free(item->prpath);
    cstr_free(item->name);
    cstr_free(item->uid_name);

    free(item);
}

bool pitem_parse_cmd(PrcItem *item)
{
    CStringAuto *filepath = cstr_new_size(32);
    cstr_fmt(filepath, "/proc/%ld/cmdline", item->pid);

    CString *cmdline = item->cmdline;

    file_read(cmdline, c_str(filepath));

    if (cstr_isempty(cmdline))
        return false;

    // replace zeros with spaces
    int size = cstr_size(cmdline);
    char *p = cstr_data(cmdline);
    for (int i = 0; i < size; ++i)
    {
        if (p[i] == '\0')
            p[i] = ' ';
    }
    if (cstr_last(cmdline) == ' ')
        cstr_chop(cmdline, 1);

    char *sp = strchr(c_str(cmdline), ' ');
    if (sp)
        cstr_left(cmdline, item->prpath, (sp - cstr_data(cmdline)));
    else
        cstr_copy(item->prpath, c_str(cmdline));

    char *pname = basename(c_str(item->prpath));
    if (!pname)
        pname = c_str(item->prpath);

    cstr_copy(item->name, pname);

    return true;
}

bool pitem_parse_mem(PrcItem *item)
{
    CStringAuto *filepath = cstr_new_size(32);
    cstr_fmt(filepath, "/proc/%ld/smaps_rollup", item->pid);

    if (!file_exists(c_str(filepath)))
        return false;

    CStringAuto *buffer = cstr_new_size(512);

    file_read(buffer, c_str(filepath));

    long private_huge = 0;
    long pss = 0;
    long swap_pss = 0;

    char *ptr = c_str(buffer);
    char *line;
    int length;

    while (str_getlineptr(&ptr, &line, &length))
    {
        line[length] = '\0';

        if (str_startswith(line, "Private_Hugetlb:", true))
        {
            long val = _read_mem(line);
            if (val > 0)
                private_huge += val;
        }
        else if (str_startswith(line, "Shared_Hugetlb:", true))
        {
            long val = _read_mem(line);
            if (val > 0)
                item->shared_huge += val;
        }
        else if (str_startswith(line, "Shared", true))
        {
            long val = _read_mem(line);
            if (val > 0)
                item->shared += val;
        }
        else if (str_startswith(line, "Private", true))
        {
            long val = _read_mem(line);
            if (val > 0)
                item->priv += val;
        }
        else if (str_startswith(line, "Pss:", true))
        {
            _have_pss = true;

            long val = _read_mem(line);
            if (val > 0)
                pss += val;
        }
        else if (str_startswith(line, "Swap:", true))
        {
            long val = _read_mem(line);
            if (val > 0)
                item->swap += val;
        }
        else if (str_startswith(line, "SwapPss:", true))
        {
            _have_swap_pss = true;

            long val = _read_mem(line);
            if (val > 0)
                swap_pss += val;
        }
    }

    if (_have_pss)
        item->shared = pss - item->priv;

    item->priv += private_huge;

    if (_have_swap_pss)
        item->swap = swap_pss;

    item->total = item->priv + item->shared + item->shared_huge;

    return true;
}

static long _read_mem(char *line)
{
    char *p = line;

    while (!isdigit(*p) && *p != '\0')
    {
        ++p;
    }

    if (*p == '\0')
        return -1;

    return strtol(p, NULL, 10);
}

bool pitem_parse_status(PrcItem *item)
{
    CStringAuto *filepath = cstr_new_size(32);
    cstr_fmt(filepath, "/proc/%d/status", item->pid);

    if (!file_exists(c_str(filepath)))
        return false;

    CStringAuto *buffer = cstr_new_size(512);

    file_read(buffer, c_str(filepath));

    unsigned int dummy;

    char *ptr = c_str(buffer);
    char *line;
    int length;

    while (str_getlineptr(&ptr, &line, &length))
    {
        line[length] = '\0';

        if (str_startswith(line, "Uid:", true)
            && sscanf(line, "Uid:\t%u\t%u\t%u\t%u",
                            &dummy,
                            &item->uid,
                            &dummy,
                            &dummy) == 4)
        {
            _pitem_get_uid_name(item);

            //print(c_str(item->uid_name));

            break;
        }
    }

    return true;
}

void _pitem_get_uid_name(PrcItem *item)
{
    struct passwd *pw = getpwuid(item->uid);

    if (!pw)
    {
        cstr_copy(item->uid_name, "nobody");
        return;
    }

    cstr_copy(item->uid_name, pw->pw_name);
}

// PrcList --------------------------------------------------------------------

static int _pi_cmpmem(void *entry1, void *entry2)
{
    PrcItem *e1 = *((PrcItem**) entry1);
    PrcItem *e2 = *((PrcItem**) entry2);

    return (e2->total - e1->total);
}

struct _PrcList
{
    CList *list;
    //MemList *memlist;
};

#define PrcListAuto GC_CLEANUP(_freePrcList) PrcList
GC_UNUSED static inline void _freePrcList(PrcList **obj)
{
    plist_free(*obj);
}

PrcList* plist_new()
{
    PrcList *list = (PrcList*) malloc(sizeof(PrcList));

    list->list = clist_new(64, (CDeleteFunc) pitem_free);
    //list->memlist = mlist_new();

    return list;
}

void plist_free(PrcList *list)
{
    if (!list)
        return;

    clist_free(list->list);
    //mlist_free(list->memlist);

    free(list);
}

int plist_size(PrcList *list)
{
    return clist_size(list->list);
}

PrcItem* plist_at(PrcList *list, int i)
{
    return (PrcItem*) clist_at(list->list, i);
}

bool plist_parse(PrcList *list)
{
    const char *procdir = "/proc";

    CDirParserAuto *dir = cdirparser_new();
    if (!cdirparser_open(dir, procdir, CDP_DIRS))
        return false;

    CStringAuto *dirname = cstr_new_size(16);

    while (cdirparser_read(dir, dirname))
    {
        if (cstr_size(dirname) < 6)
            continue;

        const char *subdir = c_str(dirname) + 6;
        char *endptr;

        long pid = strtol(subdir, &endptr, 10);
        if (*endptr != '\0')
            continue;

        plist_append(list, pid);
    }

    return true;
}

bool plist_append(PrcList *list, long pid)
{
    PrcItem *item = pitem_new(pid);

    if (!pitem_parse_cmd(item))
    {
        pitem_free(item);

        return false;
    }

    if (!pitem_parse_mem(item))
    {
        print("*** parsemem error");

        pitem_free(item);

        return false;
    }

    pitem_parse_status(item);

    clist_append(list->list, item);

    return true;
}

PrcItem* plist_find(PrcList *list, const char *name)
{
    PrcItem *item = NULL;

    int size = clist_size(list->list);

    for (int i = 0; i < size; ++i)
    {
        PrcItem *item = (PrcItem*) clist_at(list->list, i);

        if (strcmp(c_str(item->name), name) == 0)
            return item;
    }

    return item;
}

void plist_print(PrcList *list)
{
    CStringAuto *buff = cstr_new_size(64);

    clist_sort(list->list, (CCompareFunc) _pi_cmpmem);

    long total = 0;

    int size = plist_size(list);

    for (int i = 0; i < size; ++i)
    {
        PrcItem *item = plist_at(list, i);

        // name
        cstr_ellipsize(buff, c_str(item->name), 22);
        printf("%s", c_str(buff));

        cstr_long(buff, item->pid, 7);
        printf(" %s", c_str(buff));

        cstr_ellipsize(buff, c_str(item->uid_name), 16);
        printf(" %s", c_str(buff));

        cstr_long(buff, item->total, 7);
        printf(" %s", c_str(buff));

        printf("\n");

        total += item->total;
    }

    print("total : %ld", total);
}

// Query ----------------------------------------------------------------------

bool prc_query()
{
    PrcListAuto *list = plist_new();
    plist_parse(list);
    plist_print(list);

    return true;
}


