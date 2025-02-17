#ifndef SPECTRUM_CAPTURE_H
#define SPECTRUM_CAPTURE_H

#include <vector>
#include <string>
#include <memory>
#include <QObject>

class receiver;  // Forward declaration of GQRX receiver class

/**
 * @brief The SpectrumCapture class handles capturing spectrum data for specific frequency ranges
 * 
 * This class provides functionality to capture FFT data from the receiver
 * for specified frequency ranges, independent of the UI display.
 */
class SpectrumCapture : public QObject
{
    Q_OBJECT

public:
    struct CaptureRange {
        double start_freq;  // Hz
        double end_freq;    // Hz
        int fft_size;
        double sample_rate;
        
        // Validation helper
        bool isValid() const {
            return start_freq < end_freq && 
                   fft_size > 0 && 
                   sample_rate > 0;
        }
    };

    struct CaptureResult {
        bool success;
        std::vector<float> fft_data;
        CaptureRange range;
        std::string error_message;
        double timestamp;  // Unix timestamp of capture
    };

    explicit SpectrumCapture(receiver *rx, QObject *parent = nullptr);
    ~SpectrumCapture();

    // Core capture interface
    CaptureResult captureRange(const CaptureRange& range);
    bool isCapturing() const { return m_capturing; }
    void stop();

    // Getters for current state
    double getCurrentCenterFreq() const;
    double getCurrentSampleRate() const;
    int getCurrentFftSize() const;

signals:
    void captureStarted(const CaptureRange& range);
    void captureComplete(const CaptureResult& result);
    void captureError(const std::string& error_message);
    void progressUpdate(int percent);

private:
    bool validateRange(const CaptureRange& range, std::string& error_msg);
    bool prepareCaptureParameters(const CaptureRange& range);
    std::vector<float> extractFftData();

    bool m_capturing;
    receiver *m_rx;  // Non-owning pointer to receiver
    
    // Current capture state
    CaptureRange m_current_range;
    std::vector<float> m_fft_buffer;
};

// Register types with Qt's meta-object system
Q_DECLARE_METATYPE(SpectrumCapture::CaptureRange)
Q_DECLARE_METATYPE(SpectrumCapture::CaptureResult)

#endif // SPECTRUM_CAPTURE_H 