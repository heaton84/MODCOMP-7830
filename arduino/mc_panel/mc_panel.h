#ifndef __MC_PANEL__

#define __MC_PANEL__ "1.0"

#include "arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

// Input Registers
#define L_REG_ADDR_LO   0
#define L_REG_ADDR_HI   1
#define L_REG_DATA_LO   2
#define L_REG_DATA_HI   3
#define L_REG_STAT_LO   4
#define L_REG_STAT_HI   5
#define L_REG_OUT_LATCH 7 /* output buffer latch data in, must be WRITTEN to */

// Output Registers
#define L_REG_R0_LO     2
#define L_REG_R0_HI     3
#define L_REG_CPU_CTRL  4 /* if halted, controls CPU */
#define L_REG_DISPLAY   5 /* Register Display Select */

// Register Bits
#define L_RB_II_PROT    0 /* INPUT REGISTER 4 */
#define L_RB_PRIV       1
#define L_RB_IO         2
#define L_RB_TASK       3
#define L_RB_EMA_1      4
#define L_RB_EMA_2      5
#define L_RB_EMA_3      6
#define L_RB_EMA_4      7

#define L_RB_VM         0 /* INPUT REGISTER 5 */
#define L_RB_PM         1
#define L_RB_M_ERR      2
#define L_RB_RUN        3
#define L_RB_N          4
#define L_RB_Z          5
#define L_RB_O          6
#define L_RB_C          7

/* OUTPUT REGISTER 0/1/3 IS R0 (SWITCHES), HI WORD. NO BITS MAPPED */
/* OUTPUT REGISTER 2     IS R0 (SWITCHES), LO WORD. NO BITS MAPPED */

#define L_RB_MEM        4 /* OUTPUT REGISTER 4 (HI NIBBLE), GRF SWITCHES */
#define L_RB_INTS       5
#define L_RB_PSW        6
#define L_RB_ISTK       7

#define L_RB_IOP        0 /* OUTPUT REGISTER 5/6/7, REGISTER DISPLAY SELECT SWITCHES */
#define L_RB_MBC        1
#define L_RB_CTXT       2
#define L_RB_MAP        3
#define L_RB_EAU_1      4
#define L_RB_EAU_2      5
#define L_RB_CPU_1      6
#define L_RB_CPU_2      7



// CPU Controls (when HALT active)
#define L_CC_MASK         0x0f
#define L_CC_SINGLE_STEP  0x00
#define L_CC_ENT_NXT_VOP  0x01 /* Virtual, Oper */
#define L_CC_ENT_NXT_ACT  0x02 /* Actual Instruction / Actual Oper */
#define L_CC_ENT_NXT_VIN  0x03 /* Virtual, Inst */
#define L_CC_IDLE         0x04
#define L_CC_ENT_PMA_VIN  0x05 /* Virtual, Inst */
#define L_CC_ENT_PMA_ACT  0x06 /* Actual */
/* Note: 0x07 undefined state */
#define L_CC_FILL         0x08
#define L_CC_ENT_MEM_VOP  0x09 /* Virtual, Oper */
#define L_CC_ENT_MEM_ACT  0x0A /* Actual Instruction / Actual Oper */
#define L_CC_ENT_MEM_VIN  0x0B /* Virtual, Inst */
#define L_CC_ENT_REG      0x0C
#define L_CC_STP_PMA_VIN  0x0D /* Virtual, Inst */
#define L_CC_STP_PMA_ACT  0x0E /* Actual */

// Hardwired statuses
#define L_HALT          1
#define L_MCLEAR        2
#define L_CSLINT        4
#define L_BPHLT         8

// CPU State Structure
struct s_cpu_state
{
  short r[16];          // R0=switches, R1..R15=unused for now (was going to write a CPU emulator...)
  short pr;             // Program Register (also unused for now)
  short state;          // NZOC,IO,TASK,MERR,RUN (also unused for now)

  short hw_bits;        // Hardwired status bits

  short panel_reg_sel;  // Selected register on panel
  short panel_ctxt;     // Map/IOP/MBC/CTXT context
  short panel_ccdb;     // CPU Control DeBounce word (1-shot)
  short panel_ccn;      // CPU Control Nibble
  short panel_dspn;     // display nibble

  short boot_state;     // Boot state of CPU

  // Registers for buffering output to display
  short panel_address;
  short panel_data;
  short panel_status_1; // Lo word of status
  short panel_status_2; // Hi word of status

  short emu_options;    // Emulation Options, clocked in by ENT REG during a HALT
};

// Useful Macros
#define SETBIT(w,b)  ( w | (1 << b) )
#define CLRBIT(w,b)  ( w & ~(1 << b) )
#define TSTBIT(w,b)  ( w & (1 << b) )

// mc_panel.ino
void panel_init();
void panel_set_data_dir(int dir);
void panel_write(byte addr, byte data);
void panel_write_word(byte addr, int data);
byte panel_read(byte addr);
int  panel_read_word(byte addr);
int  panel_read_hw(int hw);
int  rand_int();

// cpu.c
extern struct s_cpu_state cpu_state;
void cpu_init();
void cpu_pre_execute();
void cpu_execute();
void cpu_display_output();
void cpu_display_halt();

void prog_run_sim();
void prog_counter(int);
void prog_lamp_test();
void prog_knight_rider();

#ifdef __cplusplus
}
#endif


#endif
