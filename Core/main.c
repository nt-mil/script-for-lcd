/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "main.h"

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
extern EventGroupHandle_t display_event;
/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static const uint8_t test_row_checkerboard_black[ILI9341_BYTES_PER_ROW] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// Pattern 2: Starts with white (0xF), alternates with black (0x0)
static const uint8_t test_row_checkerboard_white[ILI9341_BYTES_PER_ROW] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Task_Init(void);
void test_display(void);

/**
  * @brief  The application entry point.
  * @retval int
  */
extern SPI_HandleTypeDef hspi2;

int main(void)
{
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    Init_Peripherals();

    display_init();

    // test_display();

    process_layout_script();

    Task_Init();

    while(1)
    {

    }
    return 0;
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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

#define ROWS_PER_BAND 20
// Load checkerboard pattern into framebuffer for all 240 rows
void load_test_checkerboard(void *data) {
    if (data == NULL) return;

    uint16_t row;
    ili9341_display_buffer_t* framebuffer = (ili9341_display_buffer_t*)data;

    if (framebuffer->buffer_page[framebuffer->render_page].state == ILI9341_BUFFER_STATE_IDLE) {
        uint8_t* source_buffer = (uint8_t*)&framebuffer->buffer_page[framebuffer->render_page].data[0];
        // Copy alternating row patterns to create checkerboard effect
        for (row = 0; row < ILI9341_HEIGHT; row++) {
            const uint8_t *row_pattern = ((row / ROWS_PER_BAND) % 2 == 0) ? 
                test_row_checkerboard_black : test_row_checkerboard_white;
            memcpy(&source_buffer[row * ILI9341_BYTES_PER_ROW], row_pattern, ILI9341_BYTES_PER_ROW);
        }

        framebuffer->buffer_page[framebuffer->render_page].state = ILI9341_BUFFER_STATE_READY_TO_DISPLAY;
    }
}

void test_display(void)
{
    uint16_t index = get_display_data_bank_index();

    display_info_t* display_info = (display_info_t*)read_from_databank(index);

    // Get framebuffer
    // display_info_t* framebuffer = ili9341_get_driver()->get_framebuffer();
    if (display_info == NULL) {
        // Log error (e.g., via UART)
        return;
    }

    // ili9341_get_driver()->update();

    // HAL_GPIO_WritePin(LD2_GPIO_Port, LCD_BACKLIGHT_PIN, GPIO_PIN_SET);
    // Load checkerboard pattern into framebuffer
    load_test_checkerboard(display_info->data);

    // for (int i = 0; i <ILI9341_HEIGHT; i++)
    // {
    //     HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_DC_PIN, GPIO_PIN_SET);
    //     HAL_GPIO_WritePin(LCD_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    //     HAL_SPI_Transmit_DMA(&hspi2, &framebuffer[i * ILI9341_BYTES_PER_ROW], ILI9341_BYTES_PER_ROW);
    // }

    // Set target state to running to trigger display update
    // extern ili9341_operation_state_t target_state; // Access via extern (or add setter to driver)
    // target_state = ILI9341_STATE_RUNNING;
    // xEventGroupSetBits(display_event, DISPLAY_EVENT_UPDATE);

    // Wait for display to update (handled by display_task)
    // vTaskDelay(pdMS_TO_TICKS(1000)); // Adjust delay as needed
}

/**
 * @brief User Task initialization
 * @retval None
*/
void Task_Init(void)
{
    /* Register display task with the scheduler */
    if (register_scheduler(display_task,
                           "display",
                           512,            /* stack size */
                           NULL,           /* task parameter */
                           5,              /* priority */
                           DISPLAY_EVENT_PERIOD, /* event bits */
                           20) == NULL)
    {
        printf("[Error] Failed to register display task!\n");
        return;
    }

    /* Create scheduler task */
    BaseType_t ret = xTaskCreate(scheduler_task,
                                 "scheduler",
                                 128,        /* stack size */
                                 NULL,       /* task parameter */
                                 2,          /* priority */
                                 NULL);      /* task handle */
    if (ret != pdPASS) {
        printf("[Error] Failed to create scheduler task!\n");
        return;
    }

    /* Start the scheduler */
    printf("[Info] Starting FreeRTOS scheduler...\n");
    vTaskStartScheduler();

    /* If the scheduler exits (should never happen) */
    printf("[Error] Scheduler exited unexpectedly!\n");
}


/**
 * Idle task
*/
void vApplicationIdleHook(void)
{
    printf("idle task running!!!\n");
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM5 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM5) {
    HAL_IncTick();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
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
