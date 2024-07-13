#include "main.h"
#include "cmsis_os.h"
#include "uart1.h"
#include "i2c1.h"
#include "gpio.h"
#include "tim.h"
#include "ds18b20Config.h"
#include "ds18b20.h"
#include "onewire.h"
#include "ssd1306_conf.h"
#include "ssd1306_fonts.h"
#include "ssd1306.h"
#include "string.h"
#include <stdbool.h>
#include <stdio.h>

#define temperatureRead ds18b20[0].Temperature
#define stepperSpeed 2

/* Global Variables -----------------------------------------------*/
bool startFlag=0;
int8_t chooseMenu = 0;
int8_t SV = 0;
float PV = 0;
char PVstring[12];
char SVstring[12];
int8_t rotation = 1, interval = 1, timePeriod = 2;
char rotationString[20], intervalString[20], timePeriodString[20];

/* Private variables ---------------------------------------------------------*/

osThreadId defaultTaskHandle;
osThreadId userInterfaceTHandle;
osThreadId readTempTHandle;
osThreadId HCCtrlTHandle;
osThreadId timePeriodTickTHandle;
osThreadId stepCtrlTHandle;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void DS18B20_Init(void);
void StartDefaultTask(void const * argument);
void userInterface(void const * argument);
void readTemp(void const * argument);
void HCCtrl(void const * argument);
void timePeriodTick(void const * argument);
void stepCtrl(void const * argument);

int main(void)
{

  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();

  ssd1306_Init();
  ssd1306_SetCursor(5, 0);
  ssd1306_WriteString("Program", Font_11x18, White);
  ssd1306_SetCursor(5, 20);
  ssd1306_WriteString("Started", Font_11x18, White);
  ssd1306_UpdateScreen();

  /* definition and creation of userInterfaceT */
  osThreadDef(userInterfaceT, userInterface, osPriorityLow, 0, 128);
  userInterfaceTHandle = osThreadCreate(osThread(userInterfaceT), NULL);

  /* definition and creation of readTempT */
  osThreadDef(readTempT, readTemp, osPriorityLow, 0, 128);
  readTempTHandle = osThreadCreate(osThread(readTempT), NULL);

  /* definition and creation of HCCtrlT */
  osThreadDef(HCCtrlT, HCCtrl, osPriorityLow, 0, 128);
  HCCtrlTHandle = osThreadCreate(osThread(HCCtrlT), NULL);

  /* definition and creation of timePeriodTickT */
  osThreadDef(timePeriodTickT, timePeriodTick, osPriorityLow, 0, 128);
  timePeriodTickTHandle = osThreadCreate(osThread(timePeriodTickT), NULL);

  /* definition and creation of stepCtrlT */
  osThreadDef(stepCtrlT, stepCtrl, osPriorityLow, 0, 128);
  stepCtrlTHandle = osThreadCreate(osThread(stepCtrlT), NULL);

  Ds18b20_Init(osPriorityNormal);

  /* Start scheduler */
  osKernelStart();

  while (1)
  {
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_I2C1
                              |RCC_PERIPHCLK_TIM1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.Tim1ClockSelection = RCC_TIM1CLK_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

void userInterface(void const * argument)
{
  #define tempUpperLimit 100

  ssd1306_Init();

  while (1)
  {
	if (HAL_GPIO_ReadPin(HOME_BTN_GPIO_Port, HOME_BTN_Pin) == 0){
		chooseMenu = 0;
	}

	if (HAL_GPIO_ReadPin(MENU_BTN_GPIO_Port, MENU_BTN_Pin) == 0){
		chooseMenu++;

		if (chooseMenu > 4){
			chooseMenu = 0;
		}
	}
	if (chooseMenu == 0){
		ssd1306_Fill(Black);
		ssd1306_SetCursor(5, 0);
		ssd1306_WriteString("PHOTOGRAPH", Font_11x18, White);
		ssd1306_SetCursor(5, 20);
		ssd1306_WriteString("PROCESSING", Font_11x18, White);

		if (HAL_GPIO_ReadPin(UP_BTN_GPIO_Port, UP_BTN_Pin) == 0){
			startFlag=1;
		}
		else if (HAL_GPIO_ReadPin(DOWN_BTN_GPIO_Port, DOWN_BTN_Pin) == 0){
			startFlag=0;
		}

		ssd1306_SetCursor(5, 40);
		if (startFlag == 1){
			ssd1306_WriteString("START", Font_11x18, White);
			ssd1306_Line(5, 55, 60, 55, White);
			ssd1306_UpdateScreen();
			ssd1306_Line(5, 55, 60, 55, Black);
			ssd1306_UpdateScreen();
		}
		else if (startFlag == 0){
			ssd1306_WriteString("STOP", Font_11x18, White);
			ssd1306_Line(5, 55, 45, 55, White);
			ssd1306_UpdateScreen();
			ssd1306_Line(5, 55, 45, 55, Black);
			ssd1306_UpdateScreen();
		}


	}

	else if (chooseMenu == 1){
		ssd1306_Fill(Black);
		ssd1306_SetCursor(5, 0);
		ssd1306_WriteString("TEMPERATURE:", Font_11x18, White);

		if (HAL_GPIO_ReadPin(UP_BTN_GPIO_Port, UP_BTN_Pin) == 0){
			SV++;
			if (SV > tempUpperLimit){
				SV = 0;
			}
		}
		else if (HAL_GPIO_ReadPin(DOWN_BTN_GPIO_Port, DOWN_BTN_Pin) == 0){
			SV--;
			if (SV < 0){
				SV = tempUpperLimit;
			}
		}

		ssd1306_SetCursor(5, 20);
		ssd1306_WriteString(PVstring, Font_11x18, White);

		memset(SVstring, 0, sizeof(SVstring));
		sprintf(SVstring, "SV: %d C", SV);
		ssd1306_SetCursor(5, 40);
		ssd1306_WriteString(SVstring, Font_11x18, White);

		ssd1306_Line(5, 55, 25, 55, White);
		ssd1306_UpdateScreen();
		ssd1306_Line(5, 55, 25, 55, Black);
		ssd1306_UpdateScreen();
	}

	else if (chooseMenu == 2){
		ssd1306_Fill(Black);
		ssd1306_SetCursor(5, 0);
		ssd1306_WriteString("MIXING:", Font_11x18, White);

		if (HAL_GPIO_ReadPin(UP_BTN_GPIO_Port, UP_BTN_Pin) == 0){
			rotation++;
			if (rotation > 99){
				rotation = 0;
			}
		}

		else if (HAL_GPIO_ReadPin(DOWN_BTN_GPIO_Port, DOWN_BTN_Pin) == 0){
			rotation--;
			if (rotation < 0){
				rotation = 99;
			}
		}

		memset(rotationString, 0, sizeof(rotationString));
		sprintf(rotationString, "Rotation: %d", rotation);
		ssd1306_SetCursor(5, 20);
		ssd1306_WriteString(rotationString, Font_7x10, White);

		memset(intervalString, 0, sizeof(intervalString));
		sprintf(intervalString, "Interval: %dmin", interval);
		ssd1306_SetCursor(5, 35);
		ssd1306_WriteString(intervalString, Font_7x10, White);

		memset(timePeriodString, 0, sizeof(timePeriodString));
		sprintf(timePeriodString, "TimePeriod: %dmin", timePeriod);
		ssd1306_SetCursor(5, 50);
		ssd1306_WriteString(timePeriodString, Font_7x10, White);


		ssd1306_Line(5, 30, 60, 30, White);
		ssd1306_UpdateScreen();
		ssd1306_Line(5, 30, 60, 30, Black);
		ssd1306_UpdateScreen();
	}

	else if (chooseMenu == 3){
		ssd1306_Fill(Black);
		ssd1306_SetCursor(5, 0);
		ssd1306_WriteString("MIXING:", Font_11x18, White);

		if (HAL_GPIO_ReadPin(UP_BTN_GPIO_Port, UP_BTN_Pin) == 0){
			interval++;
			if (interval > 60){
				interval = 0;
			}
		}

		else if (HAL_GPIO_ReadPin(DOWN_BTN_GPIO_Port, DOWN_BTN_Pin) == 0){
			interval--;
			if (interval < 0){
				interval = 60;
			}
		}

		memset(rotationString, 0, sizeof(rotationString));
		sprintf(rotationString, "Rotation: %d", rotation);
		ssd1306_SetCursor(5, 20);
		ssd1306_WriteString(rotationString, Font_7x10, White);

		memset(intervalString, 0, sizeof(intervalString));
		sprintf(intervalString, "Interval: %dmin", interval);
		ssd1306_SetCursor(5, 35);
		ssd1306_WriteString(intervalString, Font_7x10, White);

		memset(timePeriodString, 0, sizeof(timePeriodString));
		sprintf(timePeriodString, "TimePeriod: %dmin", timePeriod);
		ssd1306_SetCursor(5, 50);
		ssd1306_WriteString(timePeriodString, Font_7x10, White);

		ssd1306_Line(5, 45, 60, 45, White);
		ssd1306_UpdateScreen();
		ssd1306_Line(5, 45, 60, 45, Black);
		ssd1306_UpdateScreen();
	}

	else if (chooseMenu == 4){
		ssd1306_Fill(Black);
		ssd1306_SetCursor(5, 0);
		ssd1306_WriteString("MIXING:", Font_11x18, White);

		if (HAL_GPIO_ReadPin(UP_BTN_GPIO_Port, UP_BTN_Pin) == 0){
			timePeriod++;
			if (timePeriod > 60){
				timePeriod = 0;
			}
		}

		else if (HAL_GPIO_ReadPin(DOWN_BTN_GPIO_Port, DOWN_BTN_Pin) == 0){
			timePeriod--;
			if (timePeriod < 0){
				timePeriod = 60;
			}
		}

		memset(rotationString, 0, sizeof(rotationString));
		sprintf(rotationString, "Rotation: %d", rotation);
		ssd1306_SetCursor(5, 20);
		ssd1306_WriteString(rotationString, Font_7x10, White);

		memset(intervalString, 0, sizeof(intervalString));
		sprintf(intervalString, "Interval: %dmin", interval);
		ssd1306_SetCursor(5, 35);
		ssd1306_WriteString(intervalString, Font_7x10, White);

		memset(timePeriodString, 0, sizeof(timePeriodString));
		sprintf(timePeriodString, "TimePeriod: %dmin", timePeriod);
		ssd1306_SetCursor(5, 50);
		ssd1306_WriteString(timePeriodString, Font_7x10, White);

		ssd1306_Line(5, 60, 80, 60, White);
		ssd1306_UpdateScreen();
		ssd1306_Line(5, 60, 80, 60, Black);
		ssd1306_UpdateScreen();
	}
  }
  /* USER CODE END 5 */
}

void readTemp(void const * argument)
{
	while (1)
	{
		memset(PVstring, 0, sizeof(PVstring));
		sprintf(PVstring, "PV: %d C", (int)(temperatureRead));
		osDelay(1);
	}
}

uint8_t hysteresis(uint8_t input, uint8_t lowerThreshold, uint8_t upperThreshold, uint8_t previousState) {
    if (input > upperThreshold) {
        return 1;  // High state
    } else if (input < lowerThreshold) {
        return 0;  // Low state
    } else {
        return previousState;  // Maintain previous state within hysteresis band
    }
}

void HCCtrl(void const * argument)
{
	static int currentState = 0;

    while (1) {
        currentState = hysteresis((int)(temperatureRead), SV, SV, currentState);
        HAL_GPIO_WritePin(HC_GPIO_Port, HC_Pin, currentState);
        osDelay(1);
    }
}

void stepCycle(void)
{
	HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, 0);
	for(int steps = 0; steps <= 200; steps++) {
		HAL_GPIO_WritePin(STEP_GPIO_Port, STEP_Pin, 1);
		osDelay(stepperSpeed);
		HAL_GPIO_WritePin(STEP_GPIO_Port, STEP_Pin, 0);
		osDelay(stepperSpeed);
		if(startFlag == 0){
			break;
		}
	}
	osDelay(1);
	HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, 1);
	for(int steps = 0; steps <= 200; steps++) {
		HAL_GPIO_WritePin(STEP_GPIO_Port, STEP_Pin, 1);
		osDelay(stepperSpeed);
		HAL_GPIO_WritePin(STEP_GPIO_Port, STEP_Pin, 0);
		osDelay(stepperSpeed);
		if(startFlag == 0){
			break;
		}
	}
	osDelay(1);
}

void timePeriodTick(void const *argument)
{
  uint16_t timePeriodTicks=0;

  for(;;)
  {
	if(startFlag == 1 && timePeriod > 0)
	{
		osDelay(1); //20,000 is equal 1min
		timePeriodTicks++;

		if(timePeriodTicks%(60000-1600) == 0){
			timePeriod--;
			timePeriodTicks = 0;
		}
	}
	else if(startFlag == 1 && timePeriod == 0)
	{
		startFlag = 0;
	}
  }
}


void stepCtrl(void const *argument)
{
  for(;;)
  {
    while(startFlag == 1 && timePeriod > 0)
    {
    	for(int loop=0; loop<rotation;	loop++)
    	{
    		stepCycle();
    	}
    	for(uint32_t delay=0; delay<interval*(60000-1600);	delay++)
    	{
    		osDelay(1);
    		if(startFlag == 0){
    			break;
    		}
    	}
	}
  }
  /* USER CODE END stepCtrl */
}

/* USER CODE BEGIN Header_pumpCtrl */
/**
* @brief Function implementing the pumpCtrlT thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_pumpCtrl */
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
