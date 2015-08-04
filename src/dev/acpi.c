#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/errno.h>

#include <arch/i386.h>
#include <attrs.h>
#include <cosec/log.h>

/*
 *    See for the _S5_ hack:
 *    http://forum.osdev.org/viewtopic.php?t=16990
 */

#define ACPI_TBL_LEN_OFF  1

#define FADT_SIGNITURE    0x50434146
#define _S5_AML_BYTECODE  0x5f35535f

typedef struct __packed {
    union { char signc[4]; uint32_t signi; };
    uint32_t len;
    uint8_t rev;
    uint8_t checksum;
    char oem_id[6];
    char oemtable_id[8];
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
} rsdt_hdr_t;

typedef struct __packed {
    char signiture[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    rsdt_hdr_t *rsdt;
} rsdp_t;

typedef struct __packed {
    rsdt_hdr_t hdr;
    uint32_t firmware_ctrl;
    uint32_t dsdt;

    uint8_t rsrvd1;

    uint8_t preffered_powermgmt_profile;
    uint16_t sci_intr;
    uint32_t smi_cmdport;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_ctrl;
    uint32_t pm1a_eventblock;
    uint32_t pm1b_eventblock;
    uint32_t pm1a_ctrlblock;
    uint32_t pm1b_ctrlblock;
    uint32_t pm2_ctrlblock;
    uint32_t pm_timerblock;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t  pm1_eventlen;
    uint8_t  pm1_ctrllen;
    uint8_t  pm2_ctrllen;
    uint8_t  pm_timerlen;
    /* and so on... */
} fadt_t;

struct {
    rsdp_t      *rsdp; 
    rsdt_hdr_t  *rsdt;
    fadt_t      *fadt;
    uint32_t    *dsdt;
} theAcpi;

static int acpi_lookup_rsdp(void) {
    ptr_t p;
    for (p = 0xe0000; p <= 0xffff0; p += 0x10) {
        uint32_t *pi = (uint32_t *)p;
        if (pi[0] == 0x20445352 && pi[1] == 0x20525450) {
            theAcpi.rsdp = (rsdp_t *)pi;
            theAcpi.rsdt = theAcpi.rsdp->rsdt;
            return 0;
        }
    }
    return 1;
}

static char * acpi_scan_dsdt_for_S5_(char *dsdt, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        char *cptr = dsdt + i;
        if (!memcmp(cptr, "_S5_", 4)) {
            return cptr;
        }
    }
    return NULL;
}

int acpi_init(void) {
    int i, ret;

    return_log_if(theAcpi.rsdt, 0, "%s: already initialized\n", __FUNCTION__);

    /* look for RSDP pointer structure in BIOS */
    ret = acpi_lookup_rsdp();
    return_msg_if(ret, ret, "%s: acpi_lookup_rsdp() failed(%d)\n", __FUNCTION__, ret);
    logmsgf("%s: RSDP = *%x\n", __FUNCTION__, (uint)theAcpi.rsdp);
    logmsgf("%s: RSDT = *%x\n", __FUNCTION__, (uint)theAcpi.rsdt);
    return_msg_if(theAcpi.rsdt->signi != (uint32_t)0x54445352,
                  -EINVAL, "%s: invalid RSDT signiture", __FUNCTION__);

    /* RSDP checksum */
    uint8_t ck = 0;
    rsdt_hdr_t *rsdt = theAcpi.rsdp->rsdt;
    for (i = 0; i < (int)rsdt->len; ++i)
        ck += ((uint8_t *)rsdt)[i];
    return_msg_if(ck, -EINVAL, "%s: invalid RDST checksum\n", __FUNCTION__);

    theAcpi.rsdt = rsdt;

    /* Enumerate tables */
    uint32_t table_name[2] = { 0 };
    ptr_t *acpi_tables = (ptr_t *)((char *)rsdt + sizeof(rsdt_hdr_t));
    for (i = 0; (char *)(acpi_tables + i) < (char *)rsdt + rsdt->len; ++i) {
        uint32_t *table = (uint32_t *)acpi_tables[i];
        table_name[0] = table[0];
        logmsgif("%s: found %s at *%p", __FUNCTION__, (char *)table_name, (uint)table);

        if (table[0] == FADT_SIGNITURE) {
            theAcpi.fadt = (fadt_t *)table;
            theAcpi.dsdt = (uint32_t *)theAcpi.fadt->dsdt;
            logmsgif("%s: DSDT at *%p", __FUNCTION__, theAcpi.dsdt);
        }
    }

    return 0;
}

int acpi_poweroff(void) {
    int ret;
    
    ret = acpi_init();
    return_msg_if(ret, ret, "%s: acpi_init() failed(%d)", __FUNCTION__, ret);
    return_msg_if(theAcpi.fadt == NULL, -EINVAL, "%s: no FADT", __FUNCTION__);

    char *s5addr = acpi_scan_dsdt_for_S5_((char *)theAcpi.dsdt,
                                          theAcpi.dsdt[ACPI_TBL_LEN_OFF]);
    return_msg_if(!s5addr, -EINVAL, "%s: _S5_ bytecode not found", __FUNCTION__);

    s5addr += 4;
    return_msg_if(s5addr[0] != 0x12, -EINVAL, "%s: weird _S5_", __FUNCTION__);
    ++s5addr;

    uint8_t s5pkglen = ((s5addr[0] & 0xc0) >> 6) + 2;
    s5addr += s5pkglen;

    if (s5addr[0] == 0x0a) ++s5addr;  // skip byteprefix
    const uint16_t SLP_TYPa = (uint16_t)s5addr[0] << 10;

    ++s5addr;
    if (s5addr[0] == 0x0a) ++s5addr;  // skip byteprefix
    const uint16_t SLP_TYPb = (uint16_t)s5addr[0] << 10;

    const uint16_t pm1a_ctrlport = theAcpi.fadt->pm1a_ctrlblock;
    const uint16_t pm1b_ctrlport = theAcpi.fadt->pm1b_ctrlblock;
    const uint16_t SLP_EN = 1 << 13;

    logmsgif("%s: outw(0x%x, 0x%x)", __FUNCTION__, pm1a_ctrlport, SLP_TYPa | SLP_EN);

    outw(pm1a_ctrlport, SLP_TYPa | SLP_EN);
    if (pm1b_ctrlport)
        outw(pm1b_ctrlport, SLP_TYPb | SLP_EN);

    return 0;
}
