// SPDX-License-Identifier: GPL-3.0-only
#include "YggdrasilStep.h"

#include <QNetworkReply>
#include <QJsonDocument>

#include "net/NetUtils.h"
#include "net/Upload.h"
#include "net/RawHeaderProxy.h"
#include "net/NetJob.h"
#include <QJsonObject>
#include <QDateTime>
#include "minecraft/auth/Parsers.h"
#include "Application.h"

YggdrasilStep::YggdrasilStep(AccountData* data, QString authServer) : AuthStep(data), m_authServer(authServer) {}

QString YggdrasilStep::describe()
{
    return tr("Logging in with Yggdrasil server");
}

void YggdrasilStep::perform()
{
    // Default to Mojang auth endpoint if none provided. Ely.by is Yggdrasil-compatible.
    QString authServer = m_authServer;
    if (authServer.isEmpty()) {
        authServer = m_data->yggdrasilToken.extra.value("authServer").toString();
    }
    QString endpoint = authServer.isEmpty() ? QString("https://authserver.mojang.com/authenticate") : authServer + QLatin1String("/authenticate");
    QUrl url(endpoint);

    // Expect username/password in m_data->yggdrasilToken.extra["username"/"password"] set by the dialog.
    QString username = m_data->yggdrasilToken.extra.value("username").toString();
    QString password = m_data->yggdrasilToken.extra.value("password").toString();

    if (username.isEmpty() || password.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Username or password is empty."));
        return;
    }

    QJsonObject payload;
    QJsonObject agent;
    agent["name"] = "Minecraft";
    agent["version"] = 1;
    payload["agent"] = agent;
    payload["username"] = username;
    payload["password"] = password;

    auto [request, response] = Net::Upload::makeByteArray(url, QJsonDocument(payload).toJson());
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(QList<Net::HeaderPair>{ { "Content-Type", "application/json" }, { "Accept", "application/json" } }));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("YggdrasilStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
}

void YggdrasilStep::onRequestDone(QByteArray* response)
{
    qCDebug(authCredentials()) << *response;
    if (m_request->error() != QNetworkReply::NoError) {
        if (Net::isApplicationError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to authenticate: %1").arg(m_request->errorString()));
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Failed to authenticate: %1").arg(m_request->errorString()));
        }
        return;
    }

    // Parse response: expect accessToken and selectedProfile
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(*response, &err);
    if (err.error != QJsonParseError::NoError) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse authentication response."));
        return;
    }
    auto obj = doc.object();
    QString accessToken = obj.value("accessToken").toString();
    auto selectedProfile = obj.value("selectedProfile").toObject();
    QString profileId = selectedProfile.value("id").toString();
    QString profileName = selectedProfile.value("name").toString();

    if (accessToken.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Authentication failed: no access token returned."));
        return;
    }

    m_data->yggdrasilToken.token = accessToken;
    m_data->yggdrasilToken.validity = Validity::Certain;
    m_data->yggdrasilToken.issueInstant = QDateTime::currentDateTimeUtc();
    // clear password for security
    m_data->yggdrasilToken.extra.remove("password");

    if (!profileId.isEmpty()) {
        m_data->minecraftProfile.id = profileId;
        m_data->minecraftProfile.name = profileName;
        m_data->minecraftProfile.validity = Validity::Certain;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Got Yggdrasil access token"));
}

    #include "YggdrasilStep.moc"
