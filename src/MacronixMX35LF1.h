/*
 * Macronix MX35LF1GE4AB-Z4I-TR
 * NAND FLASH library
 * Datasheet: https://mouser.com/datasheet/2/819/MX35LF1GE4AB_2c_3V_2c_1Gb_2c_v1_9-3371019.pdf
 * Dual/Quad IO will not be considered
*/

/* TODO:
 * Secure OTP (One-Time-Programmable) Feature
 * Internal ECC Enabled/Disabled Feature
 * Ignore Invalid Blocks
 * Check inappropriate inputs in functions
*/

#ifndef MacronixMX35LF1_H
#define MacronixMX35LF1_H

#include "MacronixMX35LF1_Registers.h"

#define CLOCK_FREQUENCY 80000000 // 80 MHz 

#define Block_To_Page(a)   ((int)(a * NUM_PAGES_PER_BLOCK)) // Convert the Block address to Page address
#define Page_To_Block(b)   ((int)(b / NUM_PAGES_PER_BLOCK)) // Convert the Page address to Block address

// enum FlashModels {
//     MX35LF, // This library is for this Flash Type, more specific the MX35LF1GE4AB-Z4I-TR
//     MX35UF
// };

class MX35LF
{
  private:
    uint8_t mx35CS;                    // Chip Select pin number
    int8_t _WPpin = -1;                // Pin to control the Write Protection (#WP)
    SPIClass *_spi;                    // The SPI-Device used
    bool debug_mode = false;           // Variable to enable the Debug in Serial Monitor
    uint8_t WP_status = LOW;           // Variable to storage the lastest value of WP status
    bool Have_Invalid_Blocks = false;  // TRUE when the FLASH have Bad blocks
    uint16_t Qnt_Invalid_Blocks = 0;   // The quantity of all bad blocks in the FLASH
    uint8_t Blocks_Invalids[20];       // Array to store all block address for ignore them

  public:
    MX35LF(uint8_t _CS);
    MX35LF(uint8_t _CS, SPIClass *_SPI);
    ~MX35LF() {};

    uint8_t begin(void);
    uint8_t begin(uint8_t _wp_pin);

    uint8_t mx35lf_init_check(void);
    void mx35lf_reset(void);

    uint8_t mx35lf_start_read_page(uint16_t page_address);
    uint8_t read_one_page_from_block(uint16_t page_address, uint8_t *buffer, uint32_t len,  \
                                     uint8_t wrap = FINAL_PAGE_ADDRESS_2048);
    // Dont Have: Read from cache  (x2) (0x3B)
    // Dont Have: Read from cache  (x4) (0x6B)
    // QE Bit disable
    uint8_t read_pages_from_cache(uint8_t *buffer, uint32_t len, uint16_t init_page,  \
                                  uint8_t final_page, uint8_t wrap = FINAL_PAGE_ADDRESS_2048);
    void read_page(uint8_t *buffer, uint32_t len, uint8_t WRAP_BIT, uint64_t init = 0);

    uint8_t LoadProgData(uint8_t wrap, uint16_t page_address, uint8_t *buffer, uint32_t len);
    uint8_t LoadRandProgData(uint8_t wrap, uint16_t page_address, uint8_t *buffer, uint32_t len);
    uint8_t ProgramExecute(uint16_t page_address);
    // Dont Have: PROGRAM LOAD (X4) (0x32)
    // Dont Have: QUAD IO PROGRAM RANDOM INPUT (0x34)

    uint8_t Block_Erase(uint16_t page_address);
    uint8_t Bulk_Erase(void);

    void Verify_Bad_Blocks(uint8_t *buffer, uint32_t buffer_size);

    uint8_t mx35lf_GET_Features(uint8_t address);
    void mx35lf_SET_features(uint8_t address, uint8_t value);

    void Write_Enable(void);
    void Write_Disable(void);

    void Parameter_Page(uint8_t *buffer);
    void UniqueID_Page(uint8_t *buffer);

    void Set_WP_pin(uint8_t _wp);
    void Enable_WP_pin(void);
    void Disable_WP_pin(void);

    void Set_Debug_Mode(bool _debug);

    uint8_t WaitOperationDone(void);

    /* 5 bits to be considered */
    uint8_t Locked_BlockProtection(uint8_t BP);
    uint8_t Unlocked_BlockProtection(void);
    void Enable_Solid_Protection(void);
    void Disable_Solid_Protection(void);

    void Test_GET_Registers_BlockProtection(void);
    void Test_GET_Registers_SecureOTP(void);
    void Test_GET_Registers_Status(void);    
    void Test_GET_Registers_InternalECC(void);

  private:
};

#endif