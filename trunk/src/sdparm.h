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

/* Mode page numbers */
#define RW_ERR_RECOVERY_MP 1
#define DISCONNECT_MP 2
#define FORMAT_MP 3
#define RIGID_DISK_MP 4
#define WRITE_PARAM_MP 5
#define RBC_DEV_PARAM_MP 6
#define V_ERR_RECOVERY_MP 7
#define CACHING_MP 8
#define CONTROL_MP 0xa
#define DATA_COMPR_MP 0xf
#define DEV_CONF_MP 0x10
#define ES_MAN_MP 0x14
#define PROT_SPEC_LU_MP 0x18
#define PROT_SPEC_PORT_MP 0x19
#define POWER_MP 0x1a
#define IEC_MP 0x1c
#define TIMEOUT_PROT_MP 0x1d
#define XOR_MP 0x10

/* Mode subpage numbers */
#define MSP_SPC_CE 1
#define MSP_SPI_MC 1
#define MSP_SPI_STC 2
#define MSP_SPI_NS 3
#define MSP_SPI_RTC 4
#define MSP_SAS_PCD 1

#define MODE_DATA_OVERHEAD 128
#define EBUFF_SZ 256
#define MAX_MP_IT_VAL 128
#define MAX_MODE_DATA_LEN 2048

/* VPD pages (fetched by INQUIRY command) */
#define VPD_SUPPORTED_VPDS 0x0
#define VPD_UNIT_SERIAL_NUM 0x80
#define VPD_DEVICE_ID 0x83
#define VPD_MAN_NET_ADDR 0x85
#define VPD_EXT_INQ 0x86
#define VPD_SCSI_PORTS 0x88
#define VPD_ASSOC_LU 0
#define VPD_ASSOC_TPORT 1
#define VPD_ASSOC_TDEVICE 2

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

/* bit flag settings for sdparm_mode_page_item::flags */
#define MF_COMMON 0x1   /* output in summary mode */
#define MF_HEX 0x2


struct sdparm_opt_coll {
    int all;
    int mode_6;
    int defaults;
    int dummy;
    int enumerate;
    int hex;
    int inquiry;
    int long_out;
    int saved;
    int flexible;
    int transport;
    int dbd;
};

struct sdparm_values_name_t {
    int value;
    int subvalue;
    int pdt;         /* peripheral device type id, -1 is the default */
                     /* (not applicable) value */
    int read_only;
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
};

struct sdparm_mode_page_it_val {
    struct sdparm_mode_page_item mpi;
    long long val;
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

extern struct sdparm_values_name_t sdparm_gen_mode_pg[];
extern struct sdparm_values_name_t sdparm_vpd_pg[];
extern struct sdparm_values_name_t sdparm_transport_id[];
extern struct sdparm_transport_pair sdparm_transport_mp[];
extern struct sdparm_mode_page_item sdparm_mitem_arr[];
extern const char * sdparm_scsi_ptype_strs[];
extern const char * sdparm_transport_proto_arr[];
extern const char * sdparm_code_set_arr[];
extern const char * sdparm_assoc_arr[];
extern const char * sdparm_id_type_arr[];
extern const char * sdparm_ansi_version_arr[];

#endif
