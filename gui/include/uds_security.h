#pragma once
#include <QWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QGroupBox>
#include <vector>
#include <cstdint>
#include "uds_transport.h"

namespace socketspy::gui {

// SecurityAccess (UDS service 0x27) widget.
// Owns no UdsTransport — receives a shared pointer set by UdsPanel.
class UdsSecurityWidget : public QWidget {
    Q_OBJECT

public:
    explicit UdsSecurityWidget(QWidget* parent = nullptr);

    // Called by UdsPanel after the transport is configured.
    void setTransport(UdsTransport* transport);

    // Called by UdsPanel so this widget can react to responses.
    void handleResponse(const std::vector<uint8_t>& data);

    // Called by UdsPanel when a transport error occurs.
    void handleError(const QString& message);

    // Returns true if a security request is currently in flight.
    bool isPending() const { return m_pending; }

signals:
    // Emitted when a 0x27 request must be sent; UdsPanel will forward it.
    void requestReady(std::vector<uint8_t> data, QString label);

private slots:
    void onRequestSeed();
    void onApplyKey();

private:
    void setupUi();
    void setSecurityStatus(const QString& text, const QString& color);
    void setBusy(bool busy);

    QSpinBox*    m_levelSpin{nullptr};
    QPushButton* m_seedBtn{nullptr};
    QLabel*      m_seedLabel{nullptr};
    QLineEdit*   m_keyEdit{nullptr};
    QPushButton* m_applyKeyBtn{nullptr};
    QLabel*      m_securityStatus{nullptr};

    UdsTransport* m_transport{nullptr};
    bool          m_pending{false};
    QString       m_pendingOp;   // "seed" or "key"
    int           m_lastLevel{1};
};

} // namespace socketspy::gui
