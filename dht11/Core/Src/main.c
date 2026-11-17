#include <stdio.h>
#include <string.h>
#include "main.h"
void DHT11_Init(void);
uint8_t DHT11_Read(uint8_t *data);
void delay_us(uint32_t us);
uint8_t DHT11_WaitFor(GPIO_PinState state,
                      uint32_t timeout_us);


UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();

    MX_USART2_UART_Init();

    DHT11_Init();

    char message[100];
    uint8_t data[5];

    while (1)
    {
        if (DHT11_Read(data))
        {
            uint8_t humidity = data[0];
            uint8_t temperature = data[2];

            snprintf(message, sizeof(message),
                     "Temperature: %d C\r\nHumidity: %d %%\r\n",
                     temperature, humidity);
        }
        else
        {
            snprintf(message, sizeof(message),
                     "DHT11 Sensor Error!\r\n");
        }

        HAL_UART_Transmit(&huart2,
                          (uint8_t *)message,
                          strlen(message),
                          HAL_MAX_DELAY);

        HAL_Delay(2000);
    }
}
void DHT11_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;

    uint32_t cycles =
        us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < cycles);
}

static void DHT11_SetPinOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void DHT11_SetPinInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

uint8_t DHT11_WaitFor(GPIO_PinState state,
                      uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;

    uint32_t timeout =
        timeout_us * (SystemCoreClock / 1000000U);

    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) != state)
    {
        if ((DWT->CYCCNT - start) > timeout)
            return 0;
    }

    return 1;
}

uint8_t DHT11_Read(uint8_t *data)
{
    uint32_t start;
    uint32_t pulse;

    memset(data, 0, 5);

    DHT11_SetPinOutput();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_Delay(18);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    delay_us(30);

    DHT11_SetPinInput();

    if (!DHT11_WaitFor(GPIO_PIN_RESET, 100)) return 0;
    if (!DHT11_WaitFor(GPIO_PIN_SET, 100)) return 0;
    if (!DHT11_WaitFor(GPIO_PIN_RESET, 100)) return 0;

    for (int i = 0; i < 40; i++)
    {
        if (!DHT11_WaitFor(GPIO_PIN_SET, 100))
            return 0;

        start = DWT->CYCCNT;

        if (!DHT11_WaitFor(GPIO_PIN_RESET, 100))
            return 0;

        pulse = DWT->CYCCNT - start;

        pulse /= (SystemCoreClock / 1000000U);

        data[i / 8] <<= 1;

        if (pulse > 40)
            data[i / 8] |= 1;
    }

    if (data[4] == (uint8_t)(data[0] + data[1] +
                             data[2] + data[3]))
        return 1;

    return 0;
}
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 10;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
