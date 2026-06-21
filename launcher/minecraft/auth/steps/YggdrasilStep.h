// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>
#include <QByteArray>

#include "minecraft/auth/AuthStep.h"
#include "net/Upload.h"
#include "net/NetJob.h"

class YggdrasilStep : public AuthStep {
    Q_OBJECT

   public:
    explicit YggdrasilStep(AccountData* data, QString authServer = QString());
    QString describe() override;
    void perform() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    Net::Upload::Ptr m_request;
    NetJob::Ptr m_task;
    QString m_authServer;
};
