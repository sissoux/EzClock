#include "MyQlock.h"
#include "FastLED.h"
#include "Snake.h"

MyQlock::MyQlock()
{
  fill_solid(leds, StripLenght, CRGB::Black);
}

void MyQlock::begin(void)
{
  FastLED.show();
}

uint32_t MyQlock::timeMaskUpdate(uint8_t H, uint8_t M)
{
  uint8_t hours = H % 13 + (H - 1) / 12 + (M >= 35);
  uint8_t Minutes = M / 5;
  uint8_t minutes = M % 5;
  if (H == 23 && M >= 35)
  {
    hours = 0;
  }
  if (H == 12 && M >= 35)
  {
    hours = 1;
  }
  return (uint32_t)1 << (17 + hours) | (uint32_t)1 << (5 + Minutes) | (uint32_t)1 << minutes;
}

void MyQlock::pixelStateUpdate(uint32_t TimeMask)
{
  for (uint8_t x = 0; x < COLUMN_COUNT; x++)
  {
    for (uint8_t y = 0; y < ROW_COUNT; y++)
    {
      this->AbsolutePixelIsOn[y][x] = ((Mask[y][x] & TimeMask) > 0); //Build the absolute coordinate active pixel table
    }
  }
}

void MyQlock::rainbowLoop()
{
  for (int idx = 0; idx < 1000; idx++)
  {
    fill_rainbow(leds, NUM_LEDS, ((float)idx * 360.0) / 1000.0, 5);
    FastLED.show();
    delay(2);
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

TimeChangeType MyQlock::UpdateTime()
{
  uint8_t PrevHour = Hour;
  uint8_t PrevMinute = Minute;
  if ((UnixTimeStamp % 3600) / 60 != Minute)
  {
    Hour = ((UnixTimeStamp+7200)  % 86400L) / 3600;
    Minute = (UnixTimeStamp  % 3600) / 60;
    fxTrigger = true;
  }
  if (PrevHour != Hour) return HourChange;
  if (Minute % 5 == 0) return FiveMinutes;
  if (PrevMinute != Minute) return MinuteChange;
  return NoChange;
}

void MyQlock::applyFX()
{
  /*for (uint8_t x = 0; x < COLUMN_COUNT; x++)    //We check each pixel, if it's supposed to be ON : Set corresponding LED ON, else turn it off
    {
    fill_rainbow(ColorBuffer[x], ROW_COUNT, x * ROW_COUNT * 5 + (UnixTimeStamp % 360), 5);
    for (uint8_t y = 0; y < ROW_COUNT; y++)
    {
      if (AbsolutePixelIsOn[y][x]) OutputBuffer[y][x] = ColorBuffer[y][x];
      else OutputBuffer[y][x] = CHSV(0,0,0);
    }
    }*/
  if (RunningFX == Fading)
  {
    if (fxTrigger)//In case it's the trig of fx, compute fx parameters
    {
      copyFrame(AbsolutePixelIsOn, PreviousFrame); //Store previous Hour frame (
      pixelStateUpdate(timeMaskUpdate(Hour, Minute)); //Update Output Buffer

      for (uint8_t x = 0; x < COLUMN_COUNT; x++)    //run through all pixel
      {
        for (uint8_t y = 0; y < ROW_COUNT; y++)
        {
          uint16_t y2 = AbsolutePixelIsOn[y][x] * 255;
          uint16_t y1 = PreviousFrame[y][x] * 255;
          LinearExtrapolation[y][x][0] = (float)(y2 - y1) / (float)FadingDuration;
          LinearExtrapolation[y][x][1] =  (float)y1;
        }
      }
      LocalTimer = 0;
      fxTrigger = false;
      fxRunning = true;
    }
    if (fxRunning)
    {
      for (uint8_t x = 0; x < COLUMN_COUNT; x++)    //run through all pixel
      {
        for (uint8_t y = 0; y < ROW_COUNT; y++)
        {
          OutputBuffer[y][x] = CHSV(NextHue, 255, (uint8_t)(LinearExtrapolation[y][x][0] * LocalTimer + LinearExtrapolation[y][x][1]));
        }
      }
      if (LocalTimer >= FadingDuration) fxRunning = false;
    }
  }
  else if (RunningFX == ColorChangeByBlack)
  {
    if (fxTrigger)//In case it's the trig of fx, compute fx parameters
    {
      copyFrame(AbsolutePixelIsOn, PreviousFrame); //Store previous Hour frame (
      pixelStateUpdate(timeMaskUpdate(Hour, Minute)); //Update Output Buffer

      for (uint8_t x = 0; x < COLUMN_COUNT; x++)    //run through all pixel
      {
        for (uint8_t y = 0; y < ROW_COUNT; y++)
        {
          if (PreviousFrame[y][x])
          {
            LinearExtrapolation[y][x][0] = -255.0 / ((float)FadingDuration);
            LinearExtrapolation[y][x][1] =  255.0;
          }
          else
          {
            LinearExtrapolation[y][x][0] = 0;
            LinearExtrapolation[y][x][1] = 0;
          }

          if (AbsolutePixelIsOn[y][x])
          {
            LinearExtrapolation[y][x][2] = 255.0 / ((float)FadingDuration);
            LinearExtrapolation[y][x][3] = 0;
          }
          else
          {
            LinearExtrapolation[y][x][2] = 0;
            LinearExtrapolation[y][x][3] = 0;
          }
        }
      }
      PreviousHue = NextHue;
      NextHue = random(0, 255);
      LocalTimer = 0;
      fxTrigger = false;
      fxRunning = true;
    }
    if (fxRunning)
    {
      if (LocalTimer < FadingDuration)
      {
        for (uint8_t x = 0; x < COLUMN_COUNT; x++)    //run through all pixel
        {
          for (uint8_t y = 0; y < ROW_COUNT; y++)
          {

            OutputBuffer[y][x] = CHSV(PreviousHue, 255, constrain((uint16_t)(LinearExtrapolation[y][x][0] * (float)LocalTimer + LinearExtrapolation[y][x][1]), 0, 255));
          }
        }
      }
      else
      {
        for (uint8_t x = 0; x < COLUMN_COUNT; x++)    //run through all pixel
        {
          for (uint8_t y = 0; y < ROW_COUNT; y++)
          {

            OutputBuffer[y][x] = CHSV(NextHue, 255, constrain((uint16_t)(LinearExtrapolation[y][x][2] * ((float)LocalTimer - ((float)FadingDuration) + LinearExtrapolation[y][x][3])), 0, 255));
          }
        }
      }
      if (LocalTimer >= 2 * FadingDuration)
      {
        for (uint8_t x = 0; x < COLUMN_COUNT; x++)    //run through all pixel
        {
          for (uint8_t y = 0; y < ROW_COUNT; y++)
          {
            OutputBuffer[y][x] = CHSV(NextHue, 255, 255 * (AbsolutePixelIsOn[y][x]));
          }
        }
        fxRunning = false;
      }
    }
  }
}

void MyQlock::writeOutput()
{
  if (false)
  {
    TimeChangeType TempChange = UpdateTime();
    if (fxTrigger)
    {
      switch (TempChange)
      {
        case HourChange: RunningFX = ColorChangeByBlack;
          break;
        case FiveMinutes: RunningFX = Fading;
          break;
        case MinuteChange: RunningFX = Fading;
          break;
        default: RunningFX = Fading;
          break;
      }
    }
    if (fxTrigger || fxRunning)
    {
      applyFX();
    }

    for (uint8_t x = 0; x < COLUMN_COUNT; x++)    //We check each pixel, if it's supposed to be ON : Set corresponding LED ON, else turn it off
    {
      for (uint8_t y = 0; y < ROW_COUNT; y++)
      {
        uint8_t LED_Number = Mapping[y][x];
        if (LED_Number < StripLenght) this->leds[LED_Number] = OutputBuffer[y][x];
      }
    }
  }
  else
  {
    snake.drawBoard(AlternativeBuffer);
    for (uint8_t x = 0; x < COLUMN_COUNT; x++)    //We check each pixel, if it's supposed to be ON : Set corresponding LED ON, else turn it off
    {
      for (uint8_t y = 0; y < ROW_COUNT; y++)
      {
        uint8_t LED_Number = Mapping[y][x];
        if (LED_Number < StripLenght) this->leds[LED_Number] = AlternativeBuffer[y][x];
      }
    }
  }
}



void MyQlock::copyFrame(uint8_t SourceFrame[][COLUMN_COUNT], uint8_t TargetFrame[][COLUMN_COUNT])
{
  for (uint8_t x = 0; x < COLUMN_COUNT; x++)    //We check each pixel, if it's supposed to be ON : Set corresponding LED ON, else turn it off
  {
    for (uint8_t y = 0; y < ROW_COUNT; y++)
    {
      TargetFrame[y][x] = SourceFrame[y][x];
    }
  }
}










