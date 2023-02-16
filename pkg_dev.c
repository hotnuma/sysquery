#include "pkg_dev.h"

#include <cprocess.h>
#include <cfile.h>
#include <libstr.h>
#include <print.h>

#include <dirent.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>
//#include <unistd.h>
//#include <sys/types.h>

bool pkg_dev()
{
    const char *cmd = "dpkg -l";

    CProcessAuto *process = cprocess_new();
    if (!cprocess_start(process, cmd, CP_PIPEOUT))
    {
        print("start failed");

        return false;
    }

    int status = cprocess_exitstatus(process);

    if (status != 0)
    {
        print("program returned : %d", status);

        return false;
    }

    CString *result = cprocess_outbuff(process);
    const char *start = c_str(result);

    CStringAuto *line = cstr_new_size(32);

    while (file_getline(&start, line))
    {
        char *start = c_str(line);

        int count = 0;
        char *result;
        int len;

        while (str_getpart(&start, &result, &len))
        {
            if (++count < 2)
                continue;

            result[len] = '\0';

            char *coln = strchr(result, ':');
            if (coln)
                *coln = '\0';

            if (str_endswith(result, "-dev", true))
                print(result);

            break;
        }
    }

    return true;
}


