// abort, calloc, free
#include <stdlib.h>

// fwprintf, swprintf, stderr
#include <stdio.h>

// wchar_t
#include <wchar.h>

#include <windows.h>

#include "os-helpers-winapi.h"

wchar_t *
pds_os_helpers_make_wcs_from_mbs (const char *mbs)
{
    if (!mbs)
    {
        return 0;
    }

    int size = MultiByteToWideChar (CP_UTF8, 0, mbs, -1, 0, 0);

    if (!size)
    {
        return 0;
    }

    wchar_t *buf = calloc (size, sizeof (wchar_t));

    if (__builtin_expect (!buf, 0))
    {
        fwprintf (stderr, L"memory allocation error\n");
        abort ();
    }

    int size2 = MultiByteToWideChar (CP_UTF8, 0, mbs, -1, buf, size);

    if (!size2 || buf[size - 1])
    {
        free (buf);

        return 0;
    }

    return buf;
}

char *
pds_os_helpers_make_mbs_from_wcs (const wchar_t *wcs)
{
    if (!wcs)
    {
        return 0;
    }

    int size = WideCharToMultiByte (CP_UTF8, 0, wcs, -1, 0, 0, 0, 0);

    if (!size)
    {
        return 0;
    }

    char *buf = calloc (size, sizeof (char));

    if (__builtin_expect (!buf, 0))
    {
        fwprintf (stderr, L"memory allocation error\n");
        abort ();
    }

    int size2 = WideCharToMultiByte (CP_UTF8, 0, wcs, -1, buf, size, 0, 0);

    if (!size2 || buf[size - 1])
    {
        free (buf);

        return 0;
    }

    return buf;
}

wchar_t *
pds_os_helpers_strerror (int error_code)
{
    int size = 128 * 1024;
    wchar_t *buf = calloc (size, sizeof (wchar_t));

    if (__builtin_expect (!buf, 0))
    {
        fwprintf (stderr, L"memory allocation error\n");
        abort ();
    }

    int format_status = FormatMessageW (
            FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
            0,
            error_code,
            MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
            buf,
            size,
            0
            );

    if (!format_status)
    {
        int l = swprintf (buf, size, L"error_code=%d", error_code);

        if (l == -1 || l == size)
        {
            fwprintf (stderr, L"something has gone wrong\n");
            abort ();
        }
    }

    return buf;
}

// vi:ts=4:sw=4:et
