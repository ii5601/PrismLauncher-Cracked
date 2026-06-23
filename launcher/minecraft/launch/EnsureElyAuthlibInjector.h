// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

#include <launch/LaunchStep.h>

#include "minecraft/auth/AuthSession.h"

class MinecraftInstance;

class EnsureElyAuthlibInjector : public LaunchStep {
    Q_OBJECT

   public:
    explicit EnsureElyAuthlibInjector(LaunchTask* parent, MinecraftInstance* instance, AuthSessionPtr session);
    ~EnsureElyAuthlibInjector() override = default;

    void executeTask() override;
    bool abort() override;
    bool canAbort() const override;

   private:
    void onDownloadFinished();
    bool isElyAccount() const;
    bool alreadyInstalled() const;
    void cleanupTempFile();

   private:
    MinecraftInstance* m_instance;
    AuthSessionPtr m_session;
    Net::Download::Ptr m_download;
    NetJob::Ptr m_task;
    QString m_downloadPath;
};