#include "mc_panel.h"

#define L_BOOT_POWER_UP     0
#define L_BOOT_CLEARED      1
#define L_BOOT_FILLED       2
#define L_BOOT_RUNNING      3

#define L_PROG_RUN_SIM      0   /* Running simulation, random blinkys at fast rate */
#define L_PROG_COUNTER      1
#define L_PROG_COUNTER_SLO  2
#define L_PROG_LAMPTEST     3
#define L_PROG_KNIGHT_RIDER 4
#define L_PROG_DEBUG        5
#define L_PROG_RUN_SIM2     0xC000 /* switches 14 and 15 up */

#define L_EO_CLK_SW_CSI     1   /* Emulation Option: Switches are only clocked in with CSL INT when set.
                                   When cleared (default behavior), switches take immediate effect. */

struct s_cpu_state cpu_state;
int m_boot_program;
int m_first_tick;               // Set for first step through each program

void cpu_init()
{
  cpu_state.boot_state = L_BOOT_POWER_UP;
  
  cpu_state.panel_status_1 = 0;
  cpu_state.panel_status_2 = 0;

  m_boot_program = L_PROG_RUN_SIM;
}

void cpu_pre_execute()
{  
  // Examines the panel to set the current CPU state
  static byte db;

  // Fetch hard-wired switch states
  cpu_state.hw_bits = 0;
  cpu_state.hw_bits |= panel_read_hw(L_HALT);
  cpu_state.hw_bits |= panel_read_hw(L_MCLEAR);
  cpu_state.hw_bits |= panel_read_hw(L_CSLINT);
  cpu_state.hw_bits |= panel_read_hw(L_BPHLT);

  // Latch in inputs
  panel_write(L_REG_OUT_LATCH, 0);

  // Load switches into R0
  if ( (cpu_state.emu_options & L_EO_CLK_SW_CSI) == 0 || (cpu_state.hw_bits & L_HALT) )
  {
    // Default behavior: immediately load R0
    // Also do so if halted
    cpu_state.r[0] = panel_read(L_REG_R0_LO) | ( panel_read(L_REG_R0_HI) << 8 );
  }
  else
  {
    // Deferred behavior: Only load R0 while CSL INT active

    if ( cpu_state.hw_bits & L_CSLINT )
      cpu_state.r[0] = panel_read(L_REG_R0_LO) | ( panel_read(L_REG_R0_HI) << 8 );
  }
    
  // Grab register selection & context
  db = panel_read(L_REG_DISPLAY);
  cpu_state.panel_ctxt    = db & 0x0f;        // Context (iop/mbc/ctxt/map)
  cpu_state.panel_reg_sel = (db & 0xf0) >> 4; // Display select (eau/cpu switches)
  
  // Snapshot CPU control word and display selection
  db = panel_read(L_REG_CPU_CTRL);
  cpu_state.panel_ccn  = db & 0x0f;         // CPU Control Nibble (register 4 low word)
  cpu_state.panel_dspn = (db & 0xf0) >> 4;  // Display Nibble (mem/ints/psw/istk)

  // Now deal with CPU control if so selected
  if ( cpu_state.hw_bits & L_HALT )
  {
    // CPU is halted, look for things to do
    cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_RUN);

    if ( cpu_state.hw_bits & L_MCLEAR )
    {
      // M CLEAR has been depressed, clear out state
      cpu_state.boot_state = L_BOOT_CLEARED;

      panel_write_word(L_REG_ADDR_LO, 1);  // "Sets" PR to 1
      panel_write_word(L_REG_DATA_LO, 0);      
      //panel_write_word(L_REG_STAT_LO, 16);

      cpu_state.panel_status_2 = 0; // Clear NZOC, MERR, etc.
    }

    if ( cpu_state.panel_ccn == L_CC_FILL )
    {
      cpu_state.boot_state = L_BOOT_FILLED;

      // Grab switches and select program
      m_boot_program = cpu_state.r[0];
      m_first_tick   = 1;

      // Update our pattern
      panel_write_word(L_REG_ADDR_LO, 1);
      panel_write_word(L_REG_DATA_LO, m_boot_program | 0xae8);
      //panel_write_word(L_REG_STAT_LO, 32);
    }
    else if ( cpu_state.panel_ccn == L_CC_SINGLE_STEP )
    {
      if (cpu_state.panel_ccdb != L_CC_SINGLE_STEP ) // rising edge filter
      {
        // Allow 1 step through (this will get reset the next time around)
        cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_RUN);
      }
    }
    else if ( cpu_state.panel_ccn == L_CC_ENT_REG )
    {
      if (cpu_state.panel_ccdb != L_CC_ENT_REG)
      {
        // ENT REG was pressed. Clock in the switches into emu_options register
        cpu_state.emu_options = panel_read_word(L_REG_R0_LO);
      }
    }

    cpu_display_halt();
  }
  else
  {
    // CPU is running
    cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_RUN);

    if ( cpu_state.boot_state == L_BOOT_FILLED )
    {      
      cpu_state.boot_state = L_BOOT_RUNNING;
    }
  }

  // Update RUN status
  panel_write(L_REG_STAT_HI, cpu_state.panel_status_2);

  cpu_state.panel_ccdb = cpu_state.panel_ccn;
}

void cpu_display_halt()
{
  // Invoked when CPU is in a HALT state. Decides what should be displayed on the panel.

  // panel_ctxt:    iop /mbc /ctxt/map
  // panel_reg_sel: eau /eau /cpu /cpu
  // panel_dspn:    mem /ints/psw /istk

  // mem: show memory
  // ints: show ints (not used)
  // psw: show program status word (not used)
  // istk: show some sort of stack? (not used)

  if ( cpu_state.panel_dspn & 0x01 )
  {
    // Showing Memory
  }
  else
  {
    // Showing Register

    panel_write(L_REG_ADDR_LO, cpu_state.panel_reg_sel & 0x0f );
    panel_write(L_REG_DATA_LO, cpu_state.r[ cpu_state.panel_reg_sel & 0x0f ]);
  }
}

void cpu_execute()
{  
  // Runs next instruction if state calls for it
  if ( cpu_state.boot_state == L_BOOT_RUNNING &&
       TSTBIT(cpu_state.panel_status_2, L_RB_RUN) )
  {
    if (m_boot_program == L_PROG_RUN_SIM ||
        m_boot_program == L_PROG_RUN_SIM2)
    {
      prog_run_sim();
    }
    else if (m_boot_program == L_PROG_COUNTER)
    {
      prog_counter(0);
    }
    else if (m_boot_program == L_PROG_LAMPTEST)
    {
      prog_lamp_test();
    }
    else if (m_boot_program == L_PROG_COUNTER_SLO)
    {
      prog_counter(1000);
    }
    else if (m_boot_program == L_PROG_KNIGHT_RIDER)
    {
      prog_knight_rider();
    }
    else if (m_boot_program == L_PROG_DEBUG)
    {
      prog_debug();
    }
    else
    {
      // Throw an MERR in protest
      cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_M_ERR);
    }

    cpu_state.pr++;

    m_first_tick = 0;
  }
  else if ( m_boot_program == L_PROG_DEBUG )
  {
    // Always run the debugger
    prog_debug();
  }
}

void cpu_display_output()
{
  // n.b.: Not yet used  
  int data;
  
  // Address is always PR
  panel_write_word(L_REG_ADDR_LO, cpu_state.pr & 0x00ff);

  // Data depends upon context
  if (cpu_state.panel_ctxt & 1) // MEM
  {
    // Show contents of memory at PR
    data = 0; // TODO
  }
  else
  {
    // Show contents of register for now
    data = cpu_state.r[cpu_state.panel_reg_sel];
  }

  panel_write_word(L_REG_DATA_LO, data);

  // Build up status
  data = 0;
  data |= ( cpu_state.panel_ccn & 4 ) ? 8 : 0; // RUN
  // TODO: NZOC

  panel_write(L_REG_STAT_HI, data);
}


// ********************************************************
// Begin simulation "programs"
//
// Note that it is generally OK to write directly to panel
// registers, however the higher status word should be
// set through CPU state, as pre_execute writes that out
// based on the current RUN setting.
//
// Switches should be accessed via r[0], to conform to
// emulation options set by user.
//
// TODO: Refactor so all regs are set via cpu_state


// Simulate a running MODCOMP computer
void prog_run_sim()
{
  // R2: Simulated Instruction
  // R3: Scratch Area
  
  cpu_state.r[2] = rand_int();
  
  if (cpu_state.r[2] >= 32000)
  {
    // Simulate a "jump"
    cpu_state.pr = rand_int();
  }
  else if (cpu_state.r[2] >= 15000)
  {
    // Simulate a loop
    cpu_state.r[3] = abs(rand_int()) / 100;

    if (cpu_state.r[3] < cpu_state.pr)
    {
      cpu_state.pr -= cpu_state.r[3];
    }
  }
  
  panel_write_word(L_REG_ADDR_LO, cpu_state.pr);
  panel_write_word(L_REG_DATA_LO, cpu_state.r[2]);

  cpu_state.r[3] = rand_int();

  cpu_state.panel_status_2 = (cpu_state.panel_status_2 & 0x0f) | (cpu_state.r[3] & 0xf0);

  // If zero, make sure negative is clear
  if (TSTBIT(cpu_state.panel_status_2, L_RB_Z))
  {
    cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_N);
    cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_O);
    cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_C);
  }

  cpu_state.r[3] = (cpu_state.r[0] & 0x3fff);

  delay((unsigned long)cpu_state.r[3] * 10L);  
}


// Simple Counter
void prog_counter(int delay_count)
{
  // Get parameters from switches
  cpu_state.r[1] = (cpu_state.r[0] & 0x00ff) + 1;
  cpu_state.r[2] = ((cpu_state.r[0] & 0xff00) >> 8) + 1;

  // Accumulate
  cpu_state.r[3] += cpu_state.r[1];
  
  panel_write_word(L_REG_ADDR_LO, cpu_state.r[3]);
  
  cpu_state.r[4] -= cpu_state.r[2];
  
  panel_write_word(L_REG_DATA_LO, cpu_state.r[4]);
  
  if (delay_count > 0)
  {
    delay(delay_count);
  }  
}


// Lamp Test
void prog_lamp_test()
{
  // Locals:
  //   R1 - Cyclic counter for blinking based on any switch set
  
  cpu_state.r[1]++;

  if (cpu_state.r[1] < 100)
  {
    panel_write_word(L_REG_ADDR_LO,0xffff);
    panel_write_word(L_REG_DATA_LO,0xffff);
    panel_write_word(L_REG_STAT_LO,0xffff);
    cpu_state.panel_status_2 = 0xff;
  }
  else if (cpu_state.r[1] < 200)
  {
    if (cpu_state.r[0] != 0)
    {      
      panel_write_word(L_REG_ADDR_LO,0x0000);
      panel_write_word(L_REG_DATA_LO,0x0000);
      panel_write_word(L_REG_STAT_LO,0x0000);
      cpu_state.panel_status_2 = 0;
    }
  }
  else
  {
    cpu_state.r[1] = 0;
  }  
}


// Larson Scanner
void prog_knight_rider()
{
  // Locals:
  //    R1:0 - Direction flag (0=right, 1=left)
  //    R2   - Left-most bit in effect
  //    R3   - Maximum of effect, factoring in NZOC flag
  //    R4   - Rendered effect for data
  //    R5   - Rendered effect for NZOC (lower 4 bits only)
  //    R6   - Interator
  //    R15  - Effect length from switches

#define FLAGS  cpu_state.r[1]  // Bit 0 = Direction Bit, Bits 1-15 unused
#define EFFSTB cpu_state.r[2]  // EFFect STart Bit
#define EFFMAX cpu_state.r[3]  // EFFect MAXimum
#define DSPDAT cpu_state.r[4]  // DiSPlay DATa
#define DSPNZO cpu_state.r[5]  // DiSPlay data on NZOc
#define I      cpu_state.r[6]  // Iterator
#define EFFLEN cpu_state.r[15] // Effect length from switches

  if (m_first_tick)
  {
    EFFSTB = 0;
    panel_write_word(L_REG_ADDR_LO, 0);
  }
              
  //static int dir = 0;

  // Extract scan size from switches 8-14
  EFFLEN = ((cpu_state.r[0] & 0x7f00) >> 8); // Bits 8-14

  // Take off 1 LED for each increase of scan_size
  EFFMAX = 15 - EFFLEN;

  if (cpu_state.r[0] & 0x8000) // use_nzoc?
  {
    EFFMAX += 4;
  }

  // Render the effect in R4:R5
  DSPDAT = 0;
  DSPNZO = 0;

  for (I = EFFSTB; I <= EFFSTB + EFFLEN; I++)
  {
    if (I <= 15)
    {
      // Still in R4
      DSPDAT |= (1 << I);
    }
    else
    {
      // Now in R5
      DSPNZO |= (1 << (I - 16));
    }
  }

  panel_write_word(L_REG_DATA_LO, DSPDAT);

  cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_N);      
  cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_Z);      
  cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_O);      
  cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_C);          

  if (cpu_state.r[0] & 0x8000) // use_nzoc?
  {
    if (DSPNZO & 1)
      cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_N);

    if (DSPNZO & 2)
      cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_Z);
  
    if (DSPNZO & 4)
      cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_O);

    if (DSPNZO & 8)
      cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_C);

    panel_write(L_REG_STAT_HI, cpu_state.panel_status_2);
  }

  if ((FLAGS & 0x01) == 0) // Moving Right
  {
    if (EFFSTB >= EFFMAX || EFFSTB < 0)
    {
      FLAGS = SETBIT(FLAGS, 0); // Change dir to left
      EFFSTB--;
    }
    else
    {
      EFFSTB++;
    }
  }
  else /* Moving Left */
  {
    if (EFFSTB <= 0)
    {
      FLAGS = CLRBIT(FLAGS, 0); // Change dir to right
      EFFSTB++;
    }
    else if (EFFSTB > EFFMAX)
    {
      // Overflowed
      FLAGS = CLRBIT(FLAGS, 0); // Change dir to right
      EFFSTB = EFFMAX;
    }
    else
    {
      EFFSTB--;
    }
  }

  delay( (cpu_state.r[0] & 0x00ff) * 10); // Switches 0-7 x10

#undef EFFSTB
#undef EFFLEN
#undef EFFMAX
#undef DSPDAT
#undef DSPNZO
#undef I
#undef FLAGS
}



// Debug program used to read any register from the panel
void prog_debug()
{
  // Locals:
  //   R1 - Address from CPU/EAU
  //   R2 - Displayed switch state
  //   R3 - Data as read from address
  
  //r1: static int addr;
  //r2: static int addr_disp;
  //r3: static int data;
  
  /* clock switches into output buffers */
  panel_write(7, 0);

  // Get read address from CPU/EAU
  cpu_state.r[1] = (panel_read(5) >> 5) & 0x07;

  // Add in switch state
  cpu_state.r[2] = cpu_state.r[1];

  if (cpu_state.hw_bits & L_MCLEAR) cpu_state.r[2] |= 16;
  if (cpu_state.hw_bits & L_HALT)   cpu_state.r[2] |= 32;
  if (cpu_state.hw_bits & L_BPHLT)  cpu_state.r[2] |= 64;
  if (cpu_state.hw_bits & L_CSLINT) cpu_state.r[2] |= 128;
  
  panel_write(L_REG_ADDR_LO, cpu_state.r[2]); // AAA_ SSSS

  cpu_state.r[3] = panel_read(cpu_state.r[1]);
  panel_write(L_REG_DATA_LO, cpu_state.r[3]); // Echo data to data lo byte
}
