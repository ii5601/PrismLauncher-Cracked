// SPDX-License-Identifier: GPL-3.0-only

#include "EnsureElyAuthlibInjector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include "Application.h"
#include "FileSystem.h"
#include "MessageLevel.h"
#include "minecraft/PackProfile.h"
#include "minecraft/MinecraftInstance.h"
#include "net/Download.h"
#include "net/NetJob.h"

namespace {
constexpr const char* AUTHLIB_INJECTOR_URL = "https://github.com/yushijinhun/authlib-injector/releases/latest/download/authlib-injector.jar";
constexpr const char* AUTHLIB_INJECTOR_MARKER = "authlib-injector";
}  // namespace

EnsureElyAuthlibInjector::EnsureElyAuthlibInjector(LaunchTask* parent, MinecraftInstance* instance, AuthSessionPtr session)
    : LaunchStep(parent), m_instance(instance), m_session(std::move(session))
{
}

bool EnsureElyAuthlibInjector::isElyAccount() const
{
    return m_session && m_session->user_type == "ely";
}

bool EnsureElyAuthlibInjector::alreadyInstalled() const
{
    const auto profile = m_instance->getPackProfile()->getProfile();
    if (!profile) {
        return false;
    }

    const auto runtimeContext = m_instance->runtimeContext();
    for (const auto& agent : profile->getAgents()) {
        if (!agent.library) {
            continue;
        }

        const auto displayName = agent.library->displayName(runtimeContext);
        const auto fileName = agent.library->filename(runtimeContext);
        if (displayName.contains(AUTHLIB_INJECTOR_MARKER, Qt::CaseInsensitive) || fileName.contains(AUTHLIB_INJECTOR_MARKER, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

void EnsureElyAuthlibInjector::cleanupTempFile()
{
    if (!m_downloadPath.isEmpty()) {
        QFile::remove(m_downloadPath);
        m_downloadPath.clear();
    }
}

void EnsureElyAuthlibInjector::executeTask()
{
    if (!isElyAccount()) {
        emitSucceeded();
        return;
    }

    if (alreadyInstalled()) {
        emit logLine(tr("authlib-injector is already installed for this Ely.by instance."), MessageLevel::Launcher);
        emitSucceeded();
        return;
    }

    emit logLine(tr("Downloading authlib-injector for Ely.by skins support..."), MessageLevel::Launcher);

    m_downloadPath = FS::PathCombine(QDir::tempPath(), QString("authlib-injector-%1.jar").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    m_download = Net::Download::makeFile(QUrl(AUTHLIB_INJECTOR_URL), m_downloadPath);
    m_download->enableAutoRetry(true);

    m_task.reset(new NetJob("ElyAuthlibInjector", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_download);

    connect(m_task.get(), &Task::finished, this, [this] { onDownloadFinished(); });

    m_task->start();
}

void EnsureElyAuthlibInjector::onDownloadFinished()
{
    if (m_download->error() != QNetworkReply::NoError) {
        emit logLine(tr("Could not download authlib-injector for Ely.by. Game will still start without automatic skin injection: %1")
                         .arg(m_download->errorString()),
                     MessageLevel::Warning);
        cleanupTempFile();
        emitSucceeded();
        return;
    }

    auto profile = m_instance->getPackProfile();
    if (!profile->installAgents(QStringList{ m_downloadPath })) {
        emit logLine(tr("Downloaded authlib-injector, but failed to install it as a Java agent. Game will still start."), MessageLevel::Warning);
        cleanupTempFile();
        emitSucceeded();
        return;
    }

    emit logLine(tr("Installed authlib-injector for Ely.by accounts."), MessageLevel::Launcher);
    cleanupTempFile();
    emitSucceeded();
}

bool EnsureElyAuthlibInjector::abort()
{
    if (m_task && m_task->canAbort()) {
        return m_task->abort();
    }
    return LaunchStep::abort();
}

bool EnsureElyAuthlibInjector::canAbort() const
{
    if (m_task) {
        return m_task->canAbort();
    }
    return true;
}