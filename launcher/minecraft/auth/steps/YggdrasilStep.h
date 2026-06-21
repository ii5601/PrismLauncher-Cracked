// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "minecraft/auth/AuthStep.h"

class YggdrasilStep : public AuthStep {
    Q_OBJECT

   public:
    explicit YggdrasilStep(AccountData* data, QString authServer = QString());
    QString describe() override;
    void perform() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    QNetworkRequest* m_request = nullptr;
    QString m_authServer;
};
