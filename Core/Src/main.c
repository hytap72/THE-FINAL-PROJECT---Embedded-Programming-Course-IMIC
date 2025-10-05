/*


Bài tập cuối kỳ: lập trình firmware có os(freeRTOS) hoặc non-os với các tính nâng sao.
1. sử dụng uart:
* “led on”: để turn on led
* “led off”: để turn off led
* “update”: để update firmware chương trình chóp tắt led pd12 chu kì 1s
2. mỗi giây đo nhiệt độ chip một lần gửi dữ liệu nhiệt độ qua uart
3. tính nâng update firmware qua uart
	firmware1: điều khiển led thông qua uart (file bin size:
	firmware2: chớp tắt led


*/

#include "main.h"
#include <string.h>
#include <stdio.h>

#define GPIOD_BASE_ADDR 0x40020C00
#define GPIOB_BASE_ADDR 0x40020400
#define USART1_BASE_ADDR 0x40011000
#define NVIC_ISER_BASE_ADDR 0xE000E100

#define Flash_Base_ADDR 0x40023C00
#define Flash_SR_Offset 0x0C
#define Flash_CR_Offet 0x10
#define Flash_Keyr_Offset 0x04
#define KEY1 0x45670123
#define KEY2 0xCDEF89AB
#define Flash_CR_ADDR (Flash_Base_ADDR + Flash_CR_Offet)
#define Flash_SR_ADDR (Flash_Base_ADDR + Flash_SR_Offset)
#define FLASH_KEYR_ADDR (Flash_Base_ADDR + Flash_Keyr_Offset)

#define ADC1_BASE_ADDR 0x40012000
#define TIM1_Base_Addr 0x40010000

//Khởi tạo Led PD12
void LED_Init(){
	__HAL_RCC_GPIOD_CLK_ENABLE();
	uint32_t* GPIOD_MODER = (uint32_t*) (GPIOD_BASE_ADDR + 0x00);
	*GPIOD_MODER &=~(0b11 << 12*2);
	*GPIOD_MODER |=(0b01 << 12*2);
}

//Khởi tạo UART1 PB6-Tx, PB7_Rx
void USART1_Init(){
	__HAL_RCC_GPIOB_CLK_ENABLE();
	uint32_t* GPIOB_MODER = (uint32_t*)(GPIOB_BASE_ADDR + 0x00);
	*GPIOB_MODER &=~(0b1111 << 6*2);
	*GPIOB_MODER |= (0b1010 << 6*2); //chuyển sang alternative function mode để sử dụng các chân usart
	uint32_t* GPIOB_AFRL = (uint32_t*)(GPIOB_BASE_ADDR + 0x20);
	*GPIOB_AFRL &=~((0b1111 << (6*4)) | (0b1111 << (7*4)));
	*GPIOB_AFRL |= (0b0111 << (6*4)) | (0b0111 << (7*4));

	__HAL_RCC_USART1_CLK_ENABLE();
	uint32_t* USART1_CR1 = (uint32_t*)(USART1_BASE_ADDR + 0x0C);
	*USART1_CR1 |= (1 << 5);
	*USART1_CR1 &=~ ((1 << 9) | (1 << 10) | (1 << 12));
	uint32_t* USART1_BRR = (uint32_t*)(USART1_BASE_ADDR + 0x08);
	*USART1_BRR = (104 << 4) | (3 << 0); //baud rate 9600
	*USART1_CR1 |= (1 << 2) | (1 << 3) | (1 << 13);

	uint32_t* NVIC_ISER1 = (uint32_t*)(NVIC_ISER_BASE_ADDR + 0x4);
	*NVIC_ISER1 |= (1 << 5); //position 37 thanh ghi iser1
}

//Khởi tạo ADC
void ADC_Init(){
	__HAL_RCC_ADC1_CLK_ENABLE();
	uint32_t* ADC_JSQR = (uint32_t*)(ADC1_BASE_ADDR + 0x38);
	uint32_t* ADC_CR2 = (uint32_t*)(ADC1_BASE_ADDR + 0x08);
	uint32_t* ADC_CCR = (uint32_t*)(ADC1_BASE_ADDR + 0x300 + 0x04);
	uint32_t* ADC_SMPR1 = (uint32_t*)(ADC1_BASE_ADDR + 0x0C);



	*ADC_JSQR &=~ (0b00 << 20);
	*ADC_JSQR |= (16 << 15);
	*ADC_CR2 |= (0b1 << 0);
	*ADC_CCR |= (0b1 << 23);
	*ADC_SMPR1 |= (0b001 << 18);
}

uint16_t temp_sensor_data_raw = 0;
float temp_sensor_data_voltage = 0, temp_sensor_value = 0;
const float vref = 3.0;
const float v_25 = 0.76;
const float avg_slope = 0.0025;

uint16_t ADC_Get_Val(){
	uint32_t* ADC_CR2 = (uint32_t*)(ADC1_BASE_ADDR + 0x08);
	uint32_t* ADC_SR = (uint32_t*)(ADC1_BASE_ADDR + 0x00);
	uint32_t* ADC_JDR1 = (uint32_t*)(ADC1_BASE_ADDR + 0x3C);

	*ADC_CR2 &=~(0b1 << 22);
	*ADC_CR2 |= (0b1 << 22); //Bật trigger đo injected group
	while(!((*ADC_SR >> 2) & 1));
	*ADC_SR &=~ (0b1 << 2);
	return (uint16_t)(*ADC_JDR1);
}

void Get_Temp_Value(){
	temp_sensor_data_raw = ADC_Get_Val();
	temp_sensor_data_voltage = (vref * temp_sensor_data_raw)/4095;
	temp_sensor_value = ((temp_sensor_data_voltage - v_25)/avg_slope) + 25;
}

char string_temp_value[12];
//Xử lý giá trị cảm biển nhiệt độ thành kí tự
void float_to_string(float value, char* buffer, int precision){
	int i = 0;

	if(value < 0){
		buffer[i++] = '-';
		value = -value;
	}

	int int_part = (int) value;
	float float_part = value - int_part;

	//Phần số nguyên
	char tmp[10];
	uint8_t j = 0;
	while(int_part > 0){
		tmp[j++] = (int_part % 10) + '0';
		int_part /= 10;
	}
	for(int k = j - 1 ; k >= 0 ; k--){
		buffer[i++] = tmp[k];
	}
	buffer[i++] = '.';

	//Phần thập phân
	for(int p = 0; p < precision; p++){
	    float_part *= 10;
	    int digit = (int)float_part;
	    buffer[i++] = digit + '0';
	    float_part -= digit;
	}

	// Kết thúc chuỗi
	buffer[i] = '\0';
}

//Khởi tạo timer
void TIM_Init(){
	__HAL_RCC_TIM1_CLK_ENABLE();
	uint32_t* TIM1_CR1 = (uint32_t*)(TIM1_Base_Addr + 0x00);
	uint32_t* TIM1_PSC = (uint32_t*)(TIM1_Base_Addr + 0x28);
	uint32_t* TIM1_ARR = (uint32_t*)(TIM1_Base_Addr + 0x2C);
	uint32_t* TIM1_DIER = (uint32_t*)(TIM1_Base_Addr + 0x0C);
	*TIM1_PSC = 16000 - 1;
	*TIM1_ARR = 1000;
	*TIM1_DIER |= (1 << 0);
	*TIM1_CR1 |= (1 << 0);

	uint32_t* NVIC_ISER0 = (uint32_t*)NVIC_ISER_BASE_ADDR;
	*NVIC_ISER0 = (1 << 25);
}

void TIM1_UP_TIM10_IRQHandler(){
	uint32_t* TIM1_SR = (uint32_t*)(TIM1_Base_Addr + 0x10);
	*TIM1_SR &= ~(1 << 0); // xóa cờ UIF
	USART_Multi_Send("Gia tri cam bien nhiet do: ");
	USART_Multi_Send(string_temp_value);
	USART_Multi_Send(" C\r\n");
}

void TIM1_Disable(){
	uint32_t* TIM1_CR1 = (uint32_t*)(TIM1_Base_Addr + 0x00);
	uint32_t* NVIC_ISER0 = (uint32_t*)NVIC_ISER_BASE_ADDR;

	*TIM1_CR1 &=~ (1 << 0);
	*NVIC_ISER0 = (0 << 25);
}

void USART_Send(char data){
	uint32_t* USART1_DR = (uint32_t*)(USART1_BASE_ADDR + 0x04);
	uint32_t* USART1_SR = (uint32_t*)(USART1_BASE_ADDR + 0x00);
	while(!((*USART1_SR >> 7 ) & 1));
	*USART1_DR = data;
	while(!((*USART1_SR >> 6) & 1));
}

void USART_Multi_Send(char* data){
	int str_len = strlen(data);
	for(int i = 0 ; i < str_len; i++){
		USART_Send(data[i]);
	}
}

char rx_buffer[2228];
uint16_t rx_index = 0;
uint8_t led_flag = 0, update_flag = 0, rx_done = 0, rx_firmware_done = 0;

void USART1_IRQHandler() {
    uint32_t* UART1_SR = (uint32_t*)(USART1_BASE_ADDR + 0x00);
    uint32_t* UART1_DR = (uint32_t*)(USART1_BASE_ADDR + 0x04);
    if((*UART1_SR >> 5) & 1){
    	char receive_data = (char)(*UART1_DR & 0xFF);
    	if(!update_flag){
    		if(rx_index <= (2228-1)){
    			rx_buffer[rx_index++] = receive_data;
    		    if(receive_data == '\n' || receive_data == '\r'){
    		    	rx_buffer[rx_index - 1] = '\0'; // kết thúc chuỗi
    		    	rx_index = 0;
    		    	rx_done = 1;
    		    }
    		}
    	}
    	else if(update_flag){
    		rx_buffer[rx_index++] = receive_data;
    		if(rx_index >= 2228)
    			rx_firmware_done = 1;
    	}
    }
}

void Check_Msg(char* rx_buffer){
	if(rx_done){
		if((strcmp(rx_buffer,"led on") == 0)||(strcmp(rx_buffer,"led off") == 0)){
			led_flag = 1;
			update_flag = 0;
		}
		else if(strcmp(rx_buffer,"update") == 0){
			led_flag = 0;
			update_flag = 1;
		}
		rx_done = 0;
	}
}

#if 1
__attribute__((section(".Ham_tren_Ram"))) void Unclock_flash(){
	uint32_t* Flash_CR = (uint32_t*)Flash_CR_ADDR;
	uint32_t* Flash_KEY = (uint32_t*)FLASH_KEYR_ADDR;
	//Kiem tra xem flash co bi khoa khong
	if(((*Flash_CR >> 31) & 1) == 1){
		*Flash_KEY = KEY1;
		*Flash_KEY = KEY2;
	}
}

__attribute__((section(".Ham_tren_Ram"))) void Erase_flash(int sector){
	uint32_t* Flash_CR = (uint32_t*)Flash_CR_ADDR;
	uint32_t* Flash_SR = (uint32_t*)Flash_SR_ADDR;
	Unclock_flash();
	while(((*Flash_SR >> 16) & 1) == 1);
	*Flash_CR |= 1 << 1;
	*Flash_CR &= ~ (0xF << 3);
	*Flash_CR |= ((sector & 0xF) << 3);
	*Flash_CR |= (1 << 16);
	while(((*Flash_SR >> 16) & 1) == 1);
}

__attribute__((section(".Ham_tren_Ram"))) void Program_Flash(char* address, char* data_buf, uint16_t size){
	uint32_t* Flash_CR = (uint32_t*)Flash_CR_ADDR;
	uint32_t* Flash_SR = (uint32_t*)Flash_SR_ADDR;
	Unclock_flash();
	while(((*Flash_SR >> 16) & 1) == 1);
	*Flash_CR |= 1 << 0;
	for(int i = 0 ; i < size ; i++){
		address[i] = data_buf[i];
	}
	while(((*Flash_SR >> 16) & 1) == 1);
}

__attribute__((section(".Ham_tren_Ram"))) void update(){
	__asm("CPSID i"); //tắt các interupt bằng assembly
	Erase_flash(0);
	Program_Flash((char*)0x08000000, rx_buffer, 2228);

	//reset system
	uint32_t* AIRCR = (uint32_t*)0xE000ED0C;
	*AIRCR = ( 0x5FA << 16) | (1 << 2);
}
#endif

void Ctrl_Led(char* signal){
	uint32_t* GPIOD_ODR = (uint32_t*)(GPIOD_BASE_ADDR + 0x14);

	if(!(strcmp(signal,"led on"))){
		*GPIOD_ODR |= (1 << 12);
		memset(rx_buffer, 0, 2228);
	}
	else if(!(strcmp(signal, "led off"))){
		*GPIOD_ODR &=~(1 << 12);
		memset(rx_buffer, 0, 2228);
	}
}


int main(){
	HAL_Init();
	USART1_Init();
	LED_Init();
	ADC_Init();
	TIM_Init();
	char msg[] = "Chay chuong trinh 1\r\n";
	HAL_Delay(1000);
	USART_Multi_Send(msg);
	while(1){
		Get_Temp_Value();
		float_to_string(temp_sensor_value, string_temp_value, 2);
		Check_Msg(rx_buffer);
		if(led_flag){
			Ctrl_Led(rx_buffer);
		}
		else if(update_flag){
			TIM1_Disable();
			USART_Multi_Send("Dang cho chuong trinh moi\r\n");
			while(!rx_firmware_done);
			update();
		}
	}
}
