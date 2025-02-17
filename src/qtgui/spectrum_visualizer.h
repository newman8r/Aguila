#ifndef SPECTRUM_VISUALIZER_H
#define SPECTRUM_VISUALIZER_H

#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QVector>
#include <QVector3D>
#include <QPainter>
#include <QTimer>
#include <memory>

#include "spectrum_capture.h"

class SpectrumVisualizer : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit SpectrumVisualizer(QWidget *parent = nullptr);
    ~SpectrumVisualizer() override;

    void updateData(const std::vector<float>& fft_data, 
                   double center_freq,
                   double bandwidth,
                   double sample_rate);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    struct VisualizationData {
        QVector<float> magnitudes;
        double center_freq;
        double bandwidth;
        double sample_rate;
        uint64_t timestamp;
    };

    // Visualization state
    VisualizationData m_data;
    QVector<QVector3D> m_vertices;
    QVector<QVector3D> m_colors;
    
    // OpenGL state
    QOpenGLBuffer m_vbo;
    QOpenGLShaderProgram m_program;
    
    // View parameters
    float m_scale;
    float m_offset;
    bool m_initialized;

    // Helper functions
    void initializeShaders();
    void updateVertices();
    void drawSpectrum(QPainter& painter);
    void drawGrid(QPainter& painter);
    void drawLabels(QPainter& painter);
};

#endif // SPECTRUM_VISUALIZER_H 