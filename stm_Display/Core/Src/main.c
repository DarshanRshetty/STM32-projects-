
#include "main.h"
#include <stdint.h>
#include <stdio.h>

/* I2C and ADC handles */
I2C_HandleTypeDef hi2c1;
ADC_HandleTypeDef hadc1;

/* LCD configuration */
#define LCD_ADDR       (0x27 << 1)
#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RS         0x01

/* DHT11 configuration */
#define DHT11_PORT GPIOA
#define DHT11_PIN  GPIO_PIN_1

/* Function declarations */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
void Error_Handler(void);

void LCD_Write4Bits(uint8_t data);
void LCD_PulseEnable(uint8_t data);
void LCD_Send(uint8_t value, uint8_t mode);
void LCD_Command(uint8_t command);
void LCD_Data(uint8_t data);
void LCD_Init(void);
void LCD_Print(const char *text);

void DWT_Init(void);
void delay_us(uint32_t us);
void DHT11_SetOutput(void);
void DHT11_SetInput(void);
uint32_t ADC_Read_Percentage(void);
uint8_t DHT11_Wait(GPIO_PinState state, uint32_t timeout);
uint8_t DHT11_Read(uint8_t *temperature, uint8_t *humidity);

/* MAIN PROGRAM */
int main(void)
{
    uint8_t temperature = 0;
    uint8_t humidity = 0;
    uint32_t adc_percentage;
    char buffer[17];

    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_ADC1_Init();

    if (HAL_ADCEx_Calibration_Start(&hadc1,
                                    ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }

    DWT_Init();

    HAL_Delay(100);

    LCD_Init();

    LCD_Command(0x80);
    LCD_Print("ADC:  0%");

    LCD_Command(0xC0);
    LCD_Print("DHT11 Starting");

    HAL_Delay(1000);

    while (1)
    {
        /* Read ADC percentage */
        adc_percentage = ADC_Read_Percentage();

        snprintf(buffer, sizeof(buffer),
                 "ADC: %3lu%%",
                 (unsigned long)adc_percentage);

        LCD_Command(0x80);
        LCD_Print("                ");
        LCD_Command(0x80);
        LCD_Print(buffer);

        /* Read DHT11 sensor */
        if (DHT11_Read(&temperature, &humidity))
        {
            snprintf(buffer, sizeof(buffer),
                     "T:%02uC H:%02u%%",
                     temperature, humidity);
        }
        else
        {
            snprintf(buffer, sizeof(buffer),
                     "DHT11 ERROR");
        }

        LCD_Command(0xC0);
        LCD_Print("                ");
        LCD_Command(0xC0);
        LCD_Print(buffer);

        HAL_Delay(2000);
    }
}

/* ADC INITIALIZATION - PA0 / ADC1_IN5 */
static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC_CLK_ENABLE();

    /* PA0 analog input */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    hadc1.Instance = ADC1;

    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge =
        ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hadc1.Init.OversamplingMode = DISABLE;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /* PA0 = ADC1 Channel 5 */
    sConfig.Channel = ADC_CHANNEL_5;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ADC READ AND CONVERT TO PERCENTAGE */
uint32_t ADC_Read_Percentage(void)
{
    uint32_t adc_value;
    uint32_t adc_percentage;

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return 0;
    }

    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        adc_value = HAL_ADC_GetValue(&hadc1);

        adc_percentage = (adc_value * 100U) / 4095U;
    }
    else
    {
        adc_percentage = 0;
    }

    HAL_ADC_Stop(&hadc1);

    return adc_percentage;
}

/* DWT MICROSECOND TIMER */
void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < ticks)
    {
    }
}

/* DHT11 GPIO OUTPUT */
void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

/* DHT11 GPIO INPUT */
void DHT11_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

/* WAIT FOR PIN STATE */
uint8_t DHT11_Wait(GPIO_PinState state, uint32_t timeout)
{
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != state)
    {
        if (timeout == 0)
        {
            return 0;
        }

        delay_us(1);
        timeout--;
    }

    return 1;
}

/* DHT11 READ FUNCTION */
uint8_t DHT11_Read(uint8_t *temperature, uint8_t *humidity)
{
    uint8_t data[5] = {0};

    /* Send start signal */
    DHT11_SetOutput();

    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);

    HAL_Delay(18);

    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    delay_us(30);

    /* Release data line */
    DHT11_SetInput();

    /* Sensor response */
    if (!DHT11_Wait(GPIO_PIN_RESET, 100))
        return 0;

    if (!DHT11_Wait(GPIO_PIN_SET, 100))
        return 0;

    if (!DHT11_Wait(GPIO_PIN_RESET, 100))
        return 0;

    /* Read 40 bits */
    for (uint8_t i = 0; i < 40; i++)
    {
        if (!DHT11_Wait(GPIO_PIN_SET, 100))
            return 0;

        delay_us(40);

        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
        {
            data[i / 8] |= (1U << (7 - (i % 8)));
        }

        if (i < 39)
        {
            if (!DHT11_Wait(GPIO_PIN_RESET, 100))
                return 0;
        }
    }

    /* Checksum verification */
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
    {
        return 0;
    }

    *humidity = data[0];
    *temperature = data[2];

    return 1;
}

/* LCD ENABLE PULSE */
void LCD_PulseEnable(uint8_t data)
{
    uint8_t temp;

    temp = data | LCD_ENABLE;

    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, &temp, 1, 100);

    HAL_Delay(1);

    temp = data & ~LCD_ENABLE;

    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, &temp, 1, 100);

    HAL_Delay(1);
}

/* LCD 4-BIT WRITE */
void LCD_Write4Bits(uint8_t data)
{
    uint8_t temp = data | LCD_BACKLIGHT;

    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, &temp, 1, 100);

    LCD_PulseEnable(temp);
}

/* LCD SEND */
void LCD_Send(uint8_t value, uint8_t mode)
{
    uint8_t high = value & 0xF0;
    uint8_t low = (value << 4) & 0xF0;

    LCD_Write4Bits(high | mode);
    LCD_Write4Bits(low | mode);
}

/* LCD COMMAND */
void LCD_Command(uint8_t command)
{
    LCD_Send(command, 0x00);
}

/* LCD DATA */
void LCD_Data(uint8_t data)
{
    LCD_Send(data, LCD_RS);
}

/* LCD INITIALIZATION */
void LCD_Init(void)
{
    HAL_Delay(50);

    LCD_Write4Bits(0x30);
    HAL_Delay(5);

    LCD_Write4Bits(0x30);
    HAL_Delay(5);

    LCD_Write4Bits(0x30);
    HAL_Delay(5);

    LCD_Write4Bits(0x20);
    HAL_Delay(5);

    LCD_Command(0x28);
    LCD_Command(0x0C);
    LCD_Command(0x06);
    LCD_Command(0x01);

    HAL_Delay(5);
}

/* LCD PRINT */
void LCD_Print(const char *text)
{
    while (*text)
    {
        LCD_Data((uint8_t)*text);
        text++;
    }
}

/* I2C INITIALIZATION */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x00100D14;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* GPIO INITIALIZATION */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* DHT11 PA1 */
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);

    /* I2C1 PB8 SCL and PB9 SDA */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* SYSTEM CLOCK - HSI 16 MHz */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(
        PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,
                            FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ERROR HANDLER */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
