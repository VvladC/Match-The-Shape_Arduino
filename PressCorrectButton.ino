#include <Wire.h>
#include <U8g2lib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "esp_task_wdt.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define BUTTON1 23
#define BUTTON2 19
#define BUTTON3 18

#define SHAPE_TRIANGLE 0
#define SHAPE_CIRCLE   1
#define SHAPE_SQUARE   2

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

SemaphoreHandle_t i2cMutex;
SemaphoreHandle_t newShapeSemaphore;
SemaphoreHandle_t gameOverSemaphore;

volatile int      currentShape = SHAPE_TRIANGLE;
volatile int      score        = 0;
volatile bool     gameRunning  = false;
volatile uint32_t gameStartMs  = 0;

void drawBottomIcons() {
  display.drawLine(4,  63, 14, 63);
  display.drawLine(4,  63,  9, 54);
  display.drawLine(14, 63,  9, 54);
  display.drawCircle(64, 58, 5);
  display.drawFrame(110, 53, 10, 10);
}

void drawBigShape(int shape) {
  switch (shape) {
    case SHAPE_TRIANGLE:
      display.drawLine(49, 44, 79, 44);
      display.drawLine(49, 44, 64, 14);
      display.drawLine(79, 44, 64, 14);
      break;
    case SHAPE_CIRCLE:
      display.drawCircle(64, 29, 15);
      break;
    case SHAPE_SQUARE:
      display.drawFrame(49, 14, 30, 30);
      break;
  }
}

uint32_t calcRemaining(uint32_t elapsedMs) {
  return (elapsedMs < 60000) ? (60000 - elapsedMs) : 0;
}

void drawTimer(uint32_t elapsedMs) {
  uint32_t remaining = calcRemaining(elapsedMs);
  uint32_t secs      = remaining / 1000;
  uint32_t centis    = (remaining % 1000) / 10;
  char timerBuf[8];
  sprintf(timerBuf, "%02lu.%02lu", (unsigned long)secs, (unsigned long)centis);
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(50, 10, timerBuf);
}

void drawGameScreen(int shape, uint32_t elapsedMs) {
  xSemaphoreTake(i2cMutex, portMAX_DELAY);
  display.clearBuffer();
  drawTimer(elapsedMs);
  drawBigShape(shape);
  drawBottomIcons();
  display.sendBuffer();
  xSemaphoreGive(i2cMutex);
}

void drawBlankScreen(uint32_t elapsedMs) {
  xSemaphoreTake(i2cMutex, portMAX_DELAY);
  display.clearBuffer();
  drawTimer(elapsedMs);
  drawBottomIcons();
  display.sendBuffer();
  xSemaphoreGive(i2cMutex);
}

volatile uint32_t lastBtn1 = 0;
volatile uint32_t lastBtn2 = 0;
volatile uint32_t lastBtn3 = 0;

void IRAM_ATTR onBtn1() {
  uint32_t now = millis();
  if (now - lastBtn1 < 150) return;
  lastBtn1 = now;
  if (!gameRunning) return;
  if (currentShape == SHAPE_TRIANGLE) {
    score++;
    BaseType_t w = pdFALSE;
    xSemaphoreGiveFromISR(newShapeSemaphore, &w);
    portYIELD_FROM_ISR(w);
  } else {
    if (score > 0) score--;
  }
}

void IRAM_ATTR onBtn2() {
  uint32_t now = millis();
  if (now - lastBtn2 < 150) return;
  lastBtn2 = now;
  if (!gameRunning) return;
  if (currentShape == SHAPE_CIRCLE) {
    score++;
    BaseType_t w = pdFALSE;
    xSemaphoreGiveFromISR(newShapeSemaphore, &w);
    portYIELD_FROM_ISR(w);
  } else {
    if (score > 0) score--;
  }
}

void IRAM_ATTR onBtn3() {
  uint32_t now = millis();
  if (now - lastBtn3 < 150) return;
  lastBtn3 = now;
  if (!gameRunning) return;
  if (currentShape == SHAPE_SQUARE) {
    score++;
    BaseType_t w = pdFALSE;
    xSemaphoreGiveFromISR(newShapeSemaphore, &w);
    portYIELD_FROM_ISR(w);
  } else {
    if (score > 0) score--;
  }
}

void gameTask(void *pvParameters) {
  esp_task_wdt_delete(NULL);
  srand(esp_random());

  for (int cd = 3; cd >= 0; cd--) {
    xSemaphoreTake(i2cMutex, portMAX_DELAY);
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB14_tr);
    display.drawStr(26, 28, "Ready?");
    if (cd > 0) {
      char buf[2];
      sprintf(buf, "%d", cd);
      display.drawStr(58, 52, buf);
    }
    drawBottomIcons();
    display.sendBuffer();
    xSemaphoreGive(i2cMutex);
    if (cd > 0) vTaskDelay(pdMS_TO_TICKS(1000));
  }

  xSemaphoreTake(newShapeSemaphore, 0);

  currentShape = rand() % 3;
  score        = 0;
  gameRunning  = true;
  gameStartMs  = millis();

  drawGameScreen(currentShape, 0);

  while (true) {
    uint32_t elapsed = millis() - gameStartMs;
    if (elapsed >= 60000) {
      gameRunning = false;
      xSemaphoreGive(gameOverSemaphore);
      break;
    }

    if (xSemaphoreTake(newShapeSemaphore, 0) == pdTRUE) {
      drawBlankScreen(millis() - gameStartMs);
      vTaskDelay(pdMS_TO_TICKS(100));
      currentShape = rand() % 3;
      xSemaphoreTake(newShapeSemaphore, 0);
    }

    drawGameScreen(currentShape, millis() - gameStartMs);
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  vTaskDelete(NULL);
}

void displayTask(void *pvParameters) {
  xSemaphoreTake(gameOverSemaphore, portMAX_DELAY);

  char line1[24];
  sprintf(line1, "You got %d points!", score);

  xSemaphoreTake(i2cMutex, portMAX_DELAY);
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(5, 18, "Congratulations!");
  display.drawStr(5, 32, line1);
  display.drawStr(5, 50, "Reset to try again");
  display.sendBuffer();
  xSemaphoreGive(i2cMutex);

  vTaskDelete(NULL);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(BUTTON3, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON1), onBtn1, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON2), onBtn2, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON3), onBtn3, FALLING);

  Wire.begin();
  Wire.setClock(100000);

  display.begin();

  i2cMutex          = xSemaphoreCreateMutex();
  newShapeSemaphore = xSemaphoreCreateBinary();
  gameOverSemaphore = xSemaphoreCreateBinary();

  xTaskCreate(gameTask,    "Game",    8192, NULL, 2, NULL);
  xTaskCreate(displayTask, "Display", 8192, NULL, 1, NULL);
}

void loop() {}