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

#define L_EO_CLK_SW_CSI     1   /* Emulation Option: Switches are only clocked in with CSL INT when set.
                                   When cleared (default behavior), switches take immediate effect. */

struct s_cpu_state cpu_state;
int m_boot_program;

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
    {
      cpu_state.r[0] = panel_read(L_REG_R0_LO) | ( panel_read(L_REG_R0_HI) << 8 );
    }
  }
    
  // Grab register selection & context
  db = panel_read(L_REG_DISPLAY);
  cpu_state.panel_ctxt    = db & 0x0f;
  cpu_state.panel_reg_sel = (db & 0xf0) >> 4;
  
  // Snapshot CPU control word and display selection
  db = panel_read(L_REG_CPU_CTRL);
  cpu_state.panel_ccn  = db & 0x0f;
  cpu_state.panel_dspn = (db & 0xf0) >> 4;

  // Now deal with CPU control if so selected
  if ( cpu_state.hw_bits & L_HALT )
  {
    // CPU is halted, look for things to do
    cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_RUN);

    if ( cpu_state.hw_bits & L_MCLEAR )
    {
      // M CLEAR has been depressed, clear out state
      cpu_state.boot_state = L_BOOT_CLEARED;

      panel_write_word(L_REG_ADDR_LO, 0);
      panel_write_word(L_REG_DATA_LO, 0);      
      panel_write_word(L_REG_STAT_LO, 0);

      cpu_state.panel_status_2 = 0; // Clear NZOC, MERR, etc.
    }

    if ( cpu_state.panel_ccn == L_CC_FILL )
    {
      cpu_state.boot_state = L_BOOT_FILLED;

      // Grab switches and select program
      m_boot_program = cpu_state.r[0];

      // Update our pattern
      panel_write_word(L_REG_ADDR_LO, 0);
      panel_write_word(L_REG_DATA_LO, m_boot_program | 0xae8);
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

void cpu_execute()
{  
  // Runs next instruction if state calls for it
  if ( cpu_state.boot_state == L_BOOT_RUNNING &&
       TSTBIT(cpu_state.panel_status_2, L_RB_RUN) )
  {
    if (m_boot_program == L_PROG_RUN_SIM)
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
  panel_write(L_REG_ADDR_LO, cpu_state.pr & 0xff);
  panel_write(L_REG_ADDR_HI, (cpu_state.pr & 0xff00) >> 8);

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

  panel_write(L_REG_DATA_LO, data & 0xff);
  panel_write(L_REG_DATA_HI, (data & 0xff00) >> 8);

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
  static int dw = 0;
  static int dw2 = 0;

  dw++;

  dw2 = rand_int();
  
  if (dw2 >= 30000)
  {
    // Simulate a "jump"
    dw = rand_int();
  }
  else if (dw2 >= 15000)
  {
    // Simulate a loop
    dw2 = rand_int() / 100;

    dw -= dw2;
  }
  
  panel_write_word(L_REG_ADDR_LO,dw);

  dw2 = rand_int();
  panel_write_word(L_REG_DATA_LO,dw2);

  dw2 = rand_int();

  cpu_state.panel_status_2 = (cpu_state.panel_status_2 & 0x0f) | (dw2 & 0xf0);

  // If zero, make sure negative is clear
  if (TSTBIT(cpu_state.panel_status_2, L_RB_Z))
  {
    cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_N);
    cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_O);
    cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_C);
  }

  dw2 = cpu_state.r[0];

  delay((unsigned long)dw2 * 10L);  
}


// Simple Counter
void prog_counter(int delay_count)
{
  static int dw = 0;
  static int dw2 = 0;
  static int dw3 = 0;
  
  dw2 = (cpu_state.r[0] & 0x00ff) + 1;
  
  dw += dw2;
  panel_write_word(L_REG_ADDR_LO,dw);
  
  dw2 = ((cpu_state.r[0] & 0xff00) >> 8) + 1;
  dw3 -= dw2;
  
  panel_write_word(L_REG_DATA_LO,dw3);
  
  if (delay_count > 0)
  {
    delay(delay_count);
  }  
}


// Lamp Test
void prog_lamp_test()
{
  static int dw = 0;
  
  dw++;

  if (dw < 100)
  {
    panel_write_word(L_REG_ADDR_LO,0xffff);
    panel_write_word(L_REG_DATA_LO,0xffff);
    panel_write_word(L_REG_STAT_LO,0xffff);
    cpu_state.panel_status_2 = 0xff;
  }
  else if (dw < 200)
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
    dw = 0;
  }  
}


// Larson Scanner
void prog_knight_rider()
{  
  static long kit = 1,          // Holds last position of scanner as bitmask
              kit_with_scan,    // Extends the position into the selected length/size
              min,              // Limits of effect
              max;
              
  static int dir = 0,           // Direction flag
             delay_time,
             scan_size = 1,
             use_nzoc = 0;

  const int C_DIR_RIGHT = 0,
            C_DIR_LEFT  = 1;

  delay_time = cpu_state.r[0] & 0x00ff;         // Bits 0-7
  scan_size = ((cpu_state.r[0] & 0x7f00) >> 8); // Bits 8-14
  use_nzoc = (cpu_state.r[0] & 0x8000);         // Bit 15

  min = 1;
  max = 32768 >> scan_size;  // Take off 1 LED for each increase of scan_size

  if (use_nzoc)
    max <<= 4;  // Add 4 LEDs if using N/Z/O/C

  kit_with_scan = kit;

  for (int i = 1; i <= scan_size; i++)
  {
    kit_with_scan |= (kit_with_scan << 1);
  }

  panel_write_word(L_REG_DATA_LO, kit_with_scan);

  cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_N);      
  cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_Z);      
  cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_O);      
  cpu_state.panel_status_2 = CLRBIT(cpu_state.panel_status_2, L_RB_C);          

  if (use_nzoc)
  {
    if (kit_with_scan & (1L << 16L))
      cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_N);

    if (kit_with_scan & (1L << 17L))
      cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_Z);
  
    if (kit_with_scan & (1L << 18L))
      cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_O);

    if (kit_with_scan & (1L << 19L))
      cpu_state.panel_status_2 = SETBIT(cpu_state.panel_status_2, L_RB_C);

    panel_write(L_REG_STAT_HI, cpu_state.panel_status_2);
  }

  if (dir == C_DIR_RIGHT)
  {
    if (kit >= max || kit <= 0)
    {
      dir = C_DIR_LEFT;
      kit >>= 1;
    }
    else
    {
      kit <<= 1;
    }
  }
  else /* C_DIR_LEFT */
  {
    if (kit <= min)
    {
      dir = C_DIR_RIGHT;
      kit <<= 1;
    }
    else
    {
      kit >>= 1;
    }
  }

  delay(delay_time * 10);
}


// Debug program used to read any register from the panel
void prog_debug()
{
  static int addr;
  static int addr_disp;
  static int data;
  
  /* clock switches into output buffers */
  panel_write(7, 0);

  // Get read address from CPU/EAU
  addr = (panel_read(5) >> 5) & 0x07;

  // Add in switch state
  addr_disp = addr;

  if (cpu_state.hw_bits & L_MCLEAR) addr_disp |= 16;
  if (cpu_state.hw_bits & L_HALT)   addr_disp |= 32;
  if (cpu_state.hw_bits & L_BPHLT)  addr_disp |= 64;
  if (cpu_state.hw_bits & L_CSLINT) addr_disp |= 128;
  
  panel_write(L_REG_ADDR_LO, addr_disp); // AAA_ SSSS

  data = panel_read(addr);
  panel_write(L_REG_DATA_LO, data); // Echo data to data lo byte

  // This is a bad idea...
  //if (cpu_state.hw_bits & L_CSLINT)
  //{
  //  // Clock contents of R0(LOW) into address
  //  data = cpu_state.r[0] & 0x0f;
  //  panel_write(addr, data);
  //}  
}
