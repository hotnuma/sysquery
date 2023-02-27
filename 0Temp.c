#if 0

bool exists = false;
MemItem *memitem = mlist_get_item(list->memlist,
                                  c_str(item->name),
                                  &exists);

if (!exists)
{
    memitem->pid = item->pid;
    cstr_copy(memitem->uid_name, c_str(item->uid_name));
}

assert(memitem != NULL);

memitem->priv += item->priv;
memitem->shared += item->shared;
memitem->shared_huge += item->shared_huge;
memitem->swap += item->swap;

memitem->total += item->total;

memitem->count += 1;

// MemItem --------------------------------------------------------------------

typedef struct _MemItem MemItem;
MemItem* mitem_new(const char *name);
void mitem_free(MemItem *item);

// MemList --------------------------------------------------------------------

typedef struct _MemList MemList;
MemList* mlist_new();
void mlist_free(MemList *list);
int mlist_size(MemList *list);
MemItem* mlist_at(MemList *list, int i);
MemItem* mlist_get_item(MemList *list, const char *name, bool *exists);
void mlist_print(MemList *list);

// MemItem --------------------------------------------------------------------

struct _MemItem
{
    CString *name;

    int pid;
    CString *uid_name;

    long priv;
    long shared;
    long shared_huge;
    long swap;

    long total;

    int count;
};

MemItem* mitem_new(const char *name)
{
    MemItem *item = (MemItem*) malloc(sizeof(MemItem));

    item->name = cstr_new(name);

    item->pid = 0;
    item->uid_name = cstr_new_size(24);

    item->priv = 0;
    item->shared = 0;
    item->shared_huge = 0;
    item->swap = 0;

    item->total = 0;

    item->count = 0;

    return item;
}

void mitem_free(MemItem *item)
{
    if (!item)
        return;

    cstr_free(item->name);
    free(item);
}


// MemList --------------------------------------------------------------------

static int _mitem_compare_mem(void *entry1, void *entry2)
{
    MemItem *e1 = *((MemItem**) entry1);
    MemItem *e2 = *((MemItem**) entry2);

    return (e2->total - e1->total);
}

struct _MemList
{
    CList *list;

};

MemList* mlist_new()
{
    MemList *list = (MemList*) malloc(sizeof(MemList));

    list->list = clist_new(64, (CDeleteFunc) mitem_free);

    return list;
}

void mlist_free(MemList *list)
{
    if (!list)
        return;

    clist_free(list->list);
    free(list);
}

int mlist_size(MemList *list)
{
    return clist_size(list->list);
}

MemItem* mlist_at(MemList *list, int i)
{
    return (MemItem*) clist_at(list->list, i);
}

MemItem* mlist_get_item(MemList *list, const char *name, bool *exists)
{
    *exists = false;

    int size = clist_size(list->list);

    for (int i = 0; i < size; ++i)
    {
        MemItem *item = (MemItem*) clist_at(list->list, i);

        if (strcmp(c_str(item->name), name) == 0)
        {
            *exists = true;
            return item;
        }
    }

    MemItem *item = mitem_new(name);
    clist_append(list->list, item);

    return item;
}

void mlist_print(MemList *list)
{
    clist_sort(list->list, (CCompareFunc) _mitem_compare_mem);

    long total = 0;

    int size = mlist_size(list);

    for (int i = 0; i < size; ++i)
    {
        MemItem *item = mlist_at(list, i);

        if (item->count > 1)
        {
            print("%s(%d)\t%d\t%s\t%ld", c_str(item->name), item->count,
                                     item->pid,
                                     c_str(item->uid_name),
                                     item->total);
        }
        else
        {
            print("%s\t%d\t%s\t%ld", c_str(item->name),
                                 item->pid,
                                 c_str(item->uid_name),
                                 item->total);
        }

        total += item->total;
    }

    print("total : %ld", total);
}

// ----------------------------------------------------------------------------

#define SEP_TAB     "\t"
#define SEP_SPACE   " "
#define COL_UNIT    26
#define COL6        6
#define COL7        7

static bool writeHeaderTxt(CFile *outfile);
bool writeLineTxt(CFile *outfile);

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

bool SvcEntry::writeLineCsv(CFile &outfile)
{
    outfile << unit;
    outfile << SEP_TAB;
    outfile << user;
    outfile << SEP_TAB;
    outfile << pid;
    outfile << SEP_TAB;
    outfile << command;

    outfile << "\n";

    return true;
}

// ----------------------------------------------------------------------------

char buffer[1024];
unsigned int dummy;

CStringAuto *filename = cstr_new_size(32);
cstr_fmt(filename, "/proc/%d/status", item->pid);

FILE *file = fopen(c_str(filename), "r");

if (!file)
    return false;

while (fgets (buffer, sizeof(buffer), file) != NULL)
{
    // UIDs in order: real, effective, saved set, and filesystem
    if (sscanf(buffer, "Uid:\t%u\t%u\t%u\t%u",
               &dummy,
               &item->uid,
               &dummy,
               &dummy) == 1)
        break;
}

fclose (file);

// ----------------------------------------------------------------------------

if (exists)
{
    if (_have_pss)
    {
        memitem->shared += item->shared;
    }
    else if (memitem->shared < item->shared)
    {
        memitem->shared = item->shared;
    }

    if (memitem->shared_huge < item->shared_huge)
    {
        memitem->shared_huge = item->shared_huge;
    }
}
else
{
    memitem->shared = item->shared;
    memitem->shared_huge = item->shared_huge;
}

#define CMDSIZE 1024
char _cmdline[CMDSIZE];

static bool get_task_cmdline(char *result, int size, const char *filename)
{
    FILE *file = fopen (filename, "r");
    if (!file)
        return false;

    /* Read full command byte per byte until EOF */
    int i;
    int c;
    for (i = 0; (c = fgetc (file)) != EOF && i < (int) size - 1; i++)
        result[i] = (c == '\0') ? ' ' : (char) c;

    result[i] = '\0';

    if (result[i-1] == ' ')
        result[i-1] = '\0';

    fclose (file);

    /* Kernel processes don't have a cmdline nor an exec path */
    if (i == 0)
        return false;

    return true;
}

#endif


