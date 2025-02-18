#ifndef WATERFALL_DISPLAY_H
#define WATERFALL_DISPLAY_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QVector>
#include <QVector3D>
#include <deque>
#include <QMutex>

class WaterfallDisplay : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit WaterfallDisplay(QWidget *parent = nullptr);
    ~WaterfallDisplay() override;

    // Update with new FFT data
    void updateData(const std::vector<float>& fft_data,
                   double center_freq,
                   double bandwidth,
                   double sample_rate);

    // Configuration
    void setTimeSpan(float seconds);
    void setColorMap(const QString& name);
    void setMinMax(float min_db, float max_db);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    struct WaterfallData {
        std::deque<std::vector<float>> history;  // Circular buffer for waterfall history
        double center_freq;
        double bandwidth;
        double sample_rate;
        uint64_t last_update;
        size_t max_history;  // Maximum number of history lines to keep
    };

    // Visualization state
    WaterfallData m_data;
    QVector<QVector3D> m_vertices;
    QVector<QVector3D> m_colors;
    
    // OpenGL state
    QOpenGLBuffer m_vbo;
    QOpenGLShaderProgram m_program;
    
    // View parameters
    float m_min_db;
    float m_max_db;
    float m_time_span;
    bool m_initialized;
    QMutex m_mutex;  // Protect data updates

    // Color mapping
    QVector<QVector3D> m_colormap;
    void initializeColorMap();
    QVector3D getColorForValue(float value) const;

    // Helper functions
    void updateVertices();
    void updateTexture();
    void drawLabels(QPainter& painter);
};

#endif // WATERFALL_DISPLAY_H 