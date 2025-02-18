#include "spectrum_visualizer.h"
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QtMath>

SpectrumVisualizer::SpectrumVisualizer(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_scale(2.5f)  // Increased from 1.0f for better peak visibility
    , m_offset(0.0f)
    , m_initialized(false)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
}

SpectrumVisualizer::~SpectrumVisualizer() = default;

void SpectrumVisualizer::initializeGL()
{
    initializeOpenGLFunctions();
    
    // Set a nicer background color
    glClearColor(0.16f, 0.16f, 0.18f, 1.0f);  // Slightly bluish dark
    
    // Initialize shaders with enhanced visuals
    const char *vertexShaderSource = R"(
        attribute vec3 vertex;
        attribute vec3 color;
        varying vec3 vert_color;
        varying float v_intensity;
        uniform mat4 matrix;
        void main() {
            gl_Position = matrix * vec4(vertex, 1.0);
            v_intensity = vertex.y;  // Pass height for intensity
            vert_color = color;
        }
    )";
    
    const char *fragmentShaderSource = R"(
        varying vec3 vert_color;
        varying float v_intensity;
        void main() {
            // Enhanced color calculation
            vec3 baseColor = vec3(0.4, 0.7, 1.0);  // Brighter blue
            vec3 finalColor = mix(baseColor * 0.3, baseColor, v_intensity);
            gl_FragColor = vec4(finalColor, 0.9);
        }
    )";
    
    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_program.link();
    
    // Set up vertex buffer
    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    
    m_initialized = true;
}

void SpectrumVisualizer::paintGL()
{
    if (!m_initialized)
        return;

    // Clear and prepare OpenGL state
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Render spectrum data using OpenGL
    if (!m_data.magnitudes.isEmpty()) {
        m_program.bind();
        
        // Set up transformation matrix (identity for now)
        QMatrix4x4 matrix;
        m_program.setUniformValue("matrix", matrix);
        
        // Set up vertex attributes
        m_vbo.bind();
        int vertexLocation = m_program.attributeLocation("vertex");
        m_program.enableAttributeArray(vertexLocation);
        m_program.setAttributeBuffer(vertexLocation, GL_FLOAT, 0, 3, sizeof(QVector3D));
        
        int colorLocation = m_program.attributeLocation("color");
        m_program.enableAttributeArray(colorLocation);
        m_program.setAttributeBuffer(colorLocation, GL_FLOAT, m_vertices.size() * sizeof(QVector3D), 3, sizeof(QVector3D));
        
        // Draw the spectrum
        glDrawArrays(GL_LINE_STRIP, 0, m_vertices.size());
        
        // Clean up
        m_program.disableAttributeArray(vertexLocation);
        m_program.disableAttributeArray(colorLocation);
        m_program.release();
    }
    
    // Overlay grid and labels using QPainter
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw subtle grid
    painter.setPen(QPen(QColor(70, 70, 80, 80), 1, Qt::SolidLine));
    
    const int width = this->width();
    const int height = this->height();
    
    // Vertical grid lines
    for (int x = 0; x <= width; x += width/10) {
        painter.drawLine(x, 0, x, height);
    }
    
    // Horizontal grid lines
    for (int y = 0; y <= height; y += height/8) {
        painter.drawLine(0, y, width, y);
    }
    
    // Draw labels last (on top)
    drawLabels(painter);
    
    painter.end();
}

void SpectrumVisualizer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    update();  // Request a repaint with new size
}

void SpectrumVisualizer::updateVertices()
{
    if (!m_initialized || m_data.magnitudes.isEmpty())
        return;
        
    const int numPoints = m_data.magnitudes.size();
    m_vertices.resize(numPoints);
    m_colors.resize(numPoints);
    
    // Convert FFT data to vertices with enhanced scaling
    for (int i = 0; i < numPoints; ++i) {
        float x = (float)i / numPoints * 2.0f - 1.0f;
        float y = m_data.magnitudes[i] * m_scale + m_offset;
        
        m_vertices[i] = QVector3D(x, y, 0.0f);
        
        // Enhanced color gradient
        float intensity = (y + 1.0f) / 2.0f;
        m_colors[i] = QVector3D(0.4f + intensity * 0.6f,  // More blue
                               0.7f + intensity * 0.3f,  // More vibrant
                               1.0f);                    // Full blue base
    }
    
    // Update VBO efficiently
    m_vbo.bind();
    m_vbo.allocate(numPoints * sizeof(QVector3D) * 2);
    m_vbo.write(0, m_vertices.constData(), numPoints * sizeof(QVector3D));
    m_vbo.write(numPoints * sizeof(QVector3D), m_colors.constData(), 
                numPoints * sizeof(QVector3D));
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