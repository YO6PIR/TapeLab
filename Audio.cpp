//========= Audio Engine v2.0 ============

#include "Audio.h"

#include "stm32f4xx_hal.h"

Audio audio;
ADC_HandleTypeDef hadc1;
static DMA_HandleTypeDef hdmaAdc1;
static TIM_HandleTypeDef htim3;
static int16_t fftSamples[FFTAnalyzer::SAMPLES];

namespace
{
    volatile bool dmaCaptureComplete = false;
    volatile bool dmaCaptureError = false;
    volatile bool dmaCaptureActive = false;

    uint32_t timer3ClockHz()
    {
        RCC_ClkInitTypeDef clocks = {};
        uint32_t flashLatency = 0;
        HAL_RCC_GetClockConfig(&clocks, &flashLatency);

        uint32_t clockHz = HAL_RCC_GetPCLK1Freq();
        if (clocks.APB1CLKDivider != RCC_HCLK_DIV1)
            clockHz *= 2;

        return clockHz;
    }
}

bool Audio::begin()
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    pinMode(PA0, INPUT_ANALOG);
    pinMode(PA1, INPUT_ANALOG);

    hdmaAdc1.Instance = DMA2_Stream0;
    hdmaAdc1.Init.Channel = DMA_CHANNEL_0;
    hdmaAdc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdmaAdc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdmaAdc1.Init.MemInc = DMA_MINC_ENABLE;
    hdmaAdc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdmaAdc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdmaAdc1.Init.Mode = DMA_NORMAL;
    hdmaAdc1.Init.Priority = DMA_PRIORITY_HIGH;
    hdmaAdc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdmaAdc1) != HAL_OK)
        return false;

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdmaAdc1);

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    return configureAdc(false);
}

bool Audio::configureAdc(bool hardwareTriggered)
{
    hadc1.Instance = ADC1;
    // APB2 is 84 MHz on the selected board. DIV4 keeps ADC_CLK at 21 MHz,
    // safely below the STM32F401 36 MHz limit.
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge =
        hardwareTriggered
            ? ADC_EXTERNALTRIGCONVEDGE_RISING
            : ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv =
        hardwareTriggered
            ? ADC_EXTERNALTRIGCONV_T3_TRGO
            : ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests =
        hardwareTriggered ? ENABLE : DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    return HAL_ADC_Init(&hadc1) == HAL_OK;
}

bool Audio::configureCaptureChannel(bool rightChannel)
{
    ADC_ChannelConfTypeDef sConfig = {};
    sConfig.Channel = rightChannel ? ADC_CHANNEL_1 : ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;

    return HAL_ADC_ConfigChannel(&hadc1, &sConfig) == HAL_OK;
}

bool Audio::configureCaptureTimer(uint32_t sampleRateHz)
{
    if (sampleRateHz == 0)
        return false;

    const uint32_t timerClock = timer3ClockHz();
    const uint64_t ticksPerSample =
        (static_cast<uint64_t>(timerClock) + sampleRateHz / 2) /
        sampleRateHz;

    if (ticksPerSample == 0 || ticksPerSample > 0x100000000ULL)
        return false;

    uint32_t prescaler =
        static_cast<uint32_t>((ticksPerSample - 1) / 65536ULL);
    if (prescaler > 0xFFFF)
        return false;

    uint32_t periodTicks =
        static_cast<uint32_t>(
            (static_cast<uint64_t>(timerClock) +
             static_cast<uint64_t>(sampleRateHz) * (prescaler + 1) / 2) /
            (static_cast<uint64_t>(sampleRateHz) * (prescaler + 1)));

    if (periodTicks == 0)
        periodTicks = 1;
    if (periodTicks > 65536)
        periodTicks = 65536;

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = prescaler;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = periodTicks - 1;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
        return false;

    TIM_MasterConfigTypeDef master = {};
    master.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    return HAL_TIMEx_MasterConfigSynchronization(&htim3, &master) == HAL_OK;
}

bool Audio::captureLeftBlock(
    int16_t *buffer,
    uint16_t sampleCount,
    uint32_t sampleRateHz)
{
    return captureBlock(buffer, sampleCount, sampleRateHz, false);
}

bool Audio::captureBlock(
    int16_t *buffer,
    uint16_t sampleCount,
    uint32_t sampleRateHz,
    bool rightChannel)
{
    if (buffer == nullptr || sampleCount == 0 || sampleRateHz == 0)
        return false;

    stopCapture();

    if (!configureAdc(true) ||
        !configureCaptureChannel(rightChannel) ||
        !configureCaptureTimer(sampleRateHz))
    {
        configureAdc(false);
        return false;
    }

    dmaCaptureComplete = false;
    dmaCaptureError = false;
    dmaCaptureActive = true;

    if (HAL_ADC_Start_DMA(
            &hadc1,
            reinterpret_cast<uint32_t *>(buffer),
            sampleCount) != HAL_OK)
    {
        stopCapture();
        return false;
    }

    __HAL_TIM_SET_COUNTER(&htim3, 0);
    if (HAL_TIM_Base_Start(&htim3) != HAL_OK)
    {
        stopCapture();
        return false;
    }

    const uint32_t expectedMs =
        (static_cast<uint32_t>(sampleCount) * 1000UL + sampleRateHz - 1) /
        sampleRateHz;
    const uint32_t timeoutMs = expectedMs + 20;
    const uint32_t startedMs = millis();

    while (!dmaCaptureComplete && !dmaCaptureError)
    {
        if (millis() - startedMs > timeoutMs)
        {
            dmaCaptureError = true;
            break;
        }
        yield();
    }

    const bool succeeded = dmaCaptureComplete && !dmaCaptureError;
    stopCapture();

    if (!succeeded)
        return false;

    for (uint16_t i = 0; i < sampleCount; ++i)
        buffer[i] = static_cast<int16_t>(
            buffer[i] - (rightChannel ? biasR : biasL));

    return true;
}

void Audio::stopCapture()
{
    if (dmaCaptureActive && htim3.Instance == TIM3)
        HAL_TIM_Base_Stop(&htim3);

    if (dmaCaptureActive &&
        hadc1.Instance == ADC1 &&
        hadc1.DMA_Handle != nullptr)
        HAL_ADC_Stop_DMA(&hadc1);

    dmaCaptureActive = false;

    // All other TapeLab tools retain their existing software-triggered reads.
    if (hadc1.Instance == ADC1)
        configureAdc(false);
}

bool Audio::captureActive() const
{
    return dmaCaptureActive;
}

uint16_t Audio::readLeft()
{
    ADC_ChannelConfTypeDef sConfig = {};
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;

    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1);

    return HAL_ADC_GetValue(&hadc1);
}

uint16_t Audio::readRight()
{
    ADC_ChannelConfTypeDef sConfig = {};
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;

    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1);

    return HAL_ADC_GetValue(&hadc1);
}

bool Audio::calibrate()
{
    constexpr uint16_t SAMPLE_COUNT = 256;

    constexpr uint16_t ADC_ZERO_MIN = 1500;
    constexpr uint16_t ADC_ZERO_MAX = 2500;

    uint32_t sumL = 0;
    uint32_t sumR = 0;

    for (uint16_t i = 0; i < SAMPLE_COUNT; ++i)
    {
        sumL += readLeft();
        sumR += readRight();
    }

    const uint16_t measuredBiasL =
        static_cast<uint16_t>(
            sumL / SAMPLE_COUNT);

    const uint16_t measuredBiasR =
        static_cast<uint16_t>(
            sumR / SAMPLE_COUNT);

    const bool leftValid =
        measuredBiasL >= ADC_ZERO_MIN &&
        measuredBiasL <= ADC_ZERO_MAX;

    const bool rightValid =
        measuredBiasR >= ADC_ZERO_MIN &&
        measuredBiasR <= ADC_ZERO_MAX;

    if (!leftValid || !rightValid)
    {

        return false;
    }

    biasL = static_cast<int16_t>(measuredBiasL);
    biasR = static_cast<int16_t>(measuredBiasR);

    return true;
}

int16_t Audio::readLeftCentered()
{
    return (int16_t)readLeft() - biasL;
}

int16_t Audio::readRightCentered()
{
    return (int16_t)readRight() - biasR;
}

void Audio::update()
{
    calculateFFT();
}

uint8_t Audio::getBand(uint8_t index)
{
    return (index < FFT_BANDS) ? spectrumBands[index] : 0;
}

void Audio::calculateFFT()
{
    // Acquisition is hardware-timed; FFTAnalyzer still owns unchanged DSP.
    if (!captureLeftBlock(
            fftSamples,
            FFTAnalyzer::SAMPLES,
            FFTAnalyzer::SAMPLE_RATE_HZ))
        return;

    fftAnalyzer.calculate(fftSamples, spectrumBands);
}

bool Audio::calculateFFTChannel(bool rightChannel, bool &clipped)
{
    clipped = false;
    if (!captureBlock(
            fftSamples,
            FFTAnalyzer::SAMPLES,
            FFTAnalyzer::SAMPLE_RATE_HZ,
            rightChannel))
        return false;

    for (uint16_t i = 0; i < FFTAnalyzer::SAMPLES; ++i)
    {
        if (abs(fftSamples[i]) >= 2000)
        {
            clipped = true;
            break;
        }
    }

    fftAnalyzer.calculate(fftSamples, spectrumBands);
    return true;
}

extern "C" void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdmaAdc1);
}

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &hadc1)
    {
        // Stop further trigger events immediately; cleanup runs in thread mode.
        __HAL_TIM_DISABLE(&htim3);
        dmaCaptureComplete = true;
    }
}

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &hadc1)
    {
        __HAL_TIM_DISABLE(&htim3);
        dmaCaptureError = true;
    }
}
