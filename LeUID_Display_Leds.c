
#include <stdio.h>
#include "pico/stdlib.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/pwm.h"
#include "mfrc522.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define LED_VERMELHO 13
#define LED_VERDE 11

// GPIO e macros para o Buzzer e PWM
#define BUZZER_PIN 21
#define WRAP 1000
#define DIV_CLK 250
static uint slice;
static struct repeating_timer timer;
volatile static bool buzzer_on = false;

bool tag_certa = false;
bool card_detected = false;

bool buzzer_callback(struct repeating_timer *t)
{   
    if (!card_detected)
    {   
        pwm_set_gpio_level(BUZZER_PIN, 0);
        buzzer_on = false;
        return false;
    }

    if (buzzer_on)
    {
        pwm_set_gpio_level(BUZZER_PIN, WRAP / 2);
    }
    else
    {
        pwm_set_gpio_level(BUZZER_PIN, 0);
    }
    buzzer_on = !buzzer_on;

    return true;
}


void main()
{
    stdio_init_all();

    // Configura os pinos dos LEDs como saída e desligados inicialmente
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_put(LED_VERMELHO, 0);

    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_put(LED_VERDE, 0);

    // Inicializa MFRC522
    MFRC522Ptr_t mfrc = MFRC522_Init();
    PCD_Init(mfrc, spi0);
    PCD_AntennaOn(mfrc); // Liga antena
    sleep_ms(500);

    // Inicializa display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BUZZER_PIN);

    pwm_set_wrap(slice, WRAP);
    pwm_set_clkdiv(slice, DIV_CLK); 
    pwm_set_gpio_level(BUZZER_PIN, 0);
    pwm_set_enabled(slice, true);

    char uid_str[24]; // Buffer para UID formatado
    bool cor = true;

    while (1)
    {
        // Desliga LEDs antes de ler novo cartão
        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_VERDE, 0);

        printf("Waiting for card...\n");
        ssd1306_fill(&ssd, !cor);                  // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);      // Desenha um retângulo
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);           // Desenha uma linha
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);           // Desenha uma linha
        ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6); // Desenha uma string
        ssd1306_draw_string(&ssd, "FECHADURA ELET.", 5, 16);  // Desenha uma string
        ssd1306_draw_string(&ssd, "RFID RC522", 10, 28);   // Desenha uma string
        ssd1306_draw_string(&ssd, "Aproxime a", 8, 41);    // Desenha uma string
        ssd1306_draw_string(&ssd, "  CHAVE", 8, 52);      // Desenha uma string
        ssd1306_send_data(&ssd);

        while (!PICC_IsNewCardPresent(mfrc))
        {   
            card_detected = false;
            sleep_ms(500);
        }

        printf("Card detected! Trying to read UID...\n");

        if (PICC_ReadCardSerial(mfrc))
        {
            // Formata UID em string "XX XX XX XX ..."
            int offset = 0;
            for (int i = 0; i < mfrc->uid.size; i++)
            {
                offset += sprintf(&uid_str[offset], "%02X ", mfrc->uid.uidByte[i]);
            }
            printf("UID: %s\n", uid_str);     

            // Verifica UID para acionar LEDs
            if (mfrc->uid.size == 4) // Certifica-se que tem 4 bytes no UID
            {   
                card_detected = true;
                // Cartão UID 00 FC 95 7C liga LED azul GPIO12
                if (mfrc->uid.uidByte[0] == 0xB0 &&
                    mfrc->uid.uidByte[1] == 0xC0 &&
                    mfrc->uid.uidByte[2] == 0xCD &&
                    mfrc->uid.uidByte[3] == 0x73)
                {
                    gpio_put(LED_VERDE, 1);
                    buzzer_on = true;
                    add_repeating_timer_ms(200, buzzer_callback, NULL, &timer);
                    ssd1306_draw_string(&ssd, "Detectada!", 8, 41);    // Desenha uma string
                    ssd1306_draw_string(&ssd, "ABERTO", 16, 52);      // Desenha uma string // Desenha uma string
                }
                // Cartão UID C0 33 C3 80 liga LED verde GPIO13
                else
                {
                    gpio_put(LED_VERMELHO, 1);
                    buzzer_on = true;
                    add_repeating_timer_ms(1000, buzzer_callback, NULL, &timer);
                    ssd1306_draw_string(&ssd, "Detectada!", 8, 41);    // Desenha uma string
                    ssd1306_draw_string(&ssd, "FECHADO", 16, 52); 
                }
            }
            ssd1306_send_data(&ssd);                     // Atualiza o display 
        }
        else
        {   
            printf("Failed to read UID\n");
            ssd1306_draw_string(&ssd, "Falha na", 8, 41);  // Desenha uma string
            ssd1306_draw_string(&ssd, " Leitura", 8, 52);   // Desenha uma string
            ssd1306_send_data(&ssd);

        }
        sleep_ms(2000);
    }
}
