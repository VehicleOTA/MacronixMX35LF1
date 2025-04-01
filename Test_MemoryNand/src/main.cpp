#include <Arduino.h>
#include <SPI.h>
#include <MacronixMX35LF1.h>
#include <MacronixMX35LF1_Registers.h>

#define CS_PIN 10 // Defina o pino correto para CS
MX35LF flash(CS_PIN);

void setup() {
    Serial.begin(115200);
    SPI.begin();
    
    // Inicializa a memória Flash
    if (flash.begin() != MX35_OK) {
        Serial.println("Falha ao iniciar a Flash!");
        while (1);
    }
    Serial.println("Flash inicializada com sucesso!");
    
    // Habilita escrita
    flash.Write_Enable();
    
    // Apaga todos os blocos
    Serial.println("Apagando todos os blocos...");
    if (flash.Bulk_Erase() != MX35_OK) {
        Serial.println("Falha ao apagar os blocos!");
    } else {
        Serial.println("Todos os blocos apagados com sucesso!");
    }
    
    // Escrever '1' em todas as posições de memória
    uint8_t buffer[PAGE_SIZE_WITHOUT_ECC];
    memset(buffer, 1, sizeof(buffer)); // Preenche o buffer com 1
    
    Serial.println("Escrevendo '1' em todas as páginas...");
    for (uint16_t page = 0; page < MAX_PAGE_SIZE; page++) {
        flash.LoadProgData(FINAL_PAGE_ADDRESS_2048, page, buffer, PAGE_SIZE_WITHOUT_ECC);
        if (flash.ProgramExecute(page) != MX35_OK) {
            Serial.print("Erro ao escrever na página: ");
            Serial.println(page);
        }
    }
    Serial.println("Escrita concluída!");
}

void loop() {
  //lendo novamente todos os blocos para garantir
  Serial.println("Lendo tudo que foi escrito");
  uint8_t readbuffer[PAGE_SIZE_WITHOUT_ECC];
  for(uint16_t page = 0; page < MAX_PAGE_SIZE; page++){
    flash.read_page(readbuffer, PAGE_SIZE_WITHOUT_ECC, FINAL_PAGE_ADDRESS_2048, 0);
  }
  Serial.println("Leitura concluida! Verificando arquivos.");
  for(int i = 0; i< PAGE_SIZE_WITHOUT_ECC; i++){
    if(readbuffer[i] == 1){
      Serial.print("Arquivo lido corretemente! ");
      Serial.println(readbuffer[i]);
    }
    else{
       Serial.print("Arquivo errado, arquivo corrompido! Encerrado.");
       return;
    }
  }
}