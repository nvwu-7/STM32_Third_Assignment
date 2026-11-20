/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include "OLED.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct __attribute__((packed))
{
  uint8_t header;
  uint8_t run;
  uint8_t dir;
  uint16_t interval_ms;
  uint8_t checksum;
} ControlPacket;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

volatile uint8_t key_pa11_event = 0;
volatile uint8_t key_pa12_event = 0;

uint8_t flow_running = 1;
uint8_t flow_reverse = 0;
uint16_t flow_interval_ms = 500;

uint32_t key_pa11_last_tick = 0;
uint32_t key_pa12_last_tick = 0;

ControlPacket rx_packet;
volatile uint8_t packet_ready = 0;

static uint8_t uart_tx_buffer[80];
volatile uint8_t uart_tx_busy = 0;
uint8_t status_feedback_pending = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  uint32_t previousTick = HAL_GetTick();
  uint8_t ledIndex = 0;

  HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);

  HAL_UART_Receive_IT(&huart1,
                      (uint8_t *)&rx_packet,
                      sizeof(rx_packet));

  OLED_Init();

  // OLED_ShowString(1, 1, "STATE: RUN");
  // OLED_ShowString(2, 1, "DIR: FWD");
  // OLED_ShowString(3, 1, "STEP: 0300ms");
  // OLED_ShowString(4, 1, "LED: 1");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    if (packet_ready)
    {
      ControlPacket packet = rx_packet;
      packet_ready = 0;

      const uint8_t *bytes = (const uint8_t *)&packet;

      uint8_t expected = (uint8_t)(
          bytes[0] + bytes[1] + bytes[2] +
          bytes[3] + bytes[4]);

      if (packet.header == 0xAA &&
          packet.checksum == expected &&
          packet.run <= 1U &&
          packet.dir <= 1U &&
          packet.interval_ms >= 100U &&
          packet.interval_ms <= 2000U)
      {
        flow_running = packet.run;
        flow_reverse = packet.dir;
        flow_interval_ms = packet.interval_ms;

        previousTick = HAL_GetTick();
        status_feedback_pending = 1;
      }

      HAL_UART_Receive_IT(&huart1,
                          (uint8_t *)&rx_packet,
                          sizeof(rx_packet));
    }

    uint32_t now_tick = HAL_GetTick();

    // PA11: 暂停 / 继续
    if (key_pa11_event == 1U)
    {
      key_pa11_last_tick = now_tick;
      key_pa11_event = 2U;
    }
    else if (key_pa11_event == 2U &&
             (uint32_t)(now_tick - key_pa11_last_tick) >= 30U)
    {
      key_pa11_event = 0U;

      if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11) == GPIO_PIN_RESET)
      {
        flow_running = !flow_running;
        status_feedback_pending = 1;

        if (flow_running)
        {
          previousTick = now_tick;
        }
      }
    }

    // PA12: 正向 / 反向
    if (key_pa12_event == 1U)
    {
      key_pa12_last_tick = now_tick;
      key_pa12_event = 2U;
    }
    else if (key_pa12_event == 2U &&
             (uint32_t)(now_tick - key_pa12_last_tick) >= 30U)
    {
      key_pa12_event = 0U;

      if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12) == GPIO_PIN_RESET)
      {
        flow_reverse = !flow_reverse;
        status_feedback_pending = 1;
      }
    }


    if (flow_running &&
        (uint32_t)(HAL_GetTick() - previousTick) >= flow_interval_ms)
    {

      previousTick += flow_interval_ms;

      // 先关闭全部 LED
      HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);

      // 切换到下一颗 LED

      if (flow_reverse == 0U)
      {
        ledIndex = (ledIndex + 1U) % 3U;
      }
      else
      {
        ledIndex = (ledIndex + 2U) % 3U;
      }

      if (ledIndex == 0U)
      {
        HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);
      }
      else if (ledIndex == 1U)
      {
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
      }
      else
      {
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
      }
    }


  if (status_feedback_pending && !uart_tx_busy)
  {
    int len = snprintf(
        (char *)uart_tx_buffer,
        sizeof(uart_tx_buffer),
        "STATE=%s,DIR=%s,STEP=%u,LED=%u\r\n",
        flow_running ? "RUN" : "PAUSE",
        flow_reverse ? "REV" : "FWD",
        (unsigned int)flow_interval_ms,
        (unsigned int)(ledIndex + 1U)
    );

    if (len > 0 &&
        len < (int)sizeof(uart_tx_buffer))
    {
      uart_tx_busy = 1;

      if (HAL_UART_Transmit_IT(
              &huart1,
              uart_tx_buffer,
              (uint16_t)len) == HAL_OK)
      {
        status_feedback_pending = 0;
      }
      else
      {
        uart_tx_busy = 0;
      }
    }
  }

  // OLED dynamic status display
  static uint8_t oled_last_run = 255;
  static uint8_t oled_last_dir = 255;
  static uint16_t oled_last_step = 65535;
  static uint8_t oled_last_led = 255;

  // Line 1: RUN / PAUSE
  if (oled_last_run != flow_running)
  {
    OLED_ShowString(1, 1,
        flow_running ? "STATE: RUN   " : "STATE: PAUSE ");
    oled_last_run = flow_running;
  }

  // Line 2: FWD / REV
  if (oled_last_dir != flow_reverse)
  {
    OLED_ShowString(2, 1,
        flow_reverse ? "DIR: REV     " : "DIR: FWD     ");
    oled_last_dir = flow_reverse;
  }

  // Line 3: LED switching interval
  if (oled_last_step != flow_interval_ms)
  {
    OLED_ShowString(3, 1, "STEP: ");
    OLED_ShowNum(3, 7, flow_interval_ms, 4);
    OLED_ShowString(3, 11, "ms");
    oled_last_step = flow_interval_ms;
  }

  // Line 4: Current LED number
  if (oled_last_led != ledIndex)
  {
    OLED_ShowString(4, 1, "LED: ");
    OLED_ShowNum(4, 6, ledIndex + 1U, 1);
    oled_last_led = ledIndex;
  }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_11)
  {
    key_pa11_event = 1;
  }
  else if (GPIO_Pin == GPIO_PIN_12)
  {
    key_pa12_event = 1;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    packet_ready = 1;
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    uart_tx_busy = 0;
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
