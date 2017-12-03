#include "config.h"

#include "statistics.h"

#ifdef STATISTICS

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "tick.h"
#include "text.h"
#include "spi.h"
#include "util.h"

volatile uint64_t timeWastedPollingGPU = 0;
int statsSpiBusSpeed = 0;
int statsCpuFrequency = 0;
double statsCpuTemperature = 0;
double spiThreadUtilizationRate;
double spiBusDataRate;
int statsGpuPollingWasted = 0;
uint64_t statsBytesTransferred = 0;

int frameSkipTimeHistorySize = 0;
uint64_t frameSkipTimeHistory[FRAME_HISTORY_MAX_SIZE] = {};

char fpsText[32] = {};
char spiUsagePercentageText[32] = {};
char spiBusDataRateText[32] = {};
uint16_t spiUsageColor = 0, fpsColor = 0;
char statsFrameSkipText[32] = {};
char spiSpeedText[32] = {};
char cpuTemperatureText[32] = {};
uint16_t cpuTemperatureColor = 0;
char gpuPollingWastedText[32] = {};
uint16_t gpuPollingWastedColor = 0;

uint64_t statsLastPrint = 0;

void PollHardwareInfo()
{
  static uint64_t lastPollTime = tick();
  uint64_t now = tick();
  if (now - lastPollTime > 500000)
  {
    // SPI bus speed
    FILE *handle = fopen("/sys/kernel/debug/clk/aux_spi1/clk_rate", "r");
    char t[32] = {};
    if (handle)
    {
      fread(t, 1, sizeof(t)-1, handle);
      fclose(handle);
    }
    statsSpiBusSpeed = atoi(t)/1000000;

    // CPU temperature
    handle = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    char t2[32] = {};
    if (handle)
    {
      fread(t2, 1, sizeof(t2)-1, handle);
      fclose(handle);
    }
    statsCpuTemperature = atoi(t2)/1000.0;

    // Raspberry pi main CPU core speed
    handle = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    char t3[32] = {};
    if (handle)
    {
      fread(t3, 1, sizeof(t3)-1, handle);
      fclose(handle);
    }
    statsCpuFrequency = atoi(t3) / 1000;

    lastPollTime = now;
  }
}

void DrawStatisticsOverlay(uint16_t *framebuffer)
{
  DrawText(framebuffer, fpsText, 1, 1, fpsColor, 0);
  DrawText(framebuffer, statsFrameSkipText, strlen(fpsText)*6, 1, RGB565(31,0,0), 0);
  DrawText(framebuffer, spiUsagePercentageText, 49, 1, spiUsageColor, 0);
  DrawText(framebuffer, spiBusDataRateText, 79, 1, 0xFFFF, 0);
  DrawText(framebuffer, spiSpeedText, 145, 1, RGB565(31,14,20), 0);
  DrawText(framebuffer, cpuTemperatureText, 220, 1, cpuTemperatureColor, 0);
  DrawText(framebuffer, gpuPollingWastedText, 262, 1, gpuPollingWastedColor, 0);
}

void RefreshStatisticsOverlayText()
{
  uint64_t now = tick();
  uint64_t elapsed = now - statsLastPrint;
  if (elapsed < STATISTICS_REFRESH_INTERVAL) return;

#ifdef KERNEL_MODULE_CLIENT
  spiThreadUtilizationRate = 0; // TODO
  int spiRate = 0;
  strcpy(spiUsagePercentageText, "N/A");
#else
  uint64_t spiThreadIdleFor = __atomic_load_n(&spiThreadIdleUsecs, __ATOMIC_RELAXED);
  __sync_fetch_and_sub(&spiThreadIdleUsecs, spiThreadIdleFor);
  if (__atomic_load_n(&spiThreadSleeping, __ATOMIC_RELAXED)) spiThreadIdleFor += tick() - spiThreadSleepStartTime;
  spiThreadUtilizationRate = MIN(1.0, MAX(0.0, 1.0 - spiThreadIdleFor / (double)STATISTICS_REFRESH_INTERVAL));
  int spiRate = (int)MIN(100, (spiThreadUtilizationRate*100.0));
  sprintf(spiUsagePercentageText, "%d%%", spiRate);
#endif
  spiBusDataRate = (double)8.0 * statsBytesTransferred * 1000.0 / (elapsed / 1000.0);

  if (spiRate < 90) spiUsageColor = RGB565(0,63,0);
  else if (spiRate < 100) spiUsageColor = RGB565(31,63,0);
  else spiUsageColor = RGB565(31,0, 0);

  if (spiBusDataRate > 1000000) sprintf(spiBusDataRateText, "%.2fmbps", spiBusDataRate/1000000.0);
  else if (spiBusDataRate > 1000) sprintf(spiBusDataRateText, "%.2fkbps", spiBusDataRate/1000.0);
  else sprintf(spiBusDataRateText, "%.2fbps", spiBusDataRate);

  uint64_t wastedTime = __atomic_load_n(&timeWastedPollingGPU, __ATOMIC_RELAXED);
  __atomic_fetch_sub(&timeWastedPollingGPU, wastedTime, __ATOMIC_RELAXED);
  //const double gpuPollingWastedScalingFactor = 0.369; // A crude heuristic to scale time spent in useless polling to what Linux 'top' tool shows as % usage percentages
  statsGpuPollingWasted = (int)(wastedTime /** gpuPollingWastedScalingFactor*/ * 100 / (now - statsLastPrint));

  statsBytesTransferred = 0;

  if (statsSpiBusSpeed > 0 && statsCpuFrequency > 0) sprintf(spiSpeedText, "%d/%dMHz", statsCpuFrequency, statsSpiBusSpeed);
  else spiSpeedText[0] = '\0';

  if (statsCpuTemperature > 0)
  {
    sprintf(cpuTemperatureText, "%.1fc", statsCpuTemperature);
    if (statsCpuTemperature >= 80) cpuTemperatureColor = RGB565(31, 0, 0);
    else if (statsCpuTemperature >= 65) cpuTemperatureColor = RGB565(31, 63, 0);
    else cpuTemperatureColor = RGB565(0, 63, 0);
  }

  if (statsGpuPollingWasted > 0)
  {
    gpuPollingWastedColor = (statsGpuPollingWasted > 5) ? RGB565(31, 0, 0) : RGB565(31, 63, 0);
    sprintf(gpuPollingWastedText, "+%d%%", statsGpuPollingWasted);
  }
  else gpuPollingWastedText[0] = '\0';

  statsLastPrint = now;

  if (frameTimeHistorySize >= 3)
  {
    bool haveInterlacedFramesInHistory = false;
    for(int i = 0; i < frameTimeHistorySize; ++i)
      if (frameTimeHistory[i].interlaced)
      {
        haveInterlacedFramesInHistory = true;
        break;
      }
    int frames = frameTimeHistorySize;
    if (haveInterlacedFramesInHistory)
      for(int i = 0; i < frameTimeHistorySize; ++i)
        if (!frameTimeHistory[i].interlaced) ++frames; // Progressive frames count twice
    int fps = (0.5 + (frames - 1) * 1000000.0 / (frameTimeHistory[frameTimeHistorySize-1].time - frameTimeHistory[0].time));
#ifdef NO_INTERLACING
    sprintf(fpsText, "%d", fps);
    fpsColor = 0xFFFF;
#else
    sprintf(fpsText, "%d%c", fps, haveInterlacedFramesInHistory ? 'i' : 'p');
    fpsColor = haveInterlacedFramesInHistory ? RGB565(31, 30, 11) : 0xFFFF;
#endif
    if (frameSkipTimeHistorySize > 0) sprintf(statsFrameSkipText, "-%d", frameSkipTimeHistorySize);
    else statsFrameSkipText[0] = '\0';
  }
  else
  {
    strcpy(fpsText, "-");
    statsFrameSkipText[0] = '\0';
    fpsColor = 0xFFFF;
  }
}
#else
void PollHardwareInfo() {}
void RefreshStatisticsOverlayText() {}
void DrawStatisticsOverlay(uint16_t *) {}
#endif // ~STATISTICS
