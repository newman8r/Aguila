#include "spectrum_visualizer.h"
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QtMath>

SpectrumVisualizer::SpectrumVisualizer(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_scale(1.0f)
    , m_offset(0.0f)
    , m_initialized(false)
{
    // Set widget attributes for proper rendering
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    
    // Enable mouse tracking for future interaction
    setMouseTracking(true);
}

SpectrumVisualizer::~SpectrumVisualizer()
{
    // Clean up OpenGL resources
    if (m_initialized) {
        makeCurrent();
        m_vbo.destroy();
        doneCurrent();
    }
}

void SpectrumVisualizer::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    // Initialize shaders
    initializeShaders();
    
    // Set up vertex buffer
    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    
    m_initialized = true;
}

void SpectrumVisualizer::paintGL()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Clear background
    painter.fillRect(rect(), QColor(30, 30, 30));
    
    // Draw visualization elements
    drawGrid(painter);
    drawSpectrum(painter);
    drawLabels(painter);
}

void SpectrumVisualizer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void SpectrumVisualizer::updateData(const std::vector<float>& fft_data,
                                   double center_freq,
                                   double bandwidth,
                                   double sample_rate)
{
    // Update visualization data
    m_data.magnitudes = QVector<float>(fft_data.begin(), fft_data.end());
    m_data.center_freq = center_freq;
    m_data.bandwidth = bandwidth;
    m_data.sample_rate = sample_rate;
    m_data.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    // Update vertex data
    updateVertices();
    
    // Request redraw
    update();
}

void SpectrumVisualizer::initializeShaders()
{
    // Basic shader for 2D visualization
    const char *vertexShaderSource = R"(
        attribute vec3 vertex;
        attribute vec3 color;
        varying vec3 vert_color;
        uniform mat4 matrix;
        void main() {
            gl_Position = matrix * vec4(vertex, 1.0);
            vert_color = color;
        }
    )";
    
    const char *fragmentShaderSource = R"(
        varying vec3 vert_color;
        void main() {
            gl_FragColor = vec4(vert_color, 1.0);
        }
    )";
    
    if (!m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource))
        qDebug() << "Failed to compile vertex shader";
        
    if (!m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource))
        qDebug() << "Failed to compile fragment shader";
        
    if (!m_program.link())
        qDebug() << "Failed to link shader program";
}

void SpectrumVisualizer::updateVertices()
{
    if (!m_initialized || m_data.magnitudes.isEmpty())
        return;
        
    const int numPoints = m_data.magnitudes.size();
    m_vertices.resize(numPoints);
    m_colors.resize(numPoints);
    
    // Convert FFT data to vertices
    for (int i = 0; i < numPoints; ++i) {
        float x = (float)i / numPoints * 2.0f - 1.0f;
        float y = m_data.magnitudes[i] * m_scale + m_offset;
        
        m_vertices[i] = QVector3D(x, y, 0.0f);
        
        // Color based on magnitude
        float intensity = (y + 1.0f) / 2.0f;
        m_colors[i] = QVector3D(0.2f + intensity * 0.8f,
                               0.5f + intensity * 0.5f,
                               0.8f + intensity * 0.2f);
    }
    
    // Update VBO
    m_vbo.bind();
    m_vbo.allocate(numPoints * sizeof(QVector3D) * 2);
    m_vbo.write(0, m_vertices.constData(), numPoints * sizeof(QVector3D));
    m_vbo.write(numPoints * sizeof(QVector3D), m_colors.constData(), 
                numPoints * sizeof(QVector3D));
}

void SpectrumVisualizer::drawSpectrum(QPainter& painter)
{
    if (!m_initialized || m_data.magnitudes.isEmpty())
        return;
        
    const int width = this->width();
    const int height = this->height();
    
    // Set up path for spectrum line
    QPainterPath path;
    const int numPoints = m_data.magnitudes.size();
    
    for (int i = 0; i < numPoints; ++i) {
        float x = (float)i / numPoints * width;
        float y = height - (m_data.magnitudes[i] * m_scale + m_offset) * height;
        
        if (i == 0)
            path.moveTo(x, y);
        else
            path.lineTo(x, y);
    }
    
    // Draw spectrum
    painter.setPen(QPen(QColor(86, 156, 214), 1.5));
    painter.drawPath(path);
    
    // Fill area under curve
    QLinearGradient gradient(0, 0, 0, height);
    gradient.setColorAt(0, QColor(86, 156, 214, 100));
    gradient.setColorAt(1, QColor(86, 156, 214, 10));
    
    QPainterPath fillPath = path;
    fillPath.lineTo(width, height);
    fillPath.lineTo(0, height);
    fillPath.closeSubpath();
    
    painter.fillPath(fillPath, gradient);
}

void SpectrumVisualizer::drawGrid(QPainter& painter)
{
    const int width = this->width();
    const int height = this->height();
    
    // Draw grid lines
    painter.setPen(QPen(QColor(60, 60, 60), 1, Qt::SolidLine));
    
    // Vertical lines
    for (int x = 0; x <= width; x += width/10) {
        painter.drawLine(x, 0, x, height);
    }
    
    // Horizontal lines
    for (int y = 0; y <= height; y += height/8) {
        painter.drawLine(0, y, width, y);
    }
}

void SpectrumVisualizer::drawLabels(QPainter& painter)
{
    const int width = this->width();
    const int height = this->height();
    
    painter.setPen(Qt::white);
    painter.setFont(QFont("Monospace", 8));
    
    // Frequency labels
    double startFreq = m_data.center_freq - m_data.bandwidth/2;
    double endFreq = m_data.center_freq + m_data.bandwidth/2;
    
    for (int i = 0; i <= 10; ++i) {
        double freq = startFreq + (endFreq - startFreq) * i / 10;
        int x = width * i / 10;
        
        QString label = QString::number(freq/1e6, 'f', 3) + " MHz";
        painter.drawText(x - 20, height - 5, label);
    }
    
    // Power labels
    for (int i = 0; i <= 8; ++i) {
        int y = height * i / 8;
        double power = -120 + 120 * (8-i) / 8;
        
        QString label = QString::number(power, 'f', 0) + " dB";
        painter.drawText(5, y + 15, label);
    }
} 