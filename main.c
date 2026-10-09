#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"

#include "esp_timer.h"
#include "esp_rom_sys.h"


// ======================================================
// PINES DEL SISTEMA
// ======================================================

// HC-SR04
#define TRIG_PIN        GPIO_NUM_5
#define ECHO_PIN        GPIO_NUM_18

// Potenciómetro - simulación de lluvia
#define RAIN_PIN        ADC1_CHANNEL_6       // GPIO34

// Servo
#define SERVO_PIN       GPIO_NUM_19

// Relay - bomba
#define RELAY_PIN       GPIO_NUM_23

// Buzzer
#define BUZZER_PIN      GPIO_NUM_22

// LEDs
#define LED_GREEN       GPIO_NUM_25
#define LED_YELLOW      GPIO_NUM_26
#define LED_RED         GPIO_NUM_27

// Botón de emergencia
#define ESTOP_PIN       GPIO_NUM_21


// ======================================================
// ESTADOS DEL SISTEMA
// ======================================================

typedef enum
{
    NORMAL,
    ALERTA,
    CRITICO,
    EMERGENCIA
} Estado;

Estado estadoActual = NORMAL;


// ======================================================
// CONFIGURACIÓN DEL SERVO
// ======================================================

#define SERVO_FREQ_HZ       50
#define SERVO_MIN_US        500
#define SERVO_MAX_US        2400

static void servo_init(void)
{
    ledc_timer_config_t servo_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_16_BIT,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ledc_timer_config(&servo_timer);

    ledc_channel_config_t servo_channel = {
        .gpio_num = SERVO_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config(&servo_channel);
}


static void servo_set_angle(int angle)
{
    if (angle < 0)
        angle = 0;

    if (angle > 180)
        angle = 180;

    int pulse_us =
        SERVO_MIN_US +
        ((SERVO_MAX_US - SERVO_MIN_US) * angle) / 180;

    uint32_t duty =
        ((uint64_t)pulse_us * 65535) / 20000;

    ledc_set_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0,
        duty
    );

    ledc_update_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0
    );
}


// ======================================================
// CONFIGURACIÓN DEL BUZZER
// ======================================================

static void buzzer_init(void)
{
    ledc_timer_config_t buzzer_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ledc_timer_config(&buzzer_timer);

    ledc_channel_config_t buzzer_channel = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config(&buzzer_channel);
}


static void buzzer_on(void)
{
    ledc_set_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_1,
        512
    );

    ledc_update_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_1
    );
}


static void buzzer_off(void)
{
    ledc_set_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_1,
        0
    );

    ledc_update_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_1
    );
}


// ======================================================
// CONFIGURACIÓN DE GPIO
// ======================================================

static void gpio_init_all(void)
{
    // Salidas
    gpio_config_t output_conf = {
        .pin_bit_mask =
            (1ULL << LED_GREEN) |
            (1ULL << LED_YELLOW) |
            (1ULL << LED_RED) |
            (1ULL << RELAY_PIN) |
            (1ULL << TRIG_PIN),

        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&output_conf);


    // Entradas
    gpio_config_t input_conf = {
        .pin_bit_mask =
            (1ULL << ECHO_PIN) |
            (1ULL << ESTOP_PIN),

        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&input_conf);


    // Estados iniciales
    gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_YELLOW, 0);
    gpio_set_level(LED_RED, 0);
    gpio_set_level(RELAY_PIN, 0);
    gpio_set_level(TRIG_PIN, 0);
}


// ======================================================
// SENSOR HC-SR04
// ======================================================

static float medir_distancia_cm(void)
{
    // Pulso de disparo
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(2);

    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);

    gpio_set_level(TRIG_PIN, 0);


    // Esperar inicio del ECHO
    int64_t inicio_espera = esp_timer_get_time();

    while (gpio_get_level(ECHO_PIN) == 0)
    {
        if ((esp_timer_get_time() - inicio_espera) > 30000)
        {
            return -1;
        }
    }


    // Medir duración del ECHO
    int64_t inicio = esp_timer_get_time();

    while (gpio_get_level(ECHO_PIN) == 1)
    {
        if ((esp_timer_get_time() - inicio) > 30000)
        {
            return -1;
        }
    }

    int64_t fin = esp_timer_get_time();

    int64_t duracion_us = fin - inicio;

    float distancia = duracion_us / 58.0;

    return distancia;
}


// ======================================================
// CONVERTIR DISTANCIA A NIVEL DE AGUA
// ======================================================

static int calcular_nivel(float distancia)
{
    if (distancia < 0)
        return 0;

    /*
       Aproximación para el depósito:

       20 cm = 0 %
       15 cm = 28 %
       10 cm = 56 %
        5 cm = 83 %
        2 cm = 100 %
    */

    int nivel = ((20.0 - distancia) / 18.0) * 100.0;

    if (nivel < 0)
        nivel = 0;

    if (nivel > 100)
        nivel = 100;

    return nivel;
}


// ======================================================
// POTENCIÓMETRO - LLUVIA
// ======================================================

static int leer_lluvia(void)
{
    int valor = adc1_get_raw(RAIN_PIN);

    return valor;
}


// ======================================================
// CAMBIAR ESTADO
// ======================================================

static void actualizar_estado(int nivel)
{
    // E-STOP
    if (gpio_get_level(ESTOP_PIN) == 0)
    {
        estadoActual = EMERGENCIA;
        return;
    }


    // Nivel de agua
    if (nivel < 50)
    {
        estadoActual = NORMAL;
    }
    else if (nivel < 80)
    {
        estadoActual = ALERTA;
    }
    else
    {
        estadoActual = CRITICO;
    }
}


// ======================================================
// CONTROL DE ACTUADORES
// ======================================================

static void controlar_actuadores(void)
{
    // Apagar LEDs inicialmente
    gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_YELLOW, 0);
    gpio_set_level(LED_RED, 0);

    gpio_set_level(RELAY_PIN, 0);

    buzzer_off();


    // ==================================================
    // NORMAL
    // ==================================================

    if (estadoActual == NORMAL)
    {
        gpio_set_level(LED_GREEN, 1);

        // Bomba apagada
        gpio_set_level(RELAY_PIN, 0);

        // Compuerta cerrada
        servo_set_angle(0);
    }


    // ==================================================
    // ALERTA
    // ==================================================

    else if (estadoActual == ALERTA)
    {
        gpio_set_level(LED_YELLOW, 1);

        // Bomba encendida
        gpio_set_level(RELAY_PIN, 1);

        // Compuerta parcialmente abierta
        servo_set_angle(60);
    }


    // ==================================================
    // CRÍTICO
    // ==================================================

    else if (estadoActual == CRITICO)
    {
        gpio_set_level(LED_RED, 1);

        // Bomba encendida
        gpio_set_level(RELAY_PIN, 1);

        // Compuerta completamente abierta
        servo_set_angle(120);

        // Alarma
        buzzer_on();
    }


    // ==================================================
    // EMERGENCIA
    // ==================================================

    else if (estadoActual == EMERGENCIA)
    {
        // TODOS LOS ACTUADORES APAGADOS

        gpio_set_level(RELAY_PIN, 0);

        gpio_set_level(LED_GREEN, 0);
        gpio_set_level(LED_YELLOW, 0);
        gpio_set_level(LED_RED, 0);

        buzzer_off();

        // Servo en posición segura
        servo_set_angle(0);
    }
}


// ======================================================
// NOMBRE DEL ESTADO
// ======================================================

const char* nombre_estado(void)
{
    switch (estadoActual)
    {
        case NORMAL:
            return "NORMAL";

        case ALERTA:
            return "ALERTA";

        case CRITICO:
            return "CRITICO";

        case EMERGENCIA:
            return "EMERGENCIA";

        default:
            return "DESCONOCIDO";
    }
}


// ======================================================
// PROGRAMA PRINCIPAL
// ======================================================

void app_main(void)
{
    printf("\n");
    printf("=============================================\n");
    printf(" SISTEMA DE PREVENCION DE INUNDACIONES\n");
    printf(" ESP32 + IoT + ROBOTICA\n");
    printf("=============================================\n");


    // Inicializar GPIO
    gpio_init_all();


    // Configurar ADC
    adc1_config_width(ADC_WIDTH_BIT_12);

    adc1_config_channel_atten(
        RAIN_PIN,
        ADC_ATTEN_DB_11
    );


    // Inicializar servo
    servo_init();


    // Inicializar buzzer
    buzzer_init();


    printf("Sistema iniciado correctamente.\n");


    while (1)
    {
        // ==========================================
        // LEER SENSOR DE DISTANCIA
        // ==========================================

        float distancia = medir_distancia_cm();

        int nivel = calcular_nivel(distancia);


        // ==========================================
        // LEER POTENCIÓMETRO
        // ==========================================

        int lluvia_raw = leer_lluvia();


        // ==========================================
        // ACTUALIZAR ESTADO
        // ==========================================

        actualizar_estado(nivel);


        // ==========================================
        // CONTROLAR ACTUADORES
        // ==========================================

        controlar_actuadores();


        // ==========================================
        // MOSTRAR INFORMACIÓN
        // ==========================================

        printf("\n");
        printf("---------------------------------------------\n");

        printf("Distancia: %.2f cm\n", distancia);

        printf("Nivel de agua: %d %%\n", nivel);

        printf("Sensor de lluvia: %d\n", lluvia_raw);

        printf("Estado: %s\n", nombre_estado());

        printf("Bomba: %s\n",
               (estadoActual == ALERTA ||
                estadoActual == CRITICO)
                   ? "ENCENDIDA"
                   : "APAGADA");

        printf("Compuerta: ");

        if (estadoActual == NORMAL ||
            estadoActual == EMERGENCIA)
        {
            printf("CERRADA\n");
        }
        else if (estadoActual == ALERTA)
        {
            printf("PARCIALMENTE ABIERTA\n");
        }
        else
        {
            printf("ABIERTA\n");
        }


        if (gpio_get_level(ESTOP_PIN) == 0)
        {
            printf("*** EMERGENCIA ACTIVADA ***\n");
        }


        // ==========================================
        // INTERVALO DE ACTUALIZACIÓN
        // ==========================================

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
