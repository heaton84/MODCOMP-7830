/*
 * mc_panel.c
 * 
 * A MODCOMP Control Panel Controller
 * Version 1.0             12/04/2020
 * 
 * Revision History
 * 12/08/2020       Added to github, added 2nd option for program 0
 * 
 * Target:  Arduino Nano
 * 
 * I/O Map:
 * 
 * NANO  DESC         MODCOMP
 * D12 = Data 4     = 02
 * D11 = Data 3     = 04
 * D10 = Data 2     = 06
 * D09 = Data 1     = 08
 * D08 = Data 6     = 10
 * D07 = Data 5     = 12
 * D06 = Data 7     = 14
 * D05 = Data 8     = 16
 * D04 = RS 0       = 18
 * D03 = RS 2       = 20
 * D02 = RS 1       = 22
 * D13 = CLOCK      = 24
 * A00 = READ/WRITE = 30
 * A01 = HALT       = 34
 * A02 = M CLEAR    = 36
 * A03 = CSL INT    = 38
 * A04 = BL-HLT     = 40
 * A05 = unused
 * A06 = unused (I ONLY)
 * A07 = unused (I ONLY)
 */

#if defined(ARDUINO_AVR_NANO)
#else
#error Unsupported hardware - Firmware targets Arduino Nano ONLY at this time.
#endif

#include "mc_panel.h"

// Arduino I/O Pin Mapping
#define P_D1            9
#define P_D2            10
#define P_D3            11
#define P_D4            12
#define P_D5            7
#define P_D6            8
#define P_D7            6
#define P_D8            5
#define P_RS0           4
#define P_RS1           2
#define P_RS2           3
#define P_CLOCK         A0
#define P_RW            13
#define P_HALT          A1
#define P_MCLEAR        A2
#define P_CSLINT        A3
#define P_BPHLT         A4

// Logicals
#define L_READ          HIGH
#define L_WRITE         LOW

// Dwell time: msec to wait for I/O transfer
#define L_IO_DWELL      1

//
// Set direction of data bus
// dir: INPUT or OUTPUT
//
void panel_set_data_dir(int dir)
{
  pinMode( P_D1, dir );
  pinMode( P_D2, dir );
  pinMode( P_D3, dir );
  pinMode( P_D4, dir );
  pinMode( P_D5, dir );
  pinMode( P_D6, dir );
  pinMode( P_D7, dir );
  pinMode( P_D8, dir );
}

//
// Write input register on panel
//
void panel_write(byte addr, byte data)
{
  // 1. Assert that CLOCK is logic high
  digitalWrite( P_CLOCK, HIGH );

  // 2. For now, R/W is tied low
  digitalWrite( P_RW   , L_WRITE );

  // 3. Set up address
  digitalWrite( P_RS0, (addr & 0x01) ? HIGH : LOW );
  digitalWrite( P_RS1, (addr & 0x02) ? HIGH : LOW );
  digitalWrite( P_RS2, (addr & 0x04) ? HIGH : LOW );

  // 4. Load data onto pins
  digitalWrite( P_D1,  (data & 0x01) ? LOW : HIGH );
  digitalWrite( P_D2,  (data & 0x02) ? LOW : HIGH );
  digitalWrite( P_D3,  (data & 0x04) ? LOW : HIGH );
  digitalWrite( P_D4,  (data & 0x08) ? LOW : HIGH );
  digitalWrite( P_D5,  (data & 0x10) ? LOW : HIGH );
  digitalWrite( P_D6,  (data & 0x20) ? LOW : HIGH );
  digitalWrite( P_D7,  (data & 0x40) ? LOW : HIGH );
  digitalWrite( P_D8,  (data & 0x80) ? LOW : HIGH );

  // 5. Strobe clock low for a tick
  digitalWrite( P_CLOCK, LOW );
  delay( L_IO_DWELL );
  digitalWrite( P_CLOCK, HIGH );
}

void panel_write_word(byte addr, int data)
{
  // Macro to write lo byte at addr, hi byte at addr+1

  byte db = data & 0x00ff;

  panel_write(addr + 0, db);

  db = (data & 0xff00) >> 8;

  panel_write(addr + 1, db);
}

//
// Read output register on panel
// Remember to write to address 7 before reading.
// That clocks in all input buffers.
//
byte panel_read(byte addr)
{
  static byte accum;

  // 1. Assert that CLOCK is logic high
  digitalWrite( P_CLOCK, HIGH );

  // 2. Ask for read from panel
  digitalWrite( P_RW   , L_READ  );  

  // 3. Set up address
  digitalWrite( P_RS0, (addr & 0x01) ? HIGH : LOW );
  digitalWrite( P_RS1, (addr & 0x02) ? HIGH : LOW );
  digitalWrite( P_RS2, (addr & 0x04) ? HIGH : LOW );

  panel_set_data_dir( INPUT );

  // 4. Clock in data
  digitalWrite( P_CLOCK, LOW );  

  delay( L_IO_DWELL ); // Let lines stabilize

  accum = 0;
  accum |= (digitalRead( P_D1 ) == HIGH ? 0x01 : 0x00);
  accum |= (digitalRead( P_D2 ) == HIGH ? 0x02 : 0x00);
  accum |= (digitalRead( P_D3 ) == HIGH ? 0x04 : 0x00);
  accum |= (digitalRead( P_D4 ) == HIGH ? 0x08 : 0x00);
  accum |= (digitalRead( P_D5 ) == HIGH ? 0x10 : 0x00);
  accum |= (digitalRead( P_D6 ) == HIGH ? 0x20 : 0x00);
  accum |= (digitalRead( P_D7 ) == HIGH ? 0x40 : 0x00);
  accum |= (digitalRead( P_D8 ) == HIGH ? 0x80 : 0x00);

  digitalWrite( P_CLOCK, HIGH );

  panel_set_data_dir( OUTPUT );

  return accum;
}

int  panel_read_word(byte addr)
{
  // Macro to read a word starting at addr as lo byte
  int dw = panel_read(addr);

  dw |= (panel_read(addr + 1) << 8);

  return dw;
}

int  panel_read_hw(int hw)
{
  if (hw == L_MCLEAR)
    return digitalRead( P_MCLEAR ) == LOW ? L_MCLEAR : 0;

  if (hw == L_HALT)
    return digitalRead( P_HALT   ) == LOW ? L_HALT : 0;
  
  if (hw == L_BPHLT)
    return digitalRead( P_BPHLT  ) == LOW ? L_BPHLT : 0;
  
  if (hw == L_CSLINT)
    return digitalRead( P_CSLINT ) == LOW ? L_CSLINT : 0;

  return 0;
}

int rand_int()
{
  // Only exists due to arduino shenanigans during development
  return random(65536) - 32768;
}

//
// Arduino Boot Routine
//
void setup() {

  panel_init();
  cpu_init();
  
  randomSeed(analogRead(7));

  // Perform lamp test
  panel_write_word(L_REG_ADDR_LO,0xffff);
  panel_write_word(L_REG_DATA_LO,0xffff);
  panel_write_word(L_REG_STAT_LO,0xffff);
  delay(100);
  panel_write_word(L_REG_ADDR_LO,0x0000);
  panel_write_word(L_REG_DATA_LO,0x0000);
  panel_write_word(L_REG_STAT_LO,0x0000);  
}

void panel_init()
{
  pinMode( P_CLOCK,  OUTPUT );
  pinMode( P_RW   ,  OUTPUT );

  pinMode( P_RS0,    OUTPUT );
  pinMode( P_RS1,    OUTPUT );
  pinMode( P_RS2,    OUTPUT );

  pinMode( P_HALT,   INPUT );
  pinMode( P_MCLEAR, INPUT );
  pinMode( P_CSLINT, INPUT );
  pinMode( P_BPHLT,  INPUT );

  panel_set_data_dir( OUTPUT );  

  // Clear out LEDs
  panel_write(L_REG_ADDR_LO, 0);
  panel_write(L_REG_ADDR_HI, 0);
  panel_write(L_REG_DATA_LO, 0);
  panel_write(L_REG_DATA_HI, 0);
  panel_write(L_REG_STAT_LO, 0);
  panel_write(L_REG_STAT_HI, 0);  
}

void loop() {

  static int sim_addr;

  static byte lo_addr, hi_addr, hi_stat;
  static byte dat, dat2;
  static byte sw;
  static byte sw_w;
  static byte toggle = 0;

  cpu_pre_execute();
  cpu_execute();
  return;

#define READ_TEST 1

#if READ_TEST
  /* clock switches into output buffers */
  panel_write(7, 0);

  // Get read address from CPU/EAU
  dat = (panel_read(5) >> 5) & 0x07;

  // Add in switch state
  dat2 = dat;
  if (digitalRead(P_MCLEAR)==LOW) dat2 |= 16;
  if (digitalRead(P_HALT)==LOW)   dat2 |= 32;
  if (digitalRead(P_BPHLT)==LOW)  dat2 |= 64;
  if (digitalRead(P_CSLINT)==LOW) dat2 |= 128;
  
  panel_write(L_REG_ADDR_LO, dat2);
  //write_panel(L_REG_ADDR_HI, 0);

  dat = panel_read(dat);
  panel_write(L_REG_DATA_LO, dat);
  //write_panel(L_REG_DATA_HI, 0);

  // Testing O LED  
  panel_write(L_REG_STAT_LO, 0);

  dat2 = 0xf0;

  if (digitalRead(P_HALT)==HIGH) dat2 |= 8;
  
  panel_write(L_REG_STAT_HI, dat2);

  if (toggle == 0)
    toggle = 0xff;
  //else
  //  toggle = 0;

  if (digitalRead(P_CSLINT) == LOW)
  {
    // Clock contents of R0(LOW) into address
    sw = panel_read(2);
    panel_write(dat, sw);
  }
#endif

  delay(100);
}
