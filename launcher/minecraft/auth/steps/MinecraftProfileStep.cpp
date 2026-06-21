#include "MinecraftProfileStep.h"

#include <QNetworkRequest>

#include "Application.h"
#include "minecraft/auth/Parsers.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"

MinecraftProfileStep::MinecraftProfileStep(AccountData* data) : AuthStep(data) {}

QString MinecraftProfileStep::describe()
{
    return tr("Fetching the Minecraft profile.");
}

void MinecraftProfileStep::perform()
{
    QUrl url("https://api.ely.by/minecraft/profile");
    QString accessToken = m_data->yggdrasilToken.token;
    // Retrieve clientToken from the extra map (saved during YggdrasilStep)
    QString clientToken = m_data->yggdrasilToken.extra.value("clientToken").toString();
    // Build headers list manually to avoid arg() issues
    QList<Net::HeaderPair> headers;
    headers << Net::HeaderPair("Content-Type", "application/json");
    headers << Net::HeaderPair("Accept", "application/json");
    headers << Net::HeaderPair("Authorization", QString("Bearer %1").arg(accessToken).toUtf8());
    headers << Net::HeaderPair("clientToken", clientToken.toUtf8());

    auto [request, response] = Net::Download::makeByteArray(url);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("MinecraftProfileStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
}

void MinecraftProfileStep::onRequestDone(QByteArray* response)
{
    if (m_request->error() == QNetworkReply::ContentNotFoundError) {
        // NOTE: Succeed even if we do not have a profile. This is a valid account state.
        m_data->minecraftProfile = MinecraftProfile();
        emit finished(AccountTaskState::STATE_WORKING, tr("Account has no Minecraft profile."));
        return;
    }
    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Error getting profile:";
        qWarning() << " HTTP Status       :" << m_request->replyStatusCode();
        qWarning() << " Internal error no.:" << m_request->error();
        qWarning() << " Error string      :" << m_request->errorString();

        qWarning() << " Response:";
        qWarning() << QString::fromUtf8(*response);

        if (Net::isApplicationError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT,
                          tr("Minecraft Java profile acquisition failed: %1").arg(m_request->errorString()));
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE,
                          tr("Minecraft Java profile acquisition failed: %1").arg(m_request->errorString()));
        }
        return;
    }
    if (!Parsers::parseMinecraftProfile(*response, m_data->minecraftProfile)) {
        m_data->minecraftProfile = MinecraftProfile();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Minecraft Java profile response could not be parsed"));
        return;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Got Minecraft profile"));
}
