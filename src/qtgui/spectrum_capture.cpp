#include "spectrum_capture.h"
#include "../applications/gqrx/receiver.h"
#include <QDebug>
#include <QMetaType>
#include <chrono>

SpectrumCapture::SpectrumCapture(receiver *rx, QObject *parent)
    : QObject(parent)
    , m_capturing(false)
    , m_rx(rx)
{
    // Register types for use in signals/slots
    qRegisterMetaType<CaptureRange>("SpectrumCapture::CaptureRange");
    qRegisterMetaType<CaptureResult>("SpectrumCapture::CaptureResult");

    if (!m_rx) {
        qDebug() << "SpectrumCapture: Warning - Null receiver pointer";
    }
}

SpectrumCapture::~SpectrumCapture()
{
    stop();
}

SpectrumCapture::CaptureResult SpectrumCapture::captureRange(const CaptureRange& range)
{
    CaptureResult result;
    result.range = range;
    result.success = false;

    // Basic validation
    if (!m_rx) {
        result.error_message = "No receiver available";
        emit captureError(result.error_message);
        return result;
    }

    if (m_capturing) {
        result.error_message = "Capture already in progress";
        emit captureError(result.error_message);
        return result;
    }

    // Validate range parameters
    if (!validateRange(range, result.error_message)) {
        emit captureError(result.error_message);
        return result;
    }

    // Start capture process
    m_capturing = true;
    m_current_range = range;
    emit captureStarted(range);

    // Prepare capture parameters
    if (!prepareCaptureParameters(range)) {
        result.error_message = "Failed to prepare capture parameters";
        m_capturing = false;
        emit captureError(result.error_message);
        return result;
    }

    // Extract FFT data
    try {
        result.fft_data = extractFftData();
        result.success = true;
        result.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count() / 1000.0;
    }
    catch (const std::exception& e) {
        result.error_message = std::string("FFT data extraction failed: ") + e.what();
        result.success = false;
    }

    m_capturing = false;
    emit captureComplete(result);
    return result;
}

void SpectrumCapture::stop()
{
    if (m_capturing) {
        m_capturing = false;
        emit captureError("Capture stopped by user");
    }
}

double SpectrumCapture::getCurrentCenterFreq() const
{
    return m_rx ? m_rx->get_rf_freq() : 0.0;
}

double SpectrumCapture::getCurrentSampleRate() const
{
    return m_rx ? m_rx->get_input_rate() : 0.0;
}

int SpectrumCapture::getCurrentFftSize() const
{
    return m_rx ? m_rx->iq_fft_size() : 0;
}

bool SpectrumCapture::validateRange(const CaptureRange& range, std::string& error_msg)
{
    if (!range.isValid()) {
        error_msg = "Invalid range parameters";
        return false;
    }

    // Check if range is within receiver capabilities
    double current_sample_rate = getCurrentSampleRate();
    if (range.sample_rate > current_sample_rate) {
        error_msg = "Requested sample rate exceeds receiver capabilities";
        return false;
    }

    // Check if FFT size is valid (power of 2)
    if ((range.fft_size & (range.fft_size - 1)) != 0) {
        error_msg = "FFT size must be a power of 2";
        return false;
    }

    return true;
}

bool SpectrumCapture::prepareCaptureParameters(const CaptureRange& range)
{
    // For now, just verify we can access the FFT data
    if (!m_rx) {
        qDebug() << "Null receiver in prepareCaptureParameters";
        return false;
    }

    // Allocate buffer if needed
    unsigned int fft_size = m_rx->iq_fft_size();
    if (fft_size == 0) {
        qDebug() << "Invalid FFT size";
        return false;
    }

    m_fft_buffer.resize(fft_size);
    
    // Test FFT data access with proper error handling
    try {
        if (m_rx->get_iq_fft_data(m_fft_buffer.data()) < 0) {
            qDebug() << "Failed to get FFT data";
            return false;
        }
    }
    catch (const std::exception& e) {
        qDebug() << "Exception getting FFT data:" << e.what();
        return false;
    }
    catch (...) {
        qDebug() << "Unknown exception getting FFT data";
        return false;
    }
    
    emit progressUpdate(50);  // Placeholder progress update
    return true;
}

std::vector<float> SpectrumCapture::extractFftData()
{
    // Phase 1A: Just get current FFT data
    // This will be expanded to handle specific ranges in later phases
    unsigned int fft_size = getCurrentFftSize();
    if (fft_size == 0) {
        throw std::runtime_error("Invalid FFT size");
    }

    std::vector<float> data(fft_size);
    if (!m_rx || m_rx->get_iq_fft_data(data.data()) < 0) {
        throw std::runtime_error("Failed to get FFT data from receiver");
    }
    
    emit progressUpdate(100);
    return data;
} 