typedef dis_fcn (int instruction);
typedef exec_fcn (int instruction);

typedef struct
{
  uint16_t base;
  uint16_t mask;
  bool eis;
  char *mnemonic;
  dis_fcn_t *dis_fcn;
  exec_fcn_t *exec_fcn;
} inst_info_t;

inst_info_t inst_info [] =
{
  { 0x0000, 0xff80, false, "halt",   no_arg_dis,   halt_exec },
  { 0x0080, 0xff80, false, "pushf",  no_arg_dis,   pushf_exec },
  { 0x0100, 0xff80, false, "rti",    imm7_dis,     rti_exec },
  // no 0180
  { 0x0200, 0xff80, false, "rts",    imm7_dis,     rti_exec },
  { 0x0280, 0xff80, false, "pullf",  no_arg_dis,   pullf_exec },
  { 0x0300, 0xff80, false, "jsrp",   eis_ptr_dis,  jsrp_exec },
  { 0x0380, 0xff80, false, "jsri",   eis_ptr_dis,  jsri_exec },
  { 0x0400, 0xff80, false, "rin",    io_dis,       rin_exec },
  { 0x0480, 0xfcf0, true,  "mpy",    eis_d_dis,    mpy_exec },
  { 0x0490, 0xfcf0, true,  "div",    eis_d_dis,    div_exec },
  { 0x04a0, 0xfcf0, true,  "dadd",   eis_d_dis,    dadd_exec },
  { 0x04b0, 0xfcf0, true,  "dsub",   eis_d_dis,    dsub_exec },
  { 0x04c0, 0xfcf0, true,  "ldb",    eis_d_dis,    ldb_exec },
  { 0x04d0, 0xfcf0, true,  "stb",    eis_d_dis,    ldb_exec },
  // no 04e0, 04f0, 0500
  { 0x0510, 0xfff0, true,  "iscan",  no_arg_dis,   iscan_exec },
  { 0x0510, 0xfff0, true,  "jmpp",   eis_ptr_dis,  jmpp_exec },
  { 0x0520, 0xfff0, true,  "jint",   eis_jint_dis, jint_exec },
  // no 0530..05f0
  { 0x0600, 0xff80, false, "rout",   io_dis,       rout_exec },
  // no 0680
  { 0x0700, 0xfff0, true,  "setst",  eis_stf_dis,  setst_exec },
  { 0x0710, 0xfff0, true,  "clrst",  eis_stf_dis,  clrst_exec },
  { 0x0720, 0xfff0, true,  "setbit", eis_bit_dis,  setbit_exec },
  { 0x0730, 0xfff0, true,  "clrbit", eis_bit_dis,  clrbit_exec },
  { 0x0740, 0xfff0, true,  "skstf",  eis_stf_dis,  skstf_exec },
  { 0x0750, 0xfff0, true,  "skbit",  eis_bit_dis,  skbit_exec },
  { 0x0760, 0xfff0, true,  "cmpbit", eis_bit_dis,  cmpbit_exec },
  // no 0770..07f0
  { 0x0800, 0xf880, false, "sflg",   flag_dis,     sflg_exec },
  { 0x0880, 0xf880, false, "pflg",   flag_dis,     pflg_exec },
  { 0x1000, 0xf000, false, "boc",    boc_dis,            boc_exec },
  { 0x2000, 0xfc00, false, "jmp",    mem_ref_dis,        jmp_exec },
  { 0x2400, 0xfc00, false, "jmp",    mem_ref_ind_dis,    jmp_ind_exec },
  { 0x2800, 0xfc00, false, "jsr",    mem_ref_dis,        jsr_exec },
  { 0x2c00, 0xfc00, false, "jsr",    mem_ref_ind_dis,    jsr_ind_exec },
  { 0x3000, 0xf083, false, "radd",   reg_reg_dis,        radd_exec },
  // no 3001, 3002, 3003
  { 0x3080, 0xf083, false, "rxch",   reg_reg_dis,        rxch_exec },
  { 0x3081, 0xf083, false, "rcpy",   reg_reg_dis,        rcpy_exec },
  { 0x3082, 0xf083, false, "rxor",   reg_reg_dis,        rxor_exec },
  { 0x3083, 0xf083, false, "rand",   reg_reg_dis,        rand_exec },
  { 0x4000, 0xfc00, false, "push",   reg_dis,            push_exec },
  { 0x4400, 0xfc00, false, "pull",   reg_dis,            pull_exec },
  { 0x4800, 0xfc00, false, "aisz",   imm_dis,            aisz_exec },
  { 0x4c00, 0xfc00, false, "li",     imm_dis,            li_exec },
  { 0x5000, 0xfc00, false, "cai",    imm_dis,            cai_exec },
  { 0x5400, 0xfc00, false, "xchrs",  reg_dis,            xchrs_exec },
  { 0x5800, 0xfc00, false, "ro?",    rot_dis,            rot_exec },
  { 0x5c00, 0xfc00, false, "sh?",    shift_dis,          shift_exec },
  { 0x6000, 0xf800, false, "and",    mem_ref_r01_dis,    and_exec },
  { 0x6800, 0xf800, false, "or",     mem_ref_r01_dis,    or_exec },
  { 0x7000, 0xf800, false, "skaz",   mem_ref_r01_dis,    skaz_exec },
  { 0x7800, 0xfc00, false, "isz",    mem_ref_dis,        isz_exec },
  { 0x7c00, 0xfc00, false, "dsz",    mem_ref_dis,        dsz_exec },
  { 0x8000, 0xf000, false, "ld",     mem_ref_r_dis,      ld_exec },
  { 0x9000, 0xf000, false, "ld",     mem_ref_r_ind_dis,  ld_ind_exec },
  { 0xa000, 0xf000, false, "st",     mem_ref_r_dis,      st_exec },
  { 0xb000, 0xf000, false, "st",     mem_ref_r_ind_dis,  st_ind_exec },
  { 0xc000, 0xf000, false, "add",    mem_ref_r_dis,      add_exec },
  { 0xd000, 0xf000, false, "sub",    mem_ref_r_dis,      sub_exec },
  { 0xe000, 0xf000, false, "skg",    mem_ref_r_dis,      skg_exec },
  { 0xf000, 0xf000, false, "skne",   mem_ref_r_dis,      skne_exec }
};

radd
