#include "MacronixMX35LF1.h"
#include "SPI.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_task_wdt.h" 
#define spi_readwrite(x) (_spi->transfer(x))
#define spi_read()       (spi_readwrite(0x00))
#define INDEX_PAGE 1
#define FILE_HEADER_MAGIC 0xA5  // Um byte para identificar que essa pzgina eh um “arquivo valido”
#define FILE_NAME_SIZE 32

MX35LF::MX35LF(uint8_t _CS) 
{
    mx35CS = _CS;
    MX35_UNSELECT();
    pinMode(mx35CS, OUTPUT);
    _spi = &SPI;
}

MX35LF::MX35LF(uint8_t _CS, SPIClass *_SPI)
{
    mx35CS = _CS;
    MX35_UNSELECT();
    pinMode(mx35CS, OUTPUT);
    _spi = _SPI;
}


uint8_t MX35LF::begin()
{
    Serial.printf("[%ld] - uint8_t begin()\r\n", millis());
    uint8_t res;

    _spi->begin();
    res = mx35lf_init_check();
    if(res){
        return MX35_OK;
    }
    return MX35_FAIL; 
}

uint8_t MX35LF::begin_WP(uint8_t _wp_pin)
{
    if (this->begin() != MX35_OK)
        return MX35_FAIL;
        
    Set_WP_pin(_wp_pin);
    return MX35_OK;
}



uint8_t MX35LF::mx35lf_init_check()
{
    Serial.printf("[%ld] - uint8_t mx35lf_init_check()\r\n", millis());
    mx35lf_reset();

    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_READ_MANUFACTURER_ID);
    spi_readwrite(0x00); // Dummy Byte
    uint16_t manufacturerID = spi_read(); 
    uint16_t deviceID = spi_read();
    MX35_UNSELECT();
    _spi->endTransaction();

    Serial.print("Manufacturer ID: 0x");
    Serial.println(manufacturerID, HEX);
    Serial.print("Device ID: 0x");
    Serial.println(deviceID, HEX);
    return (manufacturerID && deviceID);
}

void MX35LF::mx35lf_reset()
{
    Serial.printf("[%ld] - void mx35lf_reset()\r\n", millis());
    if (_WPpin >= 0)
    {
        digitalWrite((uint8_t)_WPpin, LOW);
        Serial.println("TURN OFF WP PIN");
    }

    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_RESET);
    MX35_UNSELECT();
    _spi->endTransaction();
    delay(5); // its necessary wait until the module reset

    if (_WPpin >= 0)
    {
        digitalWrite((uint8_t)_WPpin, WP_status);    
        Serial.printf("BACK THE WP PIN TO PREVIOUS STATUS: %s\r\n", WP_status == HIGH ? "HIGH" : "LOW");
    }
}



uint8_t MX35LF::mx35lf_start_read_page(uint16_t page_address)
{
    //Serial.printf("[%ld] - uint8_t mx35lf_start_read_page()\r\n", millis());

    //if (page_address >= MAX_PAGE_SIZE)
        //return MX35_FAIL;

    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_PAGE_READ);
    spi_readwrite(0x00); // Dummy Byte
    spi_readwrite((page_address & 0xFF00) >> 8);
    spi_readwrite(page_address);
    MX35_UNSELECT();
    _spi->endTransaction();

    if (WaitOperationDone() != MX35_OK)
        return MX35_FAIL;
    return MX35_OK;
}

uint8_t MX35LF::read_one_page_from_block(uint16_t page_address, uint8_t *buffer, uint32_t len, uint8_t wrap)
{
   // Serial.printf("[%ld] - uint8_t read_one_page_from_block()\r\n", millis());
    uint8_t wrap_bit = wrap << 6;

    Write_Disable();
    if (mx35lf_start_read_page(page_address) != MX35_OK) 
        return MX35_FAIL;

    read_page(buffer, len, wrap_bit);

    return MX35_OK;
}

uint8_t MX35LF::read_pages_from_cache(uint8_t *buffer, uint32_t len, uint16_t init_page, uint8_t final_page, uint8_t wrap)
{
    //Serial.printf("[%ld] - uint8_t read_pages_from_cache()\r\n", millis());
    int i = 0;
    uint16_t wrap_bit = wrap << 6;

    if ((mx35lf_start_read_page(init_page) != MX35_OK) || (final_page - init_page) <= 1)  
        return MX35_FAIL;

    for (i = init_page; i < (final_page - 1); i++)
    {
        _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
        MX35_SELECT();
        spi_readwrite(CMD_PAGE_READ_CACHE_SEQUENTIAL);
        MX35_UNSELECT();
        _spi->endTransaction();
        
        if (WaitOperationDone() != MX35_OK)
            return MX35_READ_FAIL;

        read_page(buffer, len, wrap_bit, len * i);
    }

    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_PAGE_READ_CACHE_END);
    MX35_UNSELECT();
    _spi->endTransaction();

    if (WaitOperationDone() != MX35_OK)
        return MX35_FAIL;

    read_page(buffer, len, wrap_bit, len * i);
    
    return MX35_OK;
}

void MX35LF::read_page(uint8_t *buffer, uint32_t len, uint8_t WRAP_BIT, uint64_t init)
{
    //Serial.printf("[%ld] - uint8_t read_page()\r\n", millis());
    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_READ_FROM_CACHE);
    spi_readwrite(WRAP_BIT);
    spi_readwrite(WRAP_BIT & 0x00);
    spi_readwrite(0x00); // Dummy Byte
    for (int i = 0; i < len; i++)
        buffer[init + i] = spi_read();
    MX35_UNSELECT();
    _spi->endTransaction();
}



uint8_t MX35LF::LoadProgData(uint8_t wrap, uint16_t page_address, uint8_t *buffer, uint32_t len)
{
    //Serial.printf("[%ld] - uint8_t LoadProgData()\r\n", millis());
    Write_Enable();

    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_PROGRAM_LOAD_X1);
    spi_readwrite(wrap << 6);
    spi_readwrite(wrap & 0x00);
    for (int i = 0; i < len; i++)
        spi_readwrite(buffer[i]);
    MX35_UNSELECT();
    _spi->endTransaction();

    if (ProgramExecute(page_address) != MX35_OK)
        return MX35_WRITE_FAIL;
    return MX35_OK;
}

uint8_t MX35LF::LoadRandProgData(uint8_t wrap, uint16_t page_address, uint8_t *buffer, uint32_t len)
{
    //Serial.printf("[%ld] - uint8_t LoadRandProgData()\r\n", millis());
    Write_Enable();

    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_PROGRAM_LOAD_RANDOM_DATA);
    spi_readwrite(wrap << 6);
    spi_readwrite(wrap & 0x00);
    for (int i = 0; i < len; i++)
        spi_readwrite(buffer[i]);
    MX35_UNSELECT();
    _spi->endTransaction();

    if (ProgramExecute(page_address) != MX35_OK)
        return MX35_WRITE_FAIL;
    return MX35_OK;
}

uint8_t MX35LF::ProgramExecute(uint16_t page_address)
{
    //Serial.printf("[%ld] - uint8_t ProgramExecute()\r\n", millis());

    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_PROGRAM_EXECUTE);
    spi_readwrite(0x00); // Dummy Byte
    spi_readwrite((page_address & 0xFF00) >> 8);
    spi_readwrite(page_address);
    MX35_UNSELECT();
    _spi->endTransaction();

    if (WaitOperationDone() != MX35_OK)
        return MX35_WRITE_FAIL;
    return MX35_OK;
}



uint8_t MX35LF::Block_Erase(uint16_t page_address)
{
   // Serial.printf("[%ld] - uint8_t Block_Erase()\r\n", millis()); 
    Write_Enable();
   Serial.printf("Erase Block: %d\r\n", (int)Page_To_Block(page_address));

    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_BLOCK_ERASE);
    spi_readwrite(0x00); // Dummy Byte
    spi_readwrite((page_address & 0xFF00) >> 8);
    spi_readwrite(page_address);
    MX35_UNSELECT();
    _spi->endTransaction();
    delay(5);
    if (WaitOperationDone() != MX35_OK)
        return MX35_FAIL;
    return MX35_OK;
}

uint8_t MX35LF::Bulk_Erase()
{   
    Serial.printf("[%ld] - uint8_t Bulk_Erase()\r\n", millis());
    for (uint16_t i = 0; i <= MAX_PAGE_SIZE-129; i += NUM_PAGES_PER_BLOCK){
            Serial.printf("Contador: ");
           // uint16_t cnt = i/64;
            Serial.printf("%d\n", i);
        if (Block_Erase(i) != MX35_OK){
            return MX35_FAIL;       
        }
    }
    return MX35_OK;
    Serial.printf("Sucesso em apagar todos os Blocos!");
    delay(1500);
    return MX35_OK;
}


void MX35LF::Verify_Bad_Blocks(uint8_t *buffer, uint32_t buffer_size)
{
   // Serial.printf("[%ld] - uint8_t Verify_Bad_Blocks()\r\n", millis());
    uint8_t *ref = (uint8_t*)malloc(sizeof(uint8_t) * 2);
    for (uint32_t i = 0; i < MAX_PAGE_SIZE; i += NUM_PAGES_PER_BLOCK)
    {
        mx35lf_start_read_page(i);
        read_one_page_from_block(i, ref, 2, FINAL_PAGE_ADDRESS_16);
        if (*(ref + 0) != 0xFF)
        {
            Have_Invalid_Blocks = true;
            Blocks_Invalids[Qnt_Invalid_Blocks++] = (uint8_t)Page_To_Block(i);
        }
    }

    Serial.printf("Have %d Bad blocks:\r\n", Qnt_Invalid_Blocks);
    for (uint8_t i = 0; i < Qnt_Invalid_Blocks; i++)
        Serial.println(Blocks_Invalids[i]);

    free(ref);
    if (buffer_size >= Qnt_Invalid_Blocks)
        memcpy(buffer, Blocks_Invalids, Qnt_Invalid_Blocks);
    return;
}


uint8_t MX35LF::mx35lf_GET_Features(uint8_t address)
{
    //Serial.printf("[%ld] - uint8_t mx35lf_GET_Features()\r\n", millis());
    
    uint8_t ret;
    unsigned long start_time = millis();

    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_GET_FEATURES);
    spi_readwrite(address);
    ret = spi_read();
    MX35_UNSELECT();
    _spi->endTransaction();

    unsigned long elapsed_time = millis() - start_time; 
    Serial.printf("Tempo de espera: ");
    Serial.printf("%ld\n", elapsed_time);
    // Serial.printf("Tempo para enviar o comando GET FEATURES e receber a resposta: %lums\r\n", elapsed_time);
   // Serial.printf("Valor lido do registro (0x%02X): 0x%02X\r\n", address, ret);

    return ret;
}

void MX35LF::mx35lf_SET_features(uint8_t address, uint8_t value)
{
    Serial.printf("[%ld] - void mx35lf_SET_features()\r\n", millis());

    unsigned long start_time = millis();
    
    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_SET_FEATURES);
    spi_readwrite(address);
    spi_readwrite(value);    
    MX35_UNSELECT();
    _spi->endTransaction(); 
    
    unsigned long elapsed_time = millis() - start_time; 
    Serial.printf("Tempo para enviar o comando GET FEATURES e receber a resposta: %lums\r\n", elapsed_time);
    Serial.printf("Valor lido do registro (0x%02X): 0x%02X\r\n", address, value);
    
    return;
}



void MX35LF::Write_Enable()
{
    //Serial.printf("[%ld] - void Write_Enable()\r\n", millis());
    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_WRITE_ENABLE);
    MX35_UNSELECT();
    _spi->endTransaction();  
}

void MX35LF::Write_Disable()
{
    //Serial.printf("[%ld] - void Write_Disable()\r\n", millis());
    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_WRITE_DISABLE);
    MX35_UNSELECT();
    _spi->endTransaction();
}



void MX35LF::Parameter_Page(uint8_t *buffer)
{
    Serial.printf("[%ld] - void Parameter_Page()\r\n", millis());
    mx35lf_SET_features(REG_SECURE_OTP, 0x40);
    mx35lf_start_read_page(0x01);
    if (WaitOperationDone() != MX35_OK)
        return;
    read_one_page_from_block(0x000, buffer, PAGE_SIZE, FINAL_PAGE_ADDRESS_2112);
    mx35lf_SET_features(REG_SECURE_OTP, 0x10);
}

void MX35LF::UniqueID_Page(uint8_t *buffer)
{
    Serial.printf("[%ld] - void UniqueID_Page()\r\n", millis());
    Parameter_Page(buffer);
    for (size_t i = 0; i < PAGE_SIZE; i++)
        buffer[i] ^= 0xFF;
}



void MX35LF::Set_WP_pin(uint8_t _wp)
{
    Serial.printf("[%ld] - void Set_WP_pin()\r\n", millis());
    if (!GPIO_IS_VALID_OUTPUT_GPIO(_wp))
        return;
    _WPpin = (int8_t)_wp;
    digitalWrite((uint8_t)_WPpin, LOW);
    pinMode(_WPpin, OUTPUT);
}

void MX35LF::Enable_WP_pin()
{
    Serial.printf("[%ld] - void Enable_WP_pin()\r\n", millis());
    if (_WPpin < 0)
        return;
    WP_status = HIGH;
    digitalWrite((uint8_t)_WPpin, WP_status);
}

void MX35LF::Disable_WP_pin()
{
    Serial.printf("[%ld] - void Disable_WP_pin()\r\n", millis());
    if (_WPpin < 0)
        return;
    WP_status = LOW;
    digitalWrite((uint8_t)_WPpin, WP_status);
}



void MX35LF::Set_Debug_Mode(bool _debug)
{
    Serial.printf("[%ld] - void Set_Debug_Mode()\r\n", millis());
    debug_mode = _debug;
}



uint8_t MX35LF::WaitOperationDone()
{
    //Serial.printf("[%ld] - uint8_t WaitOperationDone()\r\n", millis());
    unsigned long time_reference = millis();
    const unsigned long timeout = 500; // 0.5 seconds
    while ((millis() - time_reference) < timeout)
        if ((mx35lf_GET_Features(REG_STATUS) & 0x01) == STATUS_READY)
            return MX35_OK;
    return MX35_FAIL;   
}



uint8_t MX35LF::Locked_BlockProtection(uint8_t BP)
{
    if (BP > ALL_LOCKED_MAX_BIT || BP <= ALL_UNLOCKED)
        return MX35_FAIL;

    uint8_t re = (BP << 1) | mx35lf_GET_Features(REG_BLOCK_PROTECTION);

    Enable_WP_pin();
    mx35lf_SET_features(REG_BLOCK_PROTECTION, re);
    mx35lf_SET_features(REG_BLOCK_PROTECTION, re | BPRWD_BIT);
    Disable_WP_pin();

    return MX35_OK;
}
bool MX35LF:: load_page_to_cache(uint16_t page_number)
{
    _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(CMD_PAGE_READ);                        // 0x13
    spi_readwrite((page_number >> 0) & 0xFF);            // A[7:0]
    spi_readwrite((page_number >> 8) & 0xFF);            // A[15:8]
    spi_readwrite(0x00);                                 // A[23:16] = 0
    MX35_UNSELECT();
    _spi->endTransaction();

    return (WaitOperationDone() == MX35_OK);
}

uint8_t MX35LF::Unlocked_BlockProtection()
{
    uint8_t b_prot = mx35lf_GET_Features(REG_BLOCK_PROTECTION);

    if (b_prot & SP_BIT)
    {
        b_prot &= ~SP_BIT;
        mx35lf_SET_features(REG_BLOCK_PROTECTION, b_prot);
    }

    if (b_prot & BPRWD_BIT)
    {
        Enable_WP_pin();
        b_prot &= ~BPRWD_BIT;
        mx35lf_SET_features(REG_BLOCK_PROTECTION, b_prot);
    }

    Enable_WP_pin();
    mx35lf_SET_features(REG_BLOCK_PROTECTION, b_prot & 0x40);
    Disable_WP_pin();
    
    return MX35_OK;
}

void MX35LF::Enable_Solid_Protection()
{
    uint8_t b_prot = mx35lf_GET_Features(REG_BLOCK_PROTECTION);

    mx35lf_SET_features(REG_BLOCK_PROTECTION, b_prot | SP_BIT);
}

void MX35LF::Disable_Solid_Protection()
{
    uint8_t b_prot = mx35lf_GET_Features(REG_BLOCK_PROTECTION);

    mx35lf_SET_features(REG_BLOCK_PROTECTION, b_prot & ~SP_BIT);
}



uint8_t MX35LF::SecureOTP_NormalOperation() 
{
    uint8_t ret = mx35lf_GET_Features(REG_SECURE_OTP);
    ret &= ~SECURE_OTP_ENABLE;
    ret &= ~SECURE_OTP_PROTECT;
    mx35lf_SET_features(REG_SECURE_OTP, ret);
    return MX35_OK;
}

void MX35LF::SecureOTP_Enable()
{
    uint8_t ret = mx35lf_GET_Features(REG_SECURE_OTP);
    mx35lf_SET_features(REG_SECURE_OTP, ret | SECURE_OTP_ENABLE);
}

void MX35LF::SecureOTP_ProtectionBit()
{
    uint8_t ret = mx35lf_GET_Features(REG_SECURE_OTP);
    mx35lf_SET_features(REG_SECURE_OTP, ret | SECURE_OTP_PROTECT | SECURE_OTP_ENABLE);
}



void MX35LF::Test_GET_Registers_BlockProtection()
{
    Serial.printf("[%ld] - void Test_GET_Registers_BlockProtection()\r\n", millis()); 
    
    uint8_t reg = mx35lf_GET_Features(REG_BLOCK_PROTECTION);
    Serial.print("RESULT: 0x"); Serial.print(reg, HEX); Serial.println();

    Serial.printf("bit[0] - [SP]            - %d\r\n", (reg >> 0) & 0x01);
    Serial.printf("bit[1] - [Complementary] - %d\r\n", (reg >> 1) & 0x01);
    Serial.printf("bit[2] - [Invert]        - %d\r\n", (reg >> 2) & 0x01);
    Serial.printf("bit[3] - [BP0]           - %d\r\n", (reg >> 3) & 0x01);
    Serial.printf("bit[4] - [BP1]           - %d\r\n", (reg >> 4) & 0x01);
    Serial.printf("bit[5] - [BP2]           - %d\r\n", (reg >> 5) & 0x01);
    Serial.printf("bit[6] - [Reserved]      - %d\r\n", (reg >> 6) & 0x01);
    Serial.printf("bit[7] - [BPRWD]         - %d\r\n", (reg >> 7) & 0x01);
    Serial.println();
}

void MX35LF::Test_GET_Registers_SecureOTP()
{
    Serial.printf("[%ld] - void Test_GET_Registers_SecureOTP()\r\n", millis()); 

    uint8_t reg = mx35lf_GET_Features(REG_SECURE_OTP);
    Serial.print("RESULT: 0x"); Serial.print(reg, HEX); Serial.println();

    Serial.printf("bit[0] - [QE]                 - %d\r\n", (reg >> 0) & 0x01);
    Serial.printf("bit[1] - [Reserved]           - %d\r\n", (reg >> 1) & 0x01);
    Serial.printf("bit[2] - [Reserved]           - %d\r\n", (reg >> 2) & 0x01);
    Serial.printf("bit[3] - [Reserved]           - %d\r\n", (reg >> 3) & 0x01);
    Serial.printf("bit[4] - [ECC enabled]        - %d\r\n", (reg >> 4) & 0x01);
    Serial.printf("bit[5] - [Reserved]           - %d\r\n", (reg >> 5) & 0x01);
    Serial.printf("bit[6] - [Secure OTP Enable]  - %d\r\n", (reg >> 6) & 0x01);
    Serial.printf("bit[7] - [Secure OTP Protect] - %d\r\n", (reg >> 7) & 0x01);
    Serial.println();
}

void MX35LF::Test_GET_Registers_Status()
{
    Serial.printf("[%ld] - void Test_GET_Registers_Status()\r\n", millis()); 

    uint8_t reg = mx35lf_GET_Features(REG_STATUS);
    Serial.print("RESULT: 0x"); Serial.print(reg, HEX); Serial.println();

    Serial.printf("bit[0] - [OIP]      - %d\r\n", (reg >> 0) & 0x01);
    Serial.printf("bit[1] - [WEL]      - %d\r\n", (reg >> 1) & 0x01);
    Serial.printf("bit[2] - [E_Fail]   - %d\r\n", (reg >> 2) & 0x01);
    Serial.printf("bit[3] - [P-Fail]   - %d\r\n", (reg >> 3) & 0x01);
    Serial.printf("bit[4] - [ECC_S0]   - %d\r\n", (reg >> 4) & 0x01);
    Serial.printf("bit[5] - [ECC_S1]   - %d\r\n", (reg >> 5) & 0x01);
    Serial.printf("bit[6] - [CRBSY]    - %d\r\n", (reg >> 6) & 0x01);
    Serial.printf("bit[7] - [Reserved] - %d\r\n", (reg >> 7) & 0x01);
    Serial.println();
}

void MX35LF::Test_GET_Registers_InternalECC()
{
    Serial.printf("[%ld] - void Test_GET_Registers_InternalECC()\r\n", millis()); 
   
   _spi->beginTransaction(SPISettings(CLOCK_FREQUENCY, MSBFIRST, SPI_MODE0));
    MX35_SELECT();
    spi_readwrite(REG_INTERNAL_ECC_STATUS);
    spi_readwrite(0x00); // Dummy byte
    uint8_t reg = spi_read();
    MX35_UNSELECT();
    _spi->endTransaction();

    Serial.print("RESULT: 0x"); Serial.print(reg, HEX); Serial.println();

    Serial.printf("bit[0] - [ECCSR[0]] - %d\r\n", (reg >> 0) & 0x01);
    Serial.printf("bit[1] - [ECCSR[1]] - %d\r\n", (reg >> 1) & 0x01);
    Serial.printf("bit[2] - [ECCSR[2]] - %d\r\n", (reg >> 2) & 0x01);
    Serial.printf("bit[3] - [ECCSR[3]] - %d\r\n", (reg >> 3) & 0x01);
    Serial.printf("bit[4] - [Reserved] - %d\r\n", (reg >> 4) & 0x01);
    Serial.printf("bit[5] - [Reserved] - %d\r\n", (reg >> 5) & 0x01);
    Serial.printf("bit[6] - [Reserved] - %d\r\n", (reg >> 6) & 0x01);
    Serial.printf("bit[7] - [Reserved] - %d\r\n", (reg >> 7) & 0x01);
    Serial.println();    
}
// Atualiza página apenas se houver diferença
uint8_t MX35LF::update_page_compare_write(uint16_t page_address, const uint8_t* new_data, uint32_t length) {
    if (!new_data || length == 0 || length > PAGE_SIZE) {
        Serial.printf("update_page_compare_write: Dados inválidos.\n");
        return MX35_FAIL;
    }
    // Aloca buffer para leitura
    uint8_t* current_data = (uint8_t*)malloc(length);
    if (!current_data) {
        Serial.printf("update_page_compare_write: Falha ao alocar memória.\n");
        return MX35_FAIL;
    }
    // Read Page
    if (read_one_page_from_block(page_address, current_data, length, FINAL_PAGE_ADDRESS_2048) != MX35_OK) {
        Serial.printf("update_page_compare_write: Falha ao ler página %d.\n", page_address);
        free(current_data);
        return MX35_FAIL;
    }
    // Veerify diferences
    bool needs_update = false;
    for (uint32_t i = 0; i < length; i++) {
        if (current_data[i] != new_data[i]) {
            needs_update = true;
            break;
        }
    }
    if (!needs_update) {
        Serial.printf("update_page_compare_write: Página %d já está atualizada.\n", page_address);
        free(current_data);
        return MX35_OK;
    }
    // Apaga e grava página
    Serial.printf("update_page_compare_write: Atualizando página %d...\n", page_address);
    if (Block_Erase(page_address) != MX35_OK) {
        Serial.printf("update_page_compare_write: Erro ao apagar bloco da página %d.\n", page_address);
        free(current_data);
        return MX35_FAIL;
    }
    if (LoadProgData(0, page_address, (uint8_t*)new_data, length) != MX35_OK) {
        Serial.printf("update_page_compare_write: Erro ao programar página %d.\n", page_address);
        free(current_data);
        return MX35_FAIL;
    }

    free(current_data);
    return MX35_OK;
}
// Adiciona entrada de arquivo ao índice (página 0)
uint8_t MX35LF::add_file_to_index(const char* filename, uint16_t start_page, uint16_t num_pages, uint32_t len) {
    if (!filename) return MX35_FAIL;
    uint8_t* index_buffer = (uint8_t*)malloc(PAGE_SIZE);
    if (!index_buffer) {
        Serial.println("add_file_to_index: Falha ao alocar memória.");
        return MX35_FAIL;
    }
    // Inicializa buffer com 0xFF (memória apagada)
    memset(index_buffer, 0xFF, PAGE_SIZE);
    // Preenche o primeiro slot de 43 bytes
    index_buffer[0] = FILE_HEADER_MAGIC;
    strncpy((char*)&index_buffer[1], filename, FILE_NAME_SIZE);  // Nome do arquivo (até 32 bytes)
    // Posição inicial
    index_buffer[33] = start_page & 0xFF;
    index_buffer[34] = (start_page >> 8) & 0xFF;
    // Número de páginas
    index_buffer[35] = num_pages & 0xFF;
    index_buffer[36] = (num_pages >> 8) & 0xFF;
    // Tamanho do arquivo (4 bytes)
    index_buffer[37] = (len >> 0) & 0xFF;
    index_buffer[38] = (len >> 8) & 0xFF;
    index_buffer[39] = (len >> 16) & 0xFF;
    index_buffer[40] = (len >> 24) & 0xFF;
    Serial.println("add_file_to_index: Apagando página de índice...");
    if (Block_Erase(INDEX_PAGE) != MX35_OK) {
        Serial.println("add_file_to_index: Erro ao apagar o bloco da página de índice.");
        free(index_buffer);
        return MX35_FAIL;
    }
    Serial.println("add_file_to_index: Gravando índice...");
    if (LoadProgData(FINAL_PAGE_ADDRESS_2048, INDEX_PAGE, index_buffer, PAGE_SIZE) != MX35_OK) {
        Serial.println("add_file_to_index: Falha no LoadProgData da página de índice.");
        free(index_buffer);
        return MX35_FAIL;
    }
    delay(5);  // Pequeno atraso pode ser necessário
    if (ProgramExecute(INDEX_PAGE) != MX35_OK) {
        Serial.println("add_file_to_index: Falha no ProgramExecute da página de índice.");
        free(index_buffer);
        return MX35_FAIL;
    }
    Serial.println("add_file_to_index: Índice gravado com sucesso!");
    free(index_buffer);
    return MX35_OK;
}

// Write the file
uint8_t MX35LF::write_file(const char* filename, const uint8_t* data, size_t len) {
    if (!filename || !data || len == 0) return MX35_FAIL;
    const uint16_t start_page = 4;  // Reserve for index
    uint16_t needed_pages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    // Find space for file
    uint16_t page = start_page, found_start = 0xFFFF, free_pages = 0;
    while (page < MAX_PAGE_SIZE) {
        if (is_block_bad(Page_To_Block(page))) {
            page += NUM_PAGES_PER_BLOCK;
            free_pages = 0;
            continue;
        }
        if (free_pages == 0) found_start = page;
        free_pages++;
        if (free_pages >= needed_pages) break;
        page++;
    }
    if (free_pages < needed_pages) {
        Serial.println("write_file: Espaço insuficiente.");
        return MX35_FAIL;
    }
    // Write page
    for (uint16_t i = 0; i < needed_pages; i++) {
        uint32_t offset = i * PAGE_SIZE;
        uint32_t chunk = (len - offset > PAGE_SIZE) ? PAGE_SIZE : (len - offset);
        // Buffer
        uint8_t* tmp = (uint8_t*)malloc(chunk);
        if (!tmp) return MX35_FAIL;
        memcpy(tmp, &data[offset], chunk);
        if (update_page_compare_write(found_start + i, tmp, chunk) != MX35_OK) {
            free(tmp);
            return MX35_FAIL;
        }
        free(tmp);
    }
    // update index
    return add_file_to_index(filename, found_start, needed_pages, len);
}

// Read file with index
uint8_t MX35LF::read_file(const char* filename, uint8_t* out_buffer, size_t* out_len) {
    if (!filename || !out_buffer || !out_len) return MX35_FAIL;
    uint8_t* index_buffer = (uint8_t*)malloc(PAGE_SIZE);
    if (!index_buffer) return MX35_FAIL;

    if (read_one_page_from_block(INDEX_PAGE, index_buffer, PAGE_SIZE, FINAL_PAGE_ADDRESS_2048) != MX35_OK) {
        Serial.printf("read_file: Falha ao ler índice.\n");
        free(index_buffer);
        return MX35_FAIL;
    }
    uint16_t name_len = strlen(filename);
    for (uint16_t offset = 0; offset + 43 <= PAGE_SIZE; offset += 43) {
        if (index_buffer[offset] == FILE_HEADER_MAGIC &&
            strncmp((char*)&index_buffer[offset + 1], filename, name_len) == 0) {

            uint16_t start_page = index_buffer[offset + 33] | (index_buffer[offset + 34] << 8);
            uint16_t num_pages  = index_buffer[offset + 35] | (index_buffer[offset + 36] << 8);
            uint32_t file_len   = index_buffer[offset + 37] |
                                  (index_buffer[offset + 38] << 8) |
                                  (index_buffer[offset + 39] << 16) |
                                  (index_buffer[offset + 40] << 24);

            Serial.printf("Arquivo encontrado no índice: Start Page=%d, Num Pages=%d, Size=%lu\n",
                          start_page, num_pages, (unsigned long)file_len);
            *out_len = file_len;
            free(index_buffer);
            for (uint16_t i = 0; i < num_pages; i++) {
                uint32_t offset_buf = i * PAGE_SIZE;
                uint32_t chunk = (file_len - offset_buf > PAGE_SIZE) ? PAGE_SIZE : (file_len - offset_buf);

                if (read_one_page_from_block(start_page + i, &out_buffer[offset_buf], chunk, FINAL_PAGE_ADDRESS_2048) != MX35_OK) {
                    Serial.printf("read_file: Falha ao ler página %d\n", start_page + i);
                    return MX35_FAIL;
                }
            }
            return MX35_OK;
        }
    }
    Serial.println("read_file: Arquivo não encontrado no índice.");
    free(index_buffer);
    return MX35_FAIL;
}


bool MX35LF::is_block_bad(uint16_t block_num) {
    for (uint8_t i = 0; i < Qnt_Invalid_Blocks; i++) {
        if (Blocks_Invalids[i] == block_num)
            return true;
    }
    return false;
}

uint8_t MX35LF::write_raw_file(uint16_t start_page, const uint8_t* data, size_t len) {
    uint16_t num_pages = (len + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint16_t i = 0; i < num_pages; i++) {
        uint32_t offset = i * PAGE_SIZE;
        uint32_t chunk = (len - offset > PAGE_SIZE) ? PAGE_SIZE : (len - offset);

        // Apagar o bloco se for primeira página dele
        if ((start_page + i) % NUM_PAGES_PER_BLOCK == 0) {
            Block_Erase(Page_To_Block(start_page + i));
        }

        uint8_t* temp = (uint8_t*)malloc(PAGE_SIZE);
        memset(temp, 0xFF, PAGE_SIZE);
        memcpy(temp, &data[offset], chunk);

        LoadProgData(FINAL_PAGE_ADDRESS_2048, start_page + i, temp, PAGE_SIZE);
        ProgramExecute(start_page + i);
        free(temp);
    }
    return MX35_OK;
}

uint8_t MX35LF::read_raw_file(uint16_t start_page, uint8_t* out_buffer, size_t len) {
    uint16_t num_pages = (len + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint16_t i = 0; i < num_pages; i++) {
        uint32_t offset = i * PAGE_SIZE;
        uint32_t chunk = (len - offset > PAGE_SIZE) ? PAGE_SIZE : (len - offset);

        read_page(&out_buffer[offset], chunk, FINAL_PAGE_ADDRESS_2048, 0);  // FIXO INIT = 0
    }
    return MX35_OK;
}
