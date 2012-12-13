/*
 * Copyright (c) 2006-2012 Douglas Gilbert.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sg_lib.h"

#define _WIN32_WINNT 0x0602
#include "sg_pt_win32.h"

/*
 * This is called when the '--wscan' option is given to sdparm. It is
 * Win32 only code and shows the relationship between various device names
 * and volumes in Windows OSes (Windows 2000, 2003, XP and Vista). There is
 * an optional scsi adapter scan.
 */

#define MAX_SCSI_ELEMS 1024
#define MAX_ADAPTER_NUM 64
#define MAX_PHYSICALDRIVE_NUM 512
#define MAX_CDROM_NUM 512
#define MAX_TAPE_NUM 512
#define MAX_HOLE_COUNT 8

// IOCTL_STORAGE_QUERY_PROPERTY

#define FILE_DEVICE_MASS_STORAGE    0x0000002d
#define IOCTL_STORAGE_BASE          FILE_DEVICE_MASS_STORAGE
#define FILE_ANY_ACCESS             0

// #define METHOD_BUFFERED             0

#define IOCTL_STORAGE_QUERY_PROPERTY \
    CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)


#ifndef _DEVIOCTL_
typedef enum _STORAGE_BUS_TYPE {
    BusTypeUnknown      = 0x00,
    BusTypeScsi         = 0x01,
    BusTypeAtapi        = 0x02,
    BusTypeAta          = 0x03,
    BusType1394         = 0x04,
    BusTypeSsa          = 0x05,
    BusTypeFibre        = 0x06,
    BusTypeUsb          = 0x07,
    BusTypeRAID         = 0x08,
    BusTypeiScsi        = 0x09,
    BusTypeSas          = 0x0A,
    BusTypeSata         = 0x0B,
    BusTypeSd           = 0x0C,
    BusTypeMmc          = 0x0D,
    BusTypeVirtual             = 0xE,
    BusTypeFileBackedVirtual   = 0xF,
    BusTypeMax,
    BusTypeMaxReserved  = 0x7F
} STORAGE_BUS_TYPE, *PSTORAGE_BUS_TYPE;

typedef struct _STORAGE_DEVICE_DESCRIPTOR {
    ULONG Version;
    ULONG Size;
    UCHAR DeviceType;
    UCHAR DeviceTypeModifier;
    BOOLEAN RemovableMedia;
    BOOLEAN CommandQueueing;
    ULONG VendorIdOffset;       /* 0 if not available */
    ULONG ProductIdOffset;      /* 0 if not available */
    ULONG ProductRevisionOffset;/* 0 if not available */
    ULONG SerialNumberOffset;   /* -1 if not available ?? */
    STORAGE_BUS_TYPE BusType;
    ULONG RawPropertiesLength;
    UCHAR RawDeviceProperties[1];
} STORAGE_DEVICE_DESCRIPTOR, *PSTORAGE_DEVICE_DESCRIPTOR;
#endif

typedef struct _STORAGE_DEVICE_UNIQUE_IDENTIFIER {
    ULONG  Version;
    ULONG  Size;
    ULONG  StorageDeviceIdOffset;
    ULONG  StorageDeviceOffset;
    ULONG  DriveLayoutSignatureOffset;
} STORAGE_DEVICE_UNIQUE_IDENTIFIER, *PSTORAGE_DEVICE_UNIQUE_IDENTIFIER;

// Use CompareStorageDuids(PSTORAGE_DEVICE_UNIQUE_IDENTIFIER duid1, duid2)
// to test for equality

#ifndef _DEVIOCTL_
typedef enum _STORAGE_QUERY_TYPE {
    PropertyStandardQuery = 0,
    PropertyExistsQuery,
    PropertyMaskQuery,
    PropertyQueryMaxDefined
} STORAGE_QUERY_TYPE, *PSTORAGE_QUERY_TYPE;

typedef enum _STORAGE_PROPERTY_ID {
    StorageDeviceProperty = 0,
    StorageAdapterProperty,
    StorageDeviceIdProperty,
    StorageDeviceUniqueIdProperty,
    StorageDeviceWriteCacheProperty,
    StorageMiniportProperty,
    StorageAccessAlignmentProperty
} STORAGE_PROPERTY_ID, *PSTORAGE_PROPERTY_ID;

typedef struct _STORAGE_PROPERTY_QUERY {
    STORAGE_PROPERTY_ID PropertyId;
    STORAGE_QUERY_TYPE QueryType;
    UCHAR AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY, *PSTORAGE_PROPERTY_QUERY;
#endif


/////////////////////////////////////////////////////////////////////////////

union STORAGE_DEVICE_DESCRIPTOR_DATA {
    STORAGE_DEVICE_DESCRIPTOR desc;
    char raw[256];
};

union STORAGE_DEVICE_UID_DATA {
    STORAGE_DEVICE_UNIQUE_IDENTIFIER desc;
    char raw[512];
};

struct storage_elem {
    char    name[32];
    char    volume_letters[32];
    int qp_descriptor_valid;
    int qp_uid_valid;
    union STORAGE_DEVICE_DESCRIPTOR_DATA qp_descriptor;
    union STORAGE_DEVICE_UID_DATA qp_uid;
};


static struct storage_elem * storage_arr;
static int next_unused_elem = 0;
static int verbose = 0;


static char *
get_err_str(DWORD err, int max_b_len, char * b)
{
    LPVOID lpMsgBuf;
    int k, num, ch;

    if (max_b_len < 2) {
        if (1 == max_b_len)
            b[0] = '\0';
        return b;
    }
    memset(b, 0, max_b_len);
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf, 0, NULL );
    num = lstrlen((LPCTSTR)lpMsgBuf);
    if (num < 1)
        return b;
    num = (num < max_b_len) ? num : (max_b_len - 1);
    for (k = 0; k < num; ++k) {
        ch = *((LPCTSTR)lpMsgBuf + k);
        if ((ch >= 0x0) && (ch < 0x7f))
            b[k] = ch & 0x7f;
        else
            b[k] = '?';
    }
    return b;
}

static const char *
get_bus_type(int bt)
{
    switch (bt)
    {
    case BusTypeUnknown:
        return "Unkno";
    case BusTypeScsi:
        return "Scsi ";
    case BusTypeAtapi:
        return "Atapi";
    case BusTypeAta:
        return "Ata  ";
    case BusType1394:
        return "1394 ";
    case BusTypeSsa:
        return "Ssa  ";
    case BusTypeFibre:
        return "Fibre";
    case BusTypeUsb:
        return "Usb  ";
    case BusTypeRAID:
        return "RAID ";
    case BusTypeiScsi:
        return "iScsi";
    case BusTypeSas:
        return "Sas  ";
    case BusTypeSata:
        return "Sata ";
    case BusTypeSd:
        return "Sd   ";
    case BusTypeMmc:
        return "Mmc  ";
    case BusTypeVirtual:
        return "Virt ";
    case BusTypeFileBackedVirtual:
        return "FBVir";
    case BusTypeMax:
        return "Max  ";
    default:
        return "_unkn";
    }
}

static int
query_dev_property(HANDLE hdevice,
                   union STORAGE_DEVICE_DESCRIPTOR_DATA * data)
{
    DWORD num_out, err;
    char b[256];
    STORAGE_PROPERTY_QUERY query = {StorageDeviceProperty,
                                    PropertyStandardQuery, {0} };

    memset(data, 0, sizeof(*data));
    if (! DeviceIoControl(hdevice, IOCTL_STORAGE_QUERY_PROPERTY,
                          &query, sizeof(query), data, sizeof(*data),
                          &num_out, NULL)) {
        if (verbose > 2) {
            err = GetLastError();
            fprintf(stderr, "  IOCTL_STORAGE_QUERY_PROPERTY(Devprop) failed, "
                    "Error=%ld %s\n", err, get_err_str(err, sizeof(b), b));
        }
        return -ENOSYS;
    }

    if (verbose > 3)
        fprintf(stderr, "  IOCTL_STORAGE_QUERY_PROPERTY(DevProp) "
                "num_out=%ld\n", num_out);
    return 0;
}

static int
query_dev_uid(HANDLE hdevice,
                    union STORAGE_DEVICE_UID_DATA * data)
{
    DWORD num_out, err;
    char b[256];
    STORAGE_PROPERTY_QUERY query = {StorageDeviceUniqueIdProperty,
                                    PropertyStandardQuery, {0} };

    memset(data, 0, sizeof(*data));
    if (! DeviceIoControl(hdevice, IOCTL_STORAGE_QUERY_PROPERTY,
                          &query, sizeof(query), data, sizeof(*data),
                          &num_out, NULL)) {
        if (verbose > 2) {
            err = GetLastError();
            fprintf(stderr, "  IOCTL_STORAGE_QUERY_PROPERTY(DevUid) failed, "
                    "Error=%ld %s\n", err, get_err_str(err, sizeof(b), b));
        }
        return -ENOSYS;
    }
    if (verbose > 3)
        fprintf(stderr, "  IOCTL_STORAGE_QUERY_PROPERTY(DevUid) num_out=%ld\n",
                num_out);
    return 0;
}

static int
check_devices(const struct storage_elem * sep)
{
    int k, j;
    struct storage_elem * sarr = storage_arr;

    for (k = 0; k < next_unused_elem; ++k, ++sarr) {
        if ('\0' == sarr->name[0])
            continue;
        if (sep->qp_descriptor_valid && sarr->qp_descriptor_valid) {
            if (0 == memcmp(&sep->qp_descriptor, &sarr->qp_descriptor,
                            sizeof(sep->qp_descriptor))) {
                for (j = 0; j < (int)sizeof(sep->volume_letters); ++j) {
                    if ('\0' == sarr->volume_letters[j]) {
                        sarr->volume_letters[j] = sep->name[0];
                        break;
                    }
                }
                return 1;
            }
        }
        // should do uid check here (probably before descriptor compare)
    }
    return 0;
}

static int
enum_scsi_adapters(void)
{
    int k, j;
    int hole_count = 0;
    HANDLE fh;
    ULONG dummy;
    DWORD err;
    BYTE bus;
    BOOL success;
    char adapter_name[64];
    char inqDataBuff[2048];
    PSCSI_ADAPTER_BUS_INFO  ai;
    char b[256];

    for (k = 0; k < MAX_ADAPTER_NUM; ++k) {
        snprintf(adapter_name, sizeof (adapter_name), "\\\\.\\SCSI%d:", k);
        fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (fh != INVALID_HANDLE_VALUE) {
            hole_count = 0;
            success = DeviceIoControl(fh, IOCTL_SCSI_GET_INQUIRY_DATA,
                                      NULL, 0, inqDataBuff,
                                      sizeof(inqDataBuff), &dummy, FALSE);
            if (success) {
                PSCSI_BUS_DATA pbd;
                PSCSI_INQUIRY_DATA pid;
                int num_lus, off;

                ai = (PSCSI_ADAPTER_BUS_INFO)inqDataBuff;
                for (bus = 0; bus < ai->NumberOfBusses; bus++) {
                    pbd = ai->BusData + bus;
                    num_lus = pbd->NumberOfLogicalUnits;
                    off = pbd->InquiryDataOffset;
                    for (j = 0; j < num_lus; ++j) {
                        if ((off < (int)sizeof(SCSI_ADAPTER_BUS_INFO)) ||
                            (off > ((int)sizeof(inqDataBuff) -
                                    (int)sizeof(SCSI_INQUIRY_DATA))))
                            break;
                        pid = (PSCSI_INQUIRY_DATA)(inqDataBuff + off);
                        snprintf(b, sizeof(b) - 1, "SCSI%d:%d,%d,%d ", k,
                                 pid->PathId, pid->TargetId, pid->Lun);
                        printf("%-15s", b);
                        snprintf(b, sizeof(b) - 1, "claimed=%d pdt=%xh %s ",
                                 pid->DeviceClaimed,
                                 pid->InquiryData[0] % 0x3f,
                                 ((0 == pid->InquiryData[4]) ? "dubious" :
                                                               ""));
                        printf("%-26s", b);
                        printf("%.8s  %.16s  %.4s\n", pid->InquiryData + 8,
                               pid->InquiryData + 16, pid->InquiryData + 32);
                        off = pid->NextInquiryDataOffset;
                    }
                }
            } else {
                err = GetLastError();
                fprintf(stderr, "%s: IOCTL_SCSI_GET_INQUIRY_DATA failed "
                        "err=%lu\n\t%s",
                        adapter_name, err, get_err_str(err, sizeof(b), b));
            }
            CloseHandle(fh);
        } else {
            if (verbose > 3) {
                err = GetLastError();
                fprintf(stderr, "%s: CreateFile failed err=%lu\n\t%s",
                        adapter_name, err, get_err_str(err, sizeof(b), b));
            }
            if (++hole_count >= MAX_HOLE_COUNT)
                break;
        }
    }
    return 0;
}

static int
enum_volumes(char letter)
{
    int k;
    HANDLE fh;
    char adapter_name[64];
    struct storage_elem tmp_se;

    if (verbose > 2)
        fprintf(stderr, "%s: enter\n", __FUNCTION__ );
    for (k = 0; k < 24; ++k) {
        memset(&tmp_se, 0, sizeof(tmp_se));
        snprintf(adapter_name, sizeof (adapter_name), "\\\\.\\%c:", 'C' + k);
        tmp_se.name[0] = 'C' + k;
        fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (fh != INVALID_HANDLE_VALUE) {
            if (query_dev_property(fh, &tmp_se.qp_descriptor) < 0)
                fprintf(stderr, "%s: query_dev_property failed\n",
                        __FUNCTION__ );
            else
                tmp_se.qp_descriptor_valid = 1;
            if (query_dev_uid(fh, &tmp_se.qp_uid) < 0) {
                if (verbose > 2)
                    fprintf(stderr, "%s: query_dev_uid failed\n",
                            __FUNCTION__ );
            } else
                tmp_se.qp_uid_valid = 1;
            if (('\0' == letter) || (letter == tmp_se.name[0]))
                check_devices(&tmp_se);
            CloseHandle(fh);
        }
    }
    return 0;
}

static int
enum_pds(void)
{
    int k;
    int hole_count = 0;
    HANDLE fh;
    DWORD err;
    char adapter_name[64];
    char b[256];
    struct storage_elem tmp_se;

    if (verbose > 2)
        fprintf(stderr, "%s: enter\n", __FUNCTION__ );
    for (k = 0; k < MAX_PHYSICALDRIVE_NUM; ++k) {
        memset(&tmp_se, 0, sizeof(tmp_se));
        snprintf(adapter_name, sizeof (adapter_name),
                 "\\\\.\\PhysicalDrive%d", k);
        snprintf(tmp_se.name, sizeof(tmp_se.name), "PD%d", k);
        fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (fh != INVALID_HANDLE_VALUE) {
            if (query_dev_property(fh, &tmp_se.qp_descriptor) < 0)
                fprintf(stderr, "%s: query_dev_property failed\n",
                        __FUNCTION__ );
            else
                tmp_se.qp_descriptor_valid = 1;
            if (query_dev_uid(fh, &tmp_se.qp_uid) < 0) {
                if (verbose > 2)
                    fprintf(stderr, "%s: query_dev_uid failed\n",
                            __FUNCTION__ );
            } else
                tmp_se.qp_uid_valid = 1;
            hole_count = 0;
            memcpy(&storage_arr[next_unused_elem++], &tmp_se, sizeof(tmp_se));
            CloseHandle(fh);
        } else {
            if (verbose > 3) {
                err = GetLastError();
                fprintf(stderr, "%s: CreateFile failed err=%lu\n\t%s",
                        adapter_name, err, get_err_str(err, sizeof(b), b));
            }
            if (++hole_count >= MAX_HOLE_COUNT)
                break;
        }
    }
    return 0;
}

static int
enum_cdroms(void)
{
    int k;
    int hole_count = 0;
    HANDLE fh;
    DWORD err;
    char adapter_name[64];
    char b[256];
    struct storage_elem tmp_se;

    if (verbose > 2)
        fprintf(stderr, "%s: enter\n", __FUNCTION__ );
    for (k = 0; k < MAX_CDROM_NUM; ++k) {
        memset(&tmp_se, 0, sizeof(tmp_se));
        snprintf(adapter_name, sizeof (adapter_name), "\\\\.\\CDROM%d", k);
        snprintf(tmp_se.name, sizeof(tmp_se.name), "CDROM%d", k);
        fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (fh != INVALID_HANDLE_VALUE) {
            if (query_dev_property(fh, &tmp_se.qp_descriptor) < 0)
                fprintf(stderr, "%s: query_dev_property failed\n",
                        __FUNCTION__ );
            else
                tmp_se.qp_descriptor_valid = 1;
            if (query_dev_uid(fh, &tmp_se.qp_uid) < 0) {
                if (verbose > 2)
                    fprintf(stderr, "%s: query_dev_uid failed\n",
                            __FUNCTION__ );
            } else
                tmp_se.qp_uid_valid = 1;
            hole_count = 0;
            memcpy(&storage_arr[next_unused_elem++], &tmp_se, sizeof(tmp_se));
            CloseHandle(fh);
        } else {
            if (verbose > 3) {
                err = GetLastError();
                fprintf(stderr, "%s: CreateFile failed err=%lu\n\t%s",
                        adapter_name, err, get_err_str(err, sizeof(b), b));
            }
            if (++hole_count >= MAX_HOLE_COUNT)
                break;
        }
    }
    return 0;
}

static int
enum_tapes(void)
{
    int k;
    int hole_count = 0;
    HANDLE fh;
    DWORD err;
    char adapter_name[64];
    char b[256];
    struct storage_elem tmp_se;

    if (verbose > 2)
        fprintf(stderr, "%s: enter\n", __FUNCTION__ );
    for (k = 0; k < MAX_TAPE_NUM; ++k) {
        memset(&tmp_se, 0, sizeof(tmp_se));
        snprintf(adapter_name, sizeof (adapter_name), "\\\\.\\TAPE%d", k);
        snprintf(tmp_se.name, sizeof(tmp_se.name), "TAPE%d", k);
        fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (fh != INVALID_HANDLE_VALUE) {
            if (query_dev_property(fh, &tmp_se.qp_descriptor) < 0)
                fprintf(stderr, "%s: query_dev_property failed\n",
                        __FUNCTION__ );
            else
                tmp_se.qp_descriptor_valid = 1;
            if (query_dev_uid(fh, &tmp_se.qp_uid) < 0) {
                if (verbose > 2)
                    fprintf(stderr, "%s: query_dev_uid failed\n",
                            __FUNCTION__ );
            } else
                tmp_se.qp_uid_valid = 1;
            hole_count = 0;
            memcpy(&storage_arr[next_unused_elem++], &tmp_se, sizeof(tmp_se));
            CloseHandle(fh);
        } else {
            if (verbose > 3) {
                err = GetLastError();
                fprintf(stderr, "%s: CreateFile failed err=%lu\n\t%s",
                        adapter_name, err, get_err_str(err, sizeof(b), b));
            }
            if (++hole_count >= MAX_HOLE_COUNT)
                break;
        }
    }
    return 0;
}

static int
do_wscan(char letter, int show_bt, int scsi_scan)
{
    int k, j, n;
    struct storage_elem * sp;

    if (scsi_scan < 2) {
        k = enum_pds();
        if (k)
            return k;
        k = enum_cdroms();
        if (k)
            return k;
        k = enum_tapes();
        if (k)
            return k;
        k = enum_volumes(letter);
        if (k)
            return k;

        for (k = 0; k < next_unused_elem; ++k) {
            sp = storage_arr + k;
            if ('\0' == sp->name[0])
                continue;
            printf("%-7s ", sp->name);
            n = strlen(sp->volume_letters);
            if (0 == n)
                printf("        ");
            else if (1 == n)
                printf("[%s]     ", sp->volume_letters);
            else if (2 == n)
                printf("[%s]    ", sp->volume_letters);
            else if (3 == n)
                printf("[%s]   ", sp->volume_letters);
            else if (4 == n)
                printf("[%s]  ", sp->volume_letters);
            else
                printf("[%4s+] ", sp->volume_letters);
            if (sp->qp_descriptor_valid) {
                if (show_bt)
                    printf("<%s>  ",
                           get_bus_type(sp->qp_descriptor.desc.BusType));
                j = sp->qp_descriptor.desc.VendorIdOffset;
                if (j > 0)
                    printf("%s  ", sp->qp_descriptor.raw + j);
                j = sp->qp_descriptor.desc.ProductIdOffset;
                if (j > 0)
                    printf("%s  ", sp->qp_descriptor.raw + j);
                j = sp->qp_descriptor.desc.ProductRevisionOffset;
                if (j > 0)
                    printf("%s  ", sp->qp_descriptor.raw + j);
                j = sp->qp_descriptor.desc.SerialNumberOffset;
                if (j > 0)
                    printf("%s", sp->qp_descriptor.raw + j);
                printf("\n");
                if (verbose > 2)
                    dStrHex(sp->qp_descriptor.raw, 144, 0);
            } else
                printf("\n");
        }
    }

    if (scsi_scan) {
        if (scsi_scan < 2)
            printf("\n");
        enum_scsi_adapters();
    }
    return 0;
}


int
sg_do_wscan(char letter, int do_scan, int verb)
{
    int ret, show_bt, scsi_scan;

    verbose = verb;
    show_bt = (do_scan > 1);
    scsi_scan = (do_scan > 2) ? (do_scan - 2) : 0;
    storage_arr = calloc(sizeof(struct storage_elem) * MAX_SCSI_ELEMS, 1);
    if (storage_arr) {
        ret = do_wscan(letter, show_bt, scsi_scan);
        free(storage_arr);
    } else {
        fprintf(stderr, "Failed to allocate storage_arr on heap\n");
        ret = SG_LIB_SYNTAX_ERROR;
    }
    return ret;
}
