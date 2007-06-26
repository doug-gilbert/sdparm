#ifndef SDPARM_H
#define SDPARM_H

/* sdparm is a utility program for the Linux OS SCSI subsystem.
 *
 * This utility fetches various parameters associated with a given
 * SCSI disk (or a disk that uses, or translates the SCSI command
 * set). In some cases these parameters can be changed.
 */


#define DEF_MODE_RESP_LEN 252
#define DEF_INQ_RESP_LEN 252
#define VPD_ATA_INFO_RESP_LEN 572

/* Mode page numbers */
#define UNIT_ATTENTION_MP 0
#define RW_ERR_RECOVERY_MP 1
#define DISCONNECT_MP 2
#define FORMAT_MP 3
#define MRW_MP 3
#define RIGID_DISK_MP 4
#define WRITE_PARAM_MP 5
#define RBC_DEV_PARAM_MP 6
#define V_ERR_RECOVERY_MP 7
#define CACHING_MP 8
#define CONTROL_MP 0xa
#define POWER_OLD_MP 0xd
/* #define CD_DEV_PARAMS 0xd */
#define DATA_COMPR_MP 0xf
#define DEV_CONF_MP 0x10
#define XOR_MP 0x10
#define MED_PART_MP 0x11
#define ES_MAN_MP 0x14
#define PROT_SPEC_LU_MP 0x18
#define PROT_SPEC_PORT_MP 0x19
#define POWER_MP 0x1a
#define IEC_MP 0x1c
#define MED_CONF_MP 0x1d
#define TIMEOUT_PROT_MP 0x1d
#define MMCMS_MP 0x2a
#define ALL_MPAGES 0x3f

/* Mode subpage numbers */
#define MSP_CONTROL_EXT 1       /* unused */
#define MSP_SPC_CE 1            /* control extension */
#define MSP_SPI_MC 1
#define MSP_SPI_STC 2
#define MSP_SPI_NS 3
#define MSP_SPI_RTC 4
#define MSP_SAS_PCD 1
#define MSP_SAS_SHA 2
#define MSP_BACK_CTL 1
#define MSP_SAT_PATA 0xf1       /* SAT PATA Control */
#define MSP_DEV_CONF_EXT 1      /* device conf extension (ssc) */

#define MODE_DATA_OVERHEAD 128
#define EBUFF_SZ 256
#define MAX_MP_IT_VAL 128
#define MAX_MODE_DATA_LEN 2048

/* VPD pages (fetched by INQUIRY command) */
#define VPD_SUPPORTED_VPDS 0x0
#define VPD_UNIT_SERIAL_NUM 0x80
#define VPD_IMP_OP_DEF 0x81     /* obsolete in SPC-2 */
#define VPD_ASCII_OP_DEF 0x82   /* obsolete in SPC-2 */
#define VPD_DEVICE_ID 0x83
#define VPD_SOFTW_INF_ID 0x84
#define VPD_MAN_NET_ADDR 0x85
#define VPD_EXT_INQ 0x86
#define VPD_MODE_PG_POLICY 0x87
#define VPD_SCSI_PORTS 0x88
#define VPD_ATA_INFO 0x89
#define VPD_BLOCK_LIMITS 0xb0   /* SBC-3 */
#define VPD_SA_DEV_CAP 0xb0     /* SSC-3 */
#define VPD_MAN_ASS_SN 0xb1     /* SSC-3 */
#define VPD_TA_SUPPORTED 0xb2   /* SSC-3 */

#define VPD_ASSOC_LU 0
#define VPD_ASSOC_TPORT 1
#define VPD_ASSOC_TDEVICE 2

/* values are 2**vpd_assoc */
#define VPD_DI_SEL_LU 1
#define VPD_DI_SEL_TPORT 2
#define VPD_DI_SEL_TARGET 4
#define VPD_DI_SEL_AS_IS 32

/* Transport protocol identifiers */
#define TP_FCP 0
#define TP_SPI 1
#define TP_SSA 2
#define TP_1394 3
#define TP_SRP 4
#define TP_ISCSI 5
#define TP_SAS 6
#define TP_ADT 7
#define TP_ATA 8
#define TP_NONE 0xf

/* Vendor identifiers */
#define VENDOR_SEAGATE 0x0
#define VENDOR_HITACHI 0x1
#define VENDOR_MAXTOR 0x2

/* bit flag settings for sdparm_mode_page_item::flags */
#define MF_COMMON 0x1   /* output in summary mode */
#define MF_HEX 0x2

/* enumerations for commands */
#define CMD_READY 1
#define CMD_START 2
#define CMD_STOP 3
#define CMD_LOAD 4
#define CMD_EJECT 5
#define CMD_UNLOCK 6
#define CMD_SENSE 7
#define CMD_SYNC 8
#define CMD_CAPACITY 9


struct sdparm_opt_coll {
    int all;
    int dbd;
    int defaults;
    int dummy;
    int enumerate;
    int flexible;
    int hex;
    int inquiry;
    int long_out;
    int mode_6;
    int quiet;
    int save;
    int transport;
    int vendor;
};

struct sdparm_values_name_t {
    int value;
    int subvalue;
    int pdt;         /* peripheral device type id, -1 is the default */
                     /* (not applicable) value */
    int ro_vendor;   /* read-only or vendor flag */
    const char * acron;
    const char * name;
};

struct sdparm_mode_page_item {
    const char * acron;
    int page_num;
    int subpage_num;
    int pdt;         /* peripheral device type or -1 (default) if not */
                     /* applicable */
    int start_byte;
    int start_bit;
    int num_bits;
    int flags;       /* bit settings or-ed, see MF_* */
    const char * description;
    const char * extra;
};

struct sdparm_mode_page_it_val {
    struct sdparm_mode_page_item mpi;
    long long val;
    long long orig_val;
};

struct sdparm_mode_page_settings {
    int page_num;
    int subpage_num;
    struct sdparm_mode_page_it_val it_vals[MAX_MP_IT_VAL];
    int num_it_vals;
};

struct sdparm_transport_pair {
    struct sdparm_values_name_t * mpage;
    struct sdparm_mode_page_item * mitem;
};

struct sdparm_vendor_pair {
    struct sdparm_values_name_t * mpage;
    struct sdparm_mode_page_item * mitem;
};

struct sdparm_command {
    int cmd_num;
    const char * name;
};

extern struct sdparm_values_name_t sdparm_gen_mode_pg[];
extern struct sdparm_values_name_t sdparm_vpd_pg[];
extern struct sdparm_values_name_t sdparm_transport_id[];
extern struct sdparm_transport_pair sdparm_transport_mp[];
extern struct sdparm_values_name_t sdparm_vendor_id[];
extern struct sdparm_vendor_pair sdparm_vendor_mp[];
extern int sdparm_vendor_mp_len;
extern struct sdparm_mode_page_item sdparm_mitem_arr[];
extern struct sdparm_command sdparm_command_arr[];

extern const char * sdparm_pdt_doc_strs[];
extern const char * sdparm_transport_proto_arr[];
extern const char * sdparm_code_set_arr[];
extern const char * sdparm_assoc_arr[];
extern const char * sdparm_desig_type_arr[];
extern const char * sdparm_ansi_version_arr[];
extern const char * sdparm_network_service_type_arr[];
extern const char * sdparm_mode_page_policy_arr[];


/*
 * Declarations for access functions found in sdparm_access.c
 */

extern int sdp_get_mp_len(unsigned char * mp);
extern const struct sdparm_values_name_t * sdp_get_mode_detail(
                int page_num, int subpage_num, int pdt, int transp_proto,
                int vendor_num);
extern void sdp_get_mpage_name(int page_num, int subpage_num, int pdt,
                int transp_proto, int vendor_num, int plus_acron, int hex,
                char * bp, int max_b_len);
extern const struct sdparm_values_name_t * sdp_find_mp_by_acron(
                const char * ap, int transp_proto, int vendor_num);
const struct sdparm_values_name_t *
        sdp_get_vpd_detail(int page_num, int subvalue, int pdt);
extern const struct sdparm_values_name_t * sdp_find_vpd_by_acron(
                const char * ap);
extern const char * sdp_get_transport_name(int proto_num);
extern const struct sdparm_values_name_t * sdp_find_transport_by_acron(
                const char * ap);
extern const char * sdp_get_vendor_name(int vendor_num);
extern const struct sdparm_values_name_t * sdp_find_vendor_by_acron(
                const char * ap);
extern const struct sdparm_vendor_pair * sdp_get_vendor_pair(int vendor_num);
extern const struct sdparm_mode_page_item * sdp_find_mitem_by_acron(
                const char * ap, int * from, int transp_proto,
                int vendor_num);
extern unsigned long long sdp_get_big_endian(const unsigned char * from,
                int start_bit, int num_bits);
extern void sdp_set_big_endian(unsigned long long val, unsigned char * to,
                int start_bit, int num_bits);
extern unsigned long long sdp_mp_get_value(
                const struct sdparm_mode_page_item *mpi,
                const unsigned char * mp);
extern unsigned long long sdp_mp_get_value_check(
                const struct sdparm_mode_page_item *mpi,
                const unsigned char * mp, int * all_set);
extern void sdp_mp_set_value(unsigned long long val,
                struct sdparm_mode_page_item *mpi, unsigned char * mp);
extern char * sdp_get_ansi_version_str(int version, int buff_len,
                char * buff);
extern char * sdp_get_pdt_doc_str(int version, int buff_len,
                char * buff);

/*
 * Declarations for functions found in sdparm_vpd.c
 */

extern int sdp_process_vpd_page(int sg_fd, int pn, int spn,
                                const struct sdparm_opt_coll * opts,
                                int req_pdt, int verbose);

/*
 * Declarations for functions found in sdparm_cmd.c
 */

extern const struct sdparm_command * sdp_build_cmd(const char * cmd_str,
                int * rwp);
extern void sdp_enumerate_commands();
extern int sdp_process_cmd(int sg_fd, const struct sdparm_command * scmdp,
                int pdt, const struct sdparm_opt_coll * opts,
                int verbose);

/*
 * Declarations for functions that are port dependant
 */

#ifdef SDPARM_WIN32

extern int sg_do_wscan(char letter, int verbose);

#endif

#endif
