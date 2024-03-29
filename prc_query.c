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
PrcItem* pritem_new(long pid);
void pritem_free(PrcItem *item);
bool pritem_parse_cmd(PrcItem *item);
bool pritem_parse_mem(PrcItem *item);
static long _read_mem(char *line);
bool pritem_parse_status(PrcItem *item);
void _pritem_get_uid_name(PrcItem *item);
bool pritem_is(PrcItem *item, const char *name);
void pritem_merge(PrcItem *item, PrcItem *other);

// PrcList --------------------------------------------------------------------

typedef struct _PrcList PrcList;
PrcList* prlist_new();
void prlist_free(PrcList *list);
int prlist_size(PrcList *list);
PrcItem* prlist_at(PrcList *list, int i);
bool prlist_parse(PrcList *list);
bool prlist_append(PrcList *list, long pid);
PrcItem* prlist_find(PrcList *list, const char *name);
void prlist_print(PrcList *list);

// PrcItem --------------------------------------------------------------------

struct _PrcItem
{
    int pid;

    CString *cmdline;
    CString *prpath;
    CString *name;

    unsigned int uid;
    CString *uid_name;

    long total;
    int count;
};

PrcItem* pritem_new(long pid)
{
    PrcItem *item = (PrcItem*) malloc(sizeof(PrcItem));

    item->pid = pid;
    item->cmdline = cstr_new_size(256);
    item->prpath = cstr_new_size(64);
    item->name = cstr_new_size(24);

    item->uid = 0;
    item->uid_name = cstr_new_size(24);

    item->total = 0;
    item->count = 1;

    return item;
}

void pritem_free(PrcItem *item)
{
    if (!item)
        return;

    cstr_free(item->cmdline);
    cstr_free(item->prpath);
    cstr_free(item->name);
    cstr_free(item->uid_name);

    free(item);
}

bool pritem_parse_cmd(PrcItem *item)
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

bool pritem_parse_mem(PrcItem *item)
{
    CStringAuto *filepath = cstr_new_size(32);
    cstr_fmt(filepath, "/proc/%ld/smaps_rollup", item->pid);

    if (!file_exists(c_str(filepath)))
        return false;

    CStringAuto *buffer = cstr_new_size(512);

    file_read(buffer, c_str(filepath));

    long mem_priv_huge = 0;
    long mem_pss = 0;
    long mem_swap_pss = 0;

    char *ptr = c_str(buffer);
    char *line;
    int length;

    long mem_priv = 0;
    long mem_shared = 0;
    long mem_shared_huge = 0;
    long mem_swap = 0;

    while (str_getlineptr(&ptr, &line, &length))
    {
        line[length] = '\0';

        if (str_startswith(line, "Private_Hugetlb:", true))
        {
            long val = _read_mem(line);
            if (val > 0)
                mem_priv_huge += val;
        }
        else if (str_startswith(line, "Shared_Hugetlb:", true))
        {
            long val = _read_mem(line);
            if (val > 0)
                mem_shared_huge += val;
        }
        else if (str_startswith(line, "Shared", true))
        {
            long val = _read_mem(line);
            if (val > 0)
                mem_shared += val;
        }
        else if (str_startswith(line, "Private", true))
        {
            long val = _read_mem(line);
            if (val > 0)
                mem_priv += val;
        }
        else if (str_startswith(line, "Pss:", true))
        {
            _have_pss = true;

            long val = _read_mem(line);
            if (val > 0)
                mem_pss += val;
        }
        else if (str_startswith(line, "Swap:", true))
        {
            long val = _read_mem(line);
            if (val > 0)
                mem_swap += val;
        }
        else if (str_startswith(line, "SwapPss:", true))
        {
            _have_swap_pss = true;

            long val = _read_mem(line);
            if (val > 0)
                mem_swap_pss += val;
        }
    }

    if (_have_pss)
        mem_shared = mem_pss - mem_priv;

    mem_priv += mem_priv_huge;

    if (_have_swap_pss)
        mem_swap = mem_swap_pss;

    item->total = mem_priv + mem_shared + mem_shared_huge;

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

bool pritem_parse_status(PrcItem *item)
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
            _pritem_get_uid_name(item);

            //print(c_str(item->uid_name));

            break;
        }
    }

    return true;
}

void _pritem_get_uid_name(PrcItem *item)
{
    struct passwd *pw = getpwuid(item->uid);

    if (!pw)
    {
        cstr_copy(item->uid_name, "nobody");
        return;
    }

    cstr_copy(item->uid_name, pw->pw_name);
}

bool pritem_is(PrcItem *item, const char *name)
{
    return (strcmp(c_str(item->name), name) == 0);
}

void pritem_merge(PrcItem *item, PrcItem *other)
{
    item->total += other->total;
    ++item->count;
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
    bool merge;
};

#define PrcListAuto GC_CLEANUP(_freePrcList) PrcList
GC_UNUSED static inline void _freePrcList(PrcList **obj)
{
    prlist_free(*obj);
}

PrcList* prlist_new()
{
    PrcList *list = (PrcList*) malloc(sizeof(PrcList));

    list->list = clist_new(64, (CDeleteFunc) pritem_free);
    list->merge = true;

    return list;
}

void prlist_free(PrcList *list)
{
    if (!list)
        return;

    clist_free(list->list);

    free(list);
}

int prlist_size(PrcList *list)
{
    return clist_size(list->list);
}

PrcItem* prlist_at(PrcList *list, int i)
{
    return (PrcItem*) clist_at(list->list, i);
}

bool prlist_parse(PrcList *list)
{
    const char *procdir = "/proc";

    CDirParserAuto *dir = cdirparser_new();
    if (!cdirparser_open(dir, procdir, CDP_DIRS))
        return false;

    CStringAuto *dirname = cstr_new_size(16);

    while (cdirparser_read(dir, dirname, NULL))
    {
        if (cstr_size(dirname) < 6)
            continue;

        const char *subdir = c_str(dirname) + 6;
        char *endptr;

        long pid = strtol(subdir, &endptr, 10);
        if (*endptr != '\0')
            continue;

        prlist_append(list, pid);
    }

    return true;
}

bool prlist_append(PrcList *list, long pid)
{
    PrcItem *item = pritem_new(pid);

    if (!pritem_parse_cmd(item))
    {
        pritem_free(item);

        return false;
    }

    if (!pritem_parse_mem(item))
    {
        print("*** parsemem error");

        pritem_free(item);

        return false;
    }

    pritem_parse_status(item);

    if (list->merge)
    {
        PrcItem *found = prlist_find(list, c_str(item->name));

        if (found)
        {
            pritem_merge(found, item);
            pritem_free(item);
            return true;
        }
    }

    clist_append(list->list, item);

    return true;
}

PrcItem* prlist_find(PrcList *list, const char *name)
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

void prlist_print(PrcList *list)
{
    CStringAuto *buff = cstr_new_size(64);

    clist_sort(list->list, (CCompareFunc) _pi_cmpmem);

    long total = 0;
    int count = 0;

    int size = prlist_size(list);

    for (int i = 0; i < size; ++i)
    {
        PrcItem *item = prlist_at(list, i);

        // name
        cstr_ellipsize(buff, c_str(item->name), 22);
        printf("%s", c_str(buff));

        count += item->count;

        if (list->merge)
        {
            if (item->count > 1)
            {
                cstr_long(buff, item->count, 2);
                printf(" %s", c_str(buff));
            }
            else
            {
                printf("   ");
            }
        }

        cstr_long(buff, item->pid, 7);
        printf(" %s", c_str(buff));

        cstr_ellipsize(buff, c_str(item->uid_name), 16);
        printf(" %s", c_str(buff));

        cstr_long(buff, item->total, 7);
        printf(" %s", c_str(buff));

        printf("\n");

        total += item->total;
    }

    print("total : %ld kB / %d prc", total, count);
}

// Query ----------------------------------------------------------------------

bool prc_query()
{
    PrcListAuto *list = prlist_new();
    prlist_parse(list);
    prlist_print(list);

    return true;
}


