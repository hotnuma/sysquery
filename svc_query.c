#include "svc_query.h"

#include "str_ext.h"

#include <cprocess.h>
#include <clist.h>
#include <cfile.h>
#include <libstr.h>
#include <libpath.h>
#include <print.h>

#include <ctype.h>

//#define SEP_TAB     "\t"
//#define SEP_SPACE   " "
//#define COL_UNIT    26
//#define COL6        6
//#define COL7        7


// SvcEntry -------------------------------------------------------------------

typedef struct _SvcEntry SvcEntry;
SvcEntry* svce_new();
void svce_free(SvcEntry *entry);
bool svce_parse(SvcEntry *entry, CString *line);

//static bool writeHeaderTxt(CFile *outfile);
//bool writeLineTxt(CFile *outfile);

// SvcList --------------------------------------------------------------------

typedef struct _SvcList SvcList;
SvcList* svcl_new();
void svcl_free(SvcList *list);
bool svcl_query(SvcList *list);
bool svcl_print(SvcList *list);


// SvcEntry -------------------------------------------------------------------

struct _SvcEntry
{
    CString *unit;
    CString *sub;
    CString *description;
};

SvcEntry* svce_new()
{
    SvcEntry *entry = malloc(sizeof(SvcEntry));

    entry->unit = cstr_new_size(32);
    entry->sub = cstr_new_size(32);
    entry->description = cstr_new_size(32);

    return entry;
}

void svce_free(SvcEntry *entry)
{
    if (!entry)
        return;

    cstr_free(entry->unit);
    cstr_free(entry->sub);
    cstr_free(entry->description);

    free(entry);
}

bool svce_parse(SvcEntry *entry, CString *line)
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
            if (strcmp("exited", part) != 0)
                cstr_copy(entry->sub, part);
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

#if 0
bool SvcEntry::writeHeaderTxt(CFile &outfile)
{
    CString col;

    col = "Unit";
    strPadRight(col, COL_UNIT, ' ');
    outfile << col;

    outfile << SEP_SPACE;

    col = "Sub";
    strPadRight(col, COL7, ' ');
    outfile << col;

    outfile << SEP_SPACE;

    outfile << "Description";

    outfile << "\n";

    return true;
}

bool SvcEntry::writeLineTxt(CFile &outfile)
{
    strEllipsize(unit, COL_UNIT, "+");
    strPadRight(unit, COL_UNIT, ' ');
    outfile << unit;

    outfile << SEP_SPACE;

    strEllipsize(sub, COL7, "+");
    strPadRight(sub, COL7, ' ');
    outfile << sub;

    outfile << SEP_SPACE;

    outfile << description;

    outfile << "\n";

    return true;
}

//bool SvcEntry::writeLineCsv(CFile &outfile)
//{
//    outfile << unit;
//    outfile << SEP_TAB;
//    outfile << user;
//    outfile << SEP_TAB;
//    outfile << pid;
//    outfile << SEP_TAB;
//    outfile << command;

//    outfile << "\n";

//    return true;
//}

#endif

// SvcList --------------------------------------------------------------------

struct _SvcList
{
    CList *entryList;
};

SvcList* svcl_new()
{
    SvcList *list = malloc(sizeof(SvcList));

    list->entryList = clist_new(64, (CDeleteFunc) svce_free);

    return list;
}

void svcl_free(SvcList *list)
{
    if (!list)
        return;

    clist_free(list->entryList);

    free(list);
}

#define SvcListAuto GC_CLEANUP(_freeSvcList) SvcList
GC_UNUSED static inline void _freeSvcList(SvcList **obj)
{
    svcl_free(*obj);
}

bool svcl_query(SvcList *list)
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

        SvcEntry *entry = svce_new();

        if (!svce_parse(entry, line))
        {
            svce_free(entry);
            continue;
        }

        print(c_str(entry->unit));

        clist_append(list->entryList, entry);
    }

    return true;
}

bool svcl_print(SvcList *list)
{
    CStringAuto *buff = cstr_new_size(64);

    int size = clist_size(list->entryList);

    for (int i = 0; i < size; ++i)
    {
        SvcEntry *entry = (SvcEntry*) clist_at(list->entryList, i);

        cstr_ellipsize(buff, c_str(entry->unit), 17);
        printf("%s", c_str(buff));

        cstr_ellipsize(buff, c_str(entry->sub), 17);
        printf(" %s", c_str(buff));

        cstr_ellipsize(buff, c_str(entry->description), 17);
        printf(" %s", c_str(buff));

        printf("\n");
    }

    return true;
}

bool svc_query()
{
    SvcListAuto *list = svcl_new();

    svcl_query(list);
    svcl_print(list);

    return true;
}


