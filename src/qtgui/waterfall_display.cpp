#include "waterfall_display.h"
#include <QPainter>
#include <QDateTime>
#include <QtMath>

WaterfallDisplay::WaterfallDisplay(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_min_db(-120.0f)
    , m_max_db(-20.0f)
    , m_time_span(10.0f)  // 10 seconds default
    , m_initialized(false)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    
    // Initialize waterfall data
    m_data.max_history = 1024;  // Adjust based on performance needs
    m_data.last_update = QDateTime::currentMSecsSinceEpoch();
}

WaterfallDisplay::~WaterfallDisplay() = default;

void WaterfallDisplay::initializeGL()
{
    initializeOpenGLFunctions();
    
    // Set background color
    glClearColor(0.16f, 0.16f, 0.18f, 1.0f);
    
    // Initialize shaders for waterfall rendering
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
    
    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_program.link();
    
    // Set up vertex buffer
    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    
    // Initialize color map
    initializeColorMap();
    
    m_initialized = true;
}

void WaterfallDisplay::paintGL()
{
    if (!m_initialized)
        return;

    glClear(GL_COLOR_BUFFER_BIT);
    
    QMutexLocker locker(&m_mutex);
    
    if (m_data.history.empty())
        return;

    m_program.bind();
    
    // Set up transformation matrix
    QMatrix4x4 matrix;
    m_program.setUniformValue("matrix", matrix);
    
    // Draw waterfall data
    m_vbo.bind();
    int vertexLocation = m_program.attributeLocation("vertex");
    m_program.enableAttributeArray(vertexLocation);
    m_program.setAttributeBuffer(vertexLocation, GL_FLOAT, 0, 3, sizeof(QVector3D));
    
    int colorLocation = m_program.attributeLocation("color");
    m_program.enableAttributeArray(colorLocation);
    m_program.setAttributeBuffer(colorLocation, GL_FLOAT, m_vertices.size() * sizeof(QVector3D), 
                                3, sizeof(QVector3D));
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, m_vertices.size());
    
    m_program.disableAttributeArray(vertexLocation);
    m_program.disableAttributeArray(colorLocation);
    m_program.release();
    
    // Draw overlay (frequency/time labels)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    drawLabels(painter);
}

void WaterfallDisplay::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    updateVertices();
}

void WaterfallDisplay::updateData(const std::vector<float>& fft_data,
                                 double center_freq,
                                 double bandwidth,
                                 double sample_rate)
{
    QMutexLocker locker(&m_mutex);
    
    // Update parameters
    m_data.center_freq = center_freq;
    m_data.bandwidth = bandwidth;
    m_data.sample_rate = sample_rate;
    
    // Add new data to history
    m_data.history.push_front(fft_data);
    if (m_data.history.size() > m_data.max_history)
        m_data.history.pop_back();
    
    m_data.last_update = QDateTime::currentMSecsSinceEpoch();
    
    // Update visualization
    updateVertices();
    update();
}

void WaterfallDisplay::setTimeSpan(float seconds)
{
    QMutexLocker locker(&m_mutex);
    m_time_span = seconds;
    m_data.max_history = static_cast<size_t>(seconds * 60);  // Assuming 60 updates per second
    updateVertices();
}

void WaterfallDisplay::setColorMap(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    // TODO: Implement different color maps
    initializeColorMap();
    updateVertices();
}

void WaterfallDisplay::setMinMax(float min_db, float max_db)
{
    QMutexLocker locker(&m_mutex);
    m_min_db = min_db;
    m_max_db = max_db;
    updateVertices();
}

void WaterfallDisplay::initializeColorMap()
{
    // Initialize with a basic heat map (black -> blue -> red -> yellow -> white)
    m_colormap.clear();
    m_colormap.reserve(256);
    
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        QVector3D color;
        
        if (t < 0.25f) {
            // Black to Blue
            color = QVector3D(0, 0, t * 4);
        }
        else if (t < 0.5f) {
            // Blue to Red
            float s = (t - 0.25f) * 4;
            color = QVector3D(s, 0, 1.0f - s);
        }
        else if (t < 0.75f) {
            // Red to Yellow
            float s = (t - 0.5f) * 4;
            color = QVector3D(1.0f, s, 0);
        }
        else {
            // Yellow to White
            float s = (t - 0.75f) * 4;
            color = QVector3D(1.0f, 1.0f, s);
        }
        
        m_colormap.append(color);
    }
}

QVector3D WaterfallDisplay::getColorForValue(float value) const
{
    // Map value from dB range to [0,1]
    float t = (value - m_min_db) / (m_max_db - m_min_db);
    t = qBound(0.0f, t, 1.0f);
    
    // Map to colormap index
    int index = static_cast<int>(t * (m_colormap.size() - 1));
    return m_colormap[index];
}

void WaterfallDisplay::updateVertices()
{
    if (!m_initialized || m_data.history.empty())
        return;

    const int width = m_data.history.front().size();
    const int height = m_data.history.size();
    
    // Create vertex grid
    m_vertices.clear();
    m_colors.clear();
    m_vertices.reserve(width * height * 2);
    m_colors.reserve(width * height * 2);
    
    for (int y = 0; y < height; ++y) {
        const auto& row = m_data.history[y];
        float y_pos = 1.0f - (2.0f * y / height);
        
        for (int x = 0; x < width; ++x) {
            float x_pos = (2.0f * x / width) - 1.0f;
            float value = row[x];
            
            m_vertices.append(QVector3D(x_pos, y_pos, 0));
            m_colors.append(getColorForValue(value));
            
            if (y < height - 1) {
                float next_y_pos = 1.0f - (2.0f * (y + 1) / height);
                m_vertices.append(QVector3D(x_pos, next_y_pos, 0));
                m_colors.append(getColorForValue(m_data.history[y + 1][x]));
            }
        }
    }
    
    // Update VBO
    m_vbo.bind();
    m_vbo.allocate(m_vertices.size() * sizeof(QVector3D) * 2);
    m_vbo.write(0, m_vertices.constData(), m_vertices.size() * sizeof(QVector3D));
    m_vbo.write(m_vertices.size() * sizeof(QVector3D), m_colors.constData(),
                m_colors.size() * sizeof(QVector3D));
}

void WaterfallDisplay::drawLabels(QPainter& painter)
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
    
    // Time labels
    for (int i = 0; i <= 5; ++i) {
        float time = m_time_span * i / 5;
        int y = height * i / 5;
        
        QString label = QString::number(-time, 'f', 1) + " s";
        painter.drawText(5, y + 15, label);
    }
} 